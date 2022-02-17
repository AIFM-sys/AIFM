#pragma once

#include "device.hpp"

#include <optional>

namespace far_memory {

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE Prefetcher<InduceFn, InferFn, MappingFn>::Prefetcher(
    FarMemDevice *device, uint8_t *state, uint32_t object_data_size)
    : kPrefetchWinSize_(device->get_prefetch_win_size() / (object_data_size)),
      state_(state), object_data_size_(object_data_size) {
  for (auto &trace : traces_) {
    trace.counter = 0;
  }
  prefetch_threads_.emplace_back([&]() { prefetch_master_fn(); });
  for (uint32_t i = 0; i < kMaxNumPrefetchSlaveThreads; i++) {
    auto &status = slave_status_[i].data;
    status.task = nullptr;
    status.is_exited = false;
    wmb();
    prefetch_threads_.emplace_back([&, i]() { prefetch_slave_fn(i); });
  }
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE Prefetcher<InduceFn, InferFn, MappingFn>::~Prefetcher() {
  exit_ = true;
  wmb();
  while (!ACCESS_ONCE(master_exited)) {
    cv_prefetch_master_.Signal();
    thread_yield();
  }
  for (uint32_t i = 0; i < kMaxNumPrefetchSlaveThreads; i++) {
    auto &status = slave_status_[i].data;
    while (!ACCESS_ONCE(status.is_exited)) {
      status.cv.Signal();
      thread_yield();
    }
  }
  for (auto &thread : prefetch_threads_) {
    thread.Join();
  }
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void
Prefetcher<InduceFn, InferFn, MappingFn>::generate_prefetch_tasks() {
  InferFn inferer;
  MappingFn mapper;
  for (uint32_t i = 0; i < kGenTasksBurstSize; i++) {
    if (!num_objs_to_prefetch) {
      return;
    }
    num_objs_to_prefetch--;
    GenericUniquePtr *task = mapper(state_, next_prefetch_idx_);
    next_prefetch_idx_ = inferer(next_prefetch_idx_, pattern_);
    if (!task) {
      continue;
    }
    bool dispatched = false;
    std::optional<uint32_t> inactive_slave_id = std::nullopt;
    for (uint32_t i = 0; i < kMaxNumPrefetchSlaveThreads; i++) {
      auto &status = slave_status_[i].data;
      if (status.cv.HasWaiters()) {
        inactive_slave_id = i;
        continue;
      }
      if (ACCESS_ONCE(status.task) == nullptr) {
        ACCESS_ONCE(status.task) = task;
        dispatched = true;
        break;
      }
    }
    if (!dispatched) {
      if (likely(inactive_slave_id)) {
        auto &status = slave_status_[*inactive_slave_id].data;
        status.task = task;
        wmb();
        status.cv.Signal();
      } else {
        DerefScope scope;
        task->swap_in(nt_);
      }
    }
  }
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void
Prefetcher<InduceFn, InferFn, MappingFn>::prefetch_slave_fn(uint32_t tid) {
  auto &status = slave_status_[tid].data;
  GenericUniquePtr **task_ptr = &status.task;
  bool *is_exited = &status.is_exited;
  rt::CondVar *cv = &status.cv;
  cv->Wait();

  while (likely(!ACCESS_ONCE(exit_))) {
    if (likely(ACCESS_ONCE(*task_ptr))) {
      GenericUniquePtr *task = *task_ptr;
      ACCESS_ONCE(*task_ptr) = nullptr;
      DerefScope scope;
      task->swap_in(nt_);
    } else {
      auto start_us = microtime();
      while (ACCESS_ONCE(*task_ptr) == nullptr &&
             microtime() - start_us <= kMaxSlaveWaitUs) {
        cpu_relax();
      }
      if (unlikely(ACCESS_ONCE(*task_ptr) == nullptr)) {
        cv->Wait();
      }
    }
  }
  ACCESS_ONCE(*is_exited) = true;
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void
Prefetcher<InduceFn, InferFn, MappingFn>::prefetch_master_fn() {
  uint64_t local_counter = 0;
  InduceFn inducer;
  InferFn inferer;

  while (likely(!ACCESS_ONCE(exit_))) {
    auto [counter, idx, nt] = traces_[traces_head_];

    if (likely(local_counter < counter)) {
      local_counter = counter;
      traces_head_ = (traces_head_ + 1) % kIdxTracesSize;

      if (unlikely(idx == last_idx_)) {
        continue;
      }
      auto new_pattern = inducer(last_idx_, idx);
      if (pattern_ != new_pattern) {
        hit_times_ = num_objs_to_prefetch = 0;
      } else if (++hit_times_ >= kHitTimesThresh) {
        if (unlikely(hit_times_ == kHitTimesThresh)) {
          next_prefetch_idx_ = inferer(idx, pattern_);
          num_objs_to_prefetch = kPrefetchWinSize_;
        } else {
          num_objs_to_prefetch++;
        }
      }
      pattern_ = new_pattern;
      last_idx_ = idx;
      if (unlikely(nt_ != nt)) {
        // nt_ is shared by all slaves. Use the store instruction only when
        // neccesary to reduce cache traffic.
        nt_ = nt;
      }
    } else if (!num_objs_to_prefetch) {
      cv_prefetch_master_.Wait();
      continue;
    }
    generate_prefetch_tasks();
  }
  ACCESS_ONCE(master_exited) = true;
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void
Prefetcher<InduceFn, InferFn, MappingFn>::add_trace(bool nt, Index_t idx) {
  // add_trace() is at the call path of the frontend mutator thread.
  // The goal is to make it extremely short and fast, therefore not compromising
  // the mutator performance when prefetching is enabled. The most overheads are
  // transferred to the backend prefetching threads.
  traces_[traces_tail_++] = {
      .counter = ++traces_counter_, .idx = idx, .nt = nt};
  traces_tail_ %= kIdxTracesSize;
  if (unlikely(cv_prefetch_master_.HasWaiters())) {
    cv_prefetch_master_.Signal();
  }
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void Prefetcher<InduceFn, InferFn, MappingFn>::static_prefetch(
    Index_t start_idx, Pattern_t pattern, uint32_t num) {
  next_prefetch_idx_ = start_idx;
  pattern_ = pattern;
  num_objs_to_prefetch = num;
}

template <typename InduceFn, typename InferFn, typename MappingFn>
FORCE_INLINE void
Prefetcher<InduceFn, InferFn, MappingFn>::update_state(uint8_t *state) {
  ACCESS_ONCE(state_) = state;
}

} // namespace far_memory
