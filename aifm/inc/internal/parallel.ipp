#pragma once

extern "C" {
#include <base/compiler.h>
}

#include "helpers.hpp"

#include <iostream>

namespace far_memory {

template <typename Task>
FORCE_INLINE Parallelizer<Task>::Parallelizer(uint32_t num_slaves,
                                              uint32_t task_queues_depth)
    : threads_{num_slaves} {
  if (very_unlikely(!num_slaves || !task_queues_depth)) {
    std::cerr << "Error: invalid arguments in Parallelizer." << std::endl;
    exit(-EINVAL);
  }
  num_slaves_ = num_slaves;
  preempt_disable();
  task_queues_ =
      std::make_unique<std::unique_ptr<CircularBuffer<Task, true>>[]>(
          num_slaves);
  for (uint32_t i = 0; i < num_slaves; i++) {
    task_queues_[i] =
        std::make_unique<CircularBuffer<Task, true>>(task_queues_depth);
  }
  preempt_enable();
}

template <typename Task>
template <typename T>
FORCE_INLINE void Parallelizer<Task>::master_enqueue_task(T &&task) {
  bool pushed = false;
  while (!pushed) {
    pushed = task_queues_[enqueue_thread_id_]->push_back(task);
    // Dispatch task to workers in a round-robin fashion.
    enqueue_thread_id_++;
    if (unlikely(enqueue_thread_id_ == num_slaves_)) {
      enqueue_thread_id_ = 0;
    }
  }
}

template <typename Task>
FORCE_INLINE bool Parallelizer<Task>::slave_dequeue_task(uint32_t tid,
                                                         Task *task) {
  return task_queues_[tid]->pop_front(task);
}

template <typename Task>
FORCE_INLINE bool Parallelizer<Task>::slave_can_exit(uint32_t tid) {
  if (unlikely(!master_up_)) {
    thread_yield();
  }
  if (unlikely(task_queues_[tid]->size() == 0)) {
    // Work stealing.
    for (uint32_t i = 0; i < num_slaves_; i++) {
      if (i == tid) {
        continue;
      }
      if (task_queues_[tid]->work_steal(task_queues_[i].get())) {
        goto done;
      }
    }
  }
done:
  return master_done_ && task_queues_[tid]->size() == 0;
}

template <typename Task>
FORCE_INLINE void Parallelizer<Task>::spawn(Status *slaves_status) {
  for (uint8_t i = 0; i < num_slaves_; i++) {
    threads_[i] =
        std::move(rt::Thread([&, i] { slave_fn(i); },
                             /* round-robin = */ true, slaves_status[i]));
  }
}

template <typename Task> FORCE_INLINE void Parallelizer<Task>::execute() {
  preempt_disable();
  BUG_ON(preempt_enabled());
  master_up_ = true;
  master_fn();
  master_done_ = true;
  preempt_enable();
  for (auto &thread : threads_) {
    thread.Join();
  }
#ifdef DEBUG
  for (uint8_t i = 0; i < num_slaves_; i++) {
    assert(task_queues_[i]->size() == 0);
  }
#endif
  master_up_ = false;
  master_done_ = false;
}
} // namespace far_memory
