extern "C" {
#include <runtime/rcu.h>
#include <runtime/runtime.h>
#include <runtime/storage.h>
#include <runtime/thread.h>
#include <runtime/timer.h>
}
#include "sync.h"
#include "thread.h"
#define __user
#include "ksched.h"

#include "deref_scope.hpp"
#include "manager.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <optional>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace far_memory {
ObjLocker FarMemManager::obj_locker_;
FarMemManager *FarMemManagerFactory::ptr_;

// The following GC status are shared with DerefScope.
bool gc_master_active;
bool almost_empty;

void GCParallelizer::master_fn() {
  for (auto &from_region : *from_regions_) {
    for (uint8_t j = 0; j < from_region.get_num_boundaries(); j++) {
      master_enqueue_task(from_region.get_boundary(j));
    }
  }
}

FarMemManager::FarMemManager(uint64_t cache_size, uint64_t far_mem_size,
                             uint32_t num_gc_threads, FarMemDevice *device)
    : cache_region_manager_(cache_size, true),
      far_mem_region_manager_(far_mem_size, false), device_ptr_(device),
      parallel_marker_(num_gc_threads, kGCSlaveThreadTaskQueueDepth,
                       &from_regions_),
      parallel_write_backer_(num_gc_threads, kGCSlaveThreadTaskQueueDepth,
                             &from_regions_),
      num_gc_threads_(num_gc_threads) {

  BUG_ON(far_mem_size >= (1ULL << FarMemPtrMeta::kObjectIDBitSize));

  ksched_fd_ = open("/dev/ksched", O_RDWR);
  if (ksched_fd_ < 0) {
    LOG_PRINTF("%s\n", "Warn: fail to open /dev/ksched.");
  }
  memset(evac_notifiers_, 0, sizeof(evac_notifiers_));

  for (uint8_t ds_id =
           std::numeric_limits<decltype(available_ds_ids_)::value_type>::min();
       ds_id <
       std::numeric_limits<decltype(available_ds_ids_)::value_type>::max();
       ds_id++) {
    if (ds_id != kVanillaPtrDSID) {
      available_ds_ids_.push(ds_id);
    }
  }
}

FarMemManager::~FarMemManager() {
  while (ACCESS_ONCE(pending_gcs_)) {
    thread_yield();
  }
}

bool FarMemManager::allocate_generic_unique_ptr_nb(
    GenericUniquePtr *ptr, uint8_t ds_id, uint16_t item_size,
    std::optional<uint8_t> optional_id_len,
    std::optional<const uint8_t *> optional_id) {
  assert(item_size <= Object::kMaxObjectDataSize);
  auto object_size =
      Object::kHeaderSize + item_size +
      (optional_id_len ? *optional_id_len : kVanillaPtrObjectIDSize);
  auto optional_local_object_addr =
      allocate_local_object_nb(false, object_size);
  if (!optional_local_object_addr) {
    return false;
  }
  auto local_object_addr = *optional_local_object_addr;
  ptr->init(local_object_addr);
  if (!optional_id_len) {
    auto remote_object_addr = allocate_remote_object(false, object_size);
    Object(local_object_addr, ds_id, static_cast<uint16_t>(item_size),
           static_cast<uint8_t>(sizeof(remote_object_addr)),
           reinterpret_cast<const uint8_t *>(&remote_object_addr));
  } else {
    Object(local_object_addr, ds_id, static_cast<uint16_t>(item_size),
           *optional_id_len, *optional_id);
  }
  Region::atomic_inc_ref_cnt(local_object_addr, -1);
  return true;
}

GenericUniquePtr FarMemManager::allocate_generic_unique_ptr(
    uint8_t ds_id, uint16_t item_size, std::optional<uint8_t> optional_id_len,
    std::optional<const uint8_t *> optional_id) {
  assert(item_size <= Object::kMaxObjectDataSize);
  auto object_size =
      Object::kHeaderSize + item_size +
      (optional_id_len ? *optional_id_len : kVanillaPtrObjectIDSize);
  auto local_object_addr = allocate_local_object(false, object_size);
  auto ptr = GenericUniquePtr(local_object_addr);
  if (!optional_id_len) {
    auto remote_object_addr = allocate_remote_object(false, object_size);
    Object(local_object_addr, ds_id, static_cast<uint16_t>(item_size),
           static_cast<uint8_t>(sizeof(remote_object_addr)),
           reinterpret_cast<const uint8_t *>(&remote_object_addr));
  } else {
    Object(local_object_addr, ds_id, static_cast<uint16_t>(item_size),
           *optional_id_len, *optional_id);
  }
  Region::atomic_inc_ref_cnt(local_object_addr, -1);
  return ptr;
}

void FarMemManager::RegionManager::push_free_region(Region &region) {
  region_spin_.Lock();
  region.reset();
  BUG_ON(!free_regions_.push_back(region));
  region_spin_.Unlock();
}

std::optional<Region> FarMemManager::RegionManager::pop_used_region() {
  Region region;
  region_spin_.Lock();
  int retry_times = 0;
nt_retry:
  bool success = nt_used_regions_.pop_front(&region);
  if (unlikely(success && !region.is_gcable())) {
    // This is very rare and can only happen when the device is
    // very slow and we only have very few available DRAM Region.
    success = false;
    nt_used_regions_.push_back(region);
    if (retry_times++ <= kPickRegionMaxRetryTimes) {
      goto nt_retry;
    }
  }
  if (!success) {
    retry_times = 0;
  t_retry:
    success = used_regions_.pop_front(&region);
    if (unlikely(success && !region.is_gcable())) {
      // Ditto.
      success = false;
      used_regions_.push_front(region);
      if (retry_times++ <= kPickRegionMaxRetryTimes) {
        goto t_retry;
      }
    }
  }
  region_spin_.Unlock();
  return success ? std::make_optional(std::move(region)) : std::nullopt;
}

bool FarMemManager::RegionManager::try_refill_core_local_free_region(
    bool nt, Region *full_region) {
  region_spin_.Lock();
  auto guard = helpers::finally([&] { region_spin_.Unlock(); });

  bool success = true;
  if (full_region) {
    if (!full_region->is_invalid()) {
      success =
          (full_region->is_local() &&
           full_region
               ->is_nt()) // is_nt() can only be called by the local region
                          // since it uses the runtime's Region space.
              ? nt_used_regions_.push_back(*full_region)
              : used_regions_.push_back(*full_region);
      BUG_ON(!success);
    }
  }
  auto &core_local_region = core_local_free_region(nt);
  if (core_local_region.is_invalid()) {
    success = free_regions_.pop_front(&core_local_region);
    if (nt) {
      core_local_region.set_nt();
    }
  }

  return success;
}

FarMemManager *
FarMemManagerFactory::build(uint64_t cache_size,
                            std::optional<uint32_t> optional_num_gc_threads,
                            FarMemDevice *device) {
  if (unlikely(ptr_)) {
    return nullptr;
  }
  uint32_t num_gc_threads = optional_num_gc_threads.has_value()
                                ? *optional_num_gc_threads
                                : kDefaultNumGCThreads;
  if (unlikely(!num_gc_threads)) {
    return nullptr;
  }
  ptr_ = new FarMemManager(cache_size, device->get_far_mem_size(),
                           num_gc_threads, device);
  return ptr_;
}

FarMemManager::RegionManager::RegionManager(uint64_t size, bool is_local) {
  auto free_regions_count = ceil(size / static_cast<double>(Region::kSize));
  if (free_regions_count <= 2 * helpers::kNumSocket1CPUs) {
    LOG_PRINTF("%s\n", "Error: two few available regions.");
    exit(-ENOSPC);
  }
  free_regions_ = std::move(CircularBuffer<Region, false>(free_regions_count));
  used_regions_ = std::move(CircularBuffer<Region, false>(free_regions_count));
  nt_used_regions_ =
      std::move(CircularBuffer<Region, false>(free_regions_count));
  if (is_local) {
    local_cache_ptr_.reset(reinterpret_cast<uint8_t *>(
        helpers::allocate_hugepage(free_regions_count * Region::kSize)));
  }
  free_regions_count -= 2 * helpers::kNumSocket1CPUs;

  uint32_t idx = 0;
  auto new_region_fn = [&](bool nt) {
    auto buf_ptr =
        is_local ? (local_cache_ptr_.get() + idx * Region::kSize) : nullptr;
    return Region(idx++, is_local, nt, buf_ptr);
  };

  FOR_ALL_SOCKET0_CORES(core_id) {
    core_local_free_regions_[core_id] = new_region_fn(false);
    core_local_free_nt_regions_[core_id] = new_region_fn(true);
  }

  for (uint64_t i = 0; i < free_regions_count; i++) {
    BUG_ON(!free_regions_.push_back(new_region_fn(false)));
  }
}

void FarMemManager::swap_in(bool nt, GenericFarMemPtr *ptr) {
  assert(preempt_enabled());

  auto meta_snapshot = ptr->meta();
  if (unlikely(meta_snapshot.is_present())) {
    return;
  }

  auto obj_id = meta_snapshot.get_object_id();
  FarMemManager::lock_object(sizeof(obj_id),
                             reinterpret_cast<const uint8_t *>(&obj_id));
  auto guard = helpers::finally([&]() {
    FarMemManager::unlock_object(sizeof(obj_id),
                                 reinterpret_cast<const uint8_t *>(&obj_id));
  });

  auto &meta = ptr->meta();
  if (likely(!meta.is_present())) {
    auto obj_addr = allocate_local_object(nt, meta.get_object_size());
    auto obj = Object(obj_addr);
    auto ds_id = meta.get_ds_id();
    uint16_t obj_data_len;
    auto obj_data_addr = reinterpret_cast<uint8_t *>(obj.get_data_addr());
    device_ptr_->read_object(ds_id, sizeof(obj_id),
                             reinterpret_cast<uint8_t *>(&obj_id),
                             &obj_data_len, obj_data_addr);
    wmb();
    obj.init(ds_id, obj_data_len, sizeof(obj_id),
             reinterpret_cast<uint8_t *>(&obj_id));
    if (!meta.is_shared()) {
      meta.set_present(obj_addr);
    } else {
      reinterpret_cast<GenericSharedPtr *>(ptr)->traverse(
          [=](GenericFarMemPtr *ptr) { ptr->meta().set_present(obj_addr); });
    }
    Region::atomic_inc_ref_cnt(obj_addr, -1);
  }
}

void FarMemManager::swap_out(GenericFarMemPtr *ptr, Object obj) {
  assert(preempt_enabled());

  auto &meta = ptr->meta();
#ifndef STW_GC
  if (unlikely(!meta.is_evacuation())) {
    return;
  }
#endif

  bool hot, nt, dirty;
  if (!meta.is_shared()) {
    hot = meta.is_hot();
    nt = meta.is_nt();
    dirty = meta.is_dirty();
  } else {
    hot = dirty = false;
    nt = true;
    reinterpret_cast<GenericSharedPtr *>(ptr)->traverse(
        [&hot, &nt, &dirty](GenericFarMemPtr *ptr) {
          hot |= ptr->meta().is_hot();
          dirty |= ptr->meta().is_dirty();
          nt &= ptr->meta().is_nt();
        });
  }

  if (hot && !nt && !ACCESS_ONCE(almost_empty)) {
    auto obj_size = obj.size();
    auto optional_local_object_addr = allocate_local_object_nb(false, obj_size);
    if (likely(optional_local_object_addr)) {
      auto new_local_object_addr = *optional_local_object_addr;
      if (auto copy_notifier = copy_notifiers_[obj.get_ds_id()]) {
        memcpy(reinterpret_cast<void *>(new_local_object_addr),
               reinterpret_cast<void *>(obj.get_addr()), Object::kHeaderSize);
        auto dest_obj = Object(new_local_object_addr);
        dest_obj.set_obj_id(obj.get_obj_id(), obj.get_obj_id_len());
        copy_notifier(Object(new_local_object_addr), obj);
      } else {
        memcpy(reinterpret_cast<void *>(new_local_object_addr),
               reinterpret_cast<void *>(obj.get_addr()), obj_size);
      }
      if (!meta.is_shared()) {
        meta.gc_copy(new_local_object_addr);
      } else {
        reinterpret_cast<GenericSharedPtr *>(ptr)->traverse(
            [=](GenericFarMemPtr *ptr) {
              ptr->meta().gc_copy(new_local_object_addr);
            });
      }
      Region::atomic_inc_ref_cnt(new_local_object_addr, -1);
      return;
    }
  }

  auto obj_id = obj.get_obj_id();
  auto obj_id_len = obj.get_obj_id_len();
  auto obj_size = obj.size();
  auto ds_id = obj.get_ds_id();
  auto data_ptr = reinterpret_cast<const uint8_t *>(obj.get_data_addr());

  auto write_object_fn = [&](uint32_t data_len) {
    if (dirty) {
      device_ptr_->write_object(ds_id, obj_id_len, obj_id, data_len, data_ptr);
    }
  };

  if (auto evac_notifier = evac_notifiers_[ds_id]) {
    if (evac_notifier(obj, write_object_fn)) { // Ptr removed.
      return;
    }
  } else {
    write_object_fn(obj.get_data_len());
  }

  if (!meta.is_shared()) {
    meta.gc_wb(ds_id, obj_size, *reinterpret_cast<const uint64_t *>(obj_id));
  } else {
    reinterpret_cast<GenericSharedPtr *>(ptr)->traverse(
        [=](GenericFarMemPtr *ptr) {
          ptr->meta().gc_wb(ds_id, obj_size,
                            *reinterpret_cast<const uint64_t *>(obj_id));
        });
  }
}

/*
  A naive from-region picker according to the simple round-robin order.
 */
void FarMemManager::pick_from_regions() {
  from_regions_.clear();
  auto ratio_per_gc_round =
      std::max(kMinRatioRegionsPerGCRound,
               std::min(kMaxRatioRegionsPerGCRound,
                        kFreeCacheHighThresh -
                            cache_region_manager_.get_free_region_ratio()));
  auto num_regions_per_gc_round =
      std::min(kMaxNumRegionsPerGCRound,
               static_cast<uint32_t>(ratio_per_gc_round *
                                     cache_region_manager_.get_num_regions()));
  do {
    auto optional_region = pop_cache_used_region();
    if (unlikely(!optional_region)) {
      break;
    }
    preempt_disable();
    from_regions_.push_back(std::move(*optional_region));
    preempt_enable();
  } while (from_regions_.size() < num_regions_per_gc_round);
}

GCParallelizer::GCParallelizer(uint32_t num_slaves, uint32_t task_queues_depth,
                               std::vector<Region> *from_regions)
    : Parallelizer<GCTask>(num_slaves, task_queues_depth),
      from_regions_(from_regions) {}

GCParallelMarker::GCParallelMarker(uint32_t num_slaves,
                                   uint32_t task_queues_depth,
                                   std::vector<Region> *from_regions)
    : GCParallelizer(num_slaves, task_queues_depth, from_regions) {}

void GCParallelMarker::slave_fn(uint32_t tid) {
  preempt_disable();
  start_gc_us[get_core_num()].c = microtime();
  preempt_enable();
  while (!slave_can_exit(tid)) {
    GCTask task;
    if (slave_dequeue_task(tid, &task)) {
      auto [left, right] = task;
      auto cur = left;
      while (cur + Object::kHeaderSize < right) {
        auto obj = Object(cur);
        if (!obj.is_freed()) {
          auto obj_id_len = obj.get_obj_id_len();
          auto *obj_id = obj.get_obj_id();
          FarMemManager::lock_object(obj_id_len, obj_id);
          auto guard = helpers::finally(
              [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });
          if (likely(!obj.is_freed())) {
            auto *ptr =
                reinterpret_cast<GenericFarMemPtr *>(obj.get_ptr_addr());
            if (!ptr->meta().is_shared()) {
              ptr->meta().set_evacuation();
            } else {
              reinterpret_cast<GenericSharedPtr *>(ptr)->traverse(
                  [](GenericSharedPtr *ptr) { ptr->meta().set_evacuation(); });
            }
          }
        }
        cur += helpers::align_to(obj.size(), sizeof(FarMemPtrMeta));
      }
    }
  }
}

void FarMemManager::mark_fm_ptrs(auto *preempt_guard) {
  Status slaves_status[num_gc_threads_];
  for (uint32_t i = 0; i < num_gc_threads_; i++) {
    slaves_status[i] = GC;
  }
  parallel_marker_.spawn(slaves_status);
  start_prioritizing(GC);
  // Now we can safely enable the preemption of GC master thread, since it
  // has been prioritized
  preempt_guard->reset();
  parallel_marker_.execute();
}

void FarMemManager::wait_mutators_observation() {
  auto old_status = load_acquire(&expected_status);
#ifndef STW_GC
  store_release(&expected_status, DerefScope::flip_status(old_status));
  set_self_th_status(old_status);
  start_prioritizing(old_status);
#endif
  // Wait all mutator threads swicth to the new status.
  while (DerefScope::get_num_threads(old_status)) {
    thread_yield();
  }
#ifndef STW_GC
  set_self_th_status(GC);
#endif
}

GCParallelWriteBacker::GCParallelWriteBacker(uint32_t num_slaves,
                                             uint32_t task_queues_depth,
                                             std::vector<Region> *from_regions)
    : GCParallelizer(num_slaves, task_queues_depth, from_regions) {}

void GCParallelWriteBacker::slave_fn(uint32_t tid) {
  preempt_disable();
  start_gc_us[get_core_num()].c = microtime();
  preempt_enable();
  while (!slave_can_exit(tid)) {
    GCTask task;
    if (slave_dequeue_task(tid, &task)) {
      auto [left, right] = task;
      auto cur = left;
      auto *manager = FarMemManagerFactory::get();
      while (cur + Object::kHeaderSize < right) {
        auto obj = Object(cur);
        if (!obj.is_freed()) {
          auto obj_id_len = obj.get_obj_id_len();
          auto *obj_id = obj.get_obj_id();
          FarMemManager::lock_object(obj_id_len, obj_id);
          auto guard = helpers::finally(
              [&]() { FarMemManager::unlock_object(obj_id_len, obj_id); });
          if (likely(!obj.is_freed())) {
            auto *ptr =
                reinterpret_cast<GenericFarMemPtr *>(obj.get_ptr_addr());
            manager->swap_out(ptr, obj);
          }
        }
        cur += helpers::align_to(obj.size(), sizeof(FarMemPtrMeta));
      }
    }
  }
}

void FarMemManager::write_back_regions() {
  Status slaves_status[num_gc_threads_];
  for (uint32_t i = 0; i < num_gc_threads_; i++) {
    slaves_status[i] = GC;
  }
  parallel_write_backer_.spawn(slaves_status);
#ifndef STW_GC
  start_prioritizing(GC);
#endif
  parallel_write_backer_.execute();
}

void FarMemManager::start_prioritizing(Status status) {
#ifndef DISABLE_PRIORITIZING
  __prioritized_status = status;
  store_release(&__global_prioritizing, true);
  // Broadcast the "start prioritizing" message to all cores.
  if (ioctl(ksched_fd_, KSCHED_IOC_PRIORITIZE) != 0) {
    LOG_PRINTF("%s\n", "Warn: fail to do ioctl KSCHED_IOC_PRIORITIZE");
  }
#endif
}

void FarMemManager::stop_prioritizing() {
#ifndef DISABLE_PRIORITIZING
  store_release(&__global_prioritizing, false);
#endif
}

void FarMemManager::gc_cache() {
#ifdef GC_LOG
  std::chrono::time_point<std::chrono::steady_clock> ts[6];
#endif
  assert(preempt_enabled());
#ifndef STW_GC
  preempt_disable();
  set_self_th_status(GC);
  auto preempt_guard = helpers::finally([]() {
    preempt_enable();
    assert(preempt_enabled());
  });
#endif
  start_gc_us[get_core_num()].c = microtime();

  if (unlikely(!is_free_cache_low())) {
    return;
  }

  // No more than 1 active gc master thread.
  if (!__sync_bool_compare_and_swap(&gc_master_active, false, true)) {
    return;
  }

#ifdef GC_LOG
  LOG_PRINTF("%s%lf\n", "Info: start GC, free mem ratio = ",
             cache_region_manager_.get_free_region_ratio());
#endif

  while (!is_free_cache_high()) {
    // Phase 1. Pick regions to be GCed.
#ifdef GC_LOG
    ts[0] = std::chrono::steady_clock::now();
#endif
    pick_from_regions();
    if (unlikely(!from_regions_.size())) {
      LOG_PRINTF("%s\n", "Warn: GC cannot find any from_regions.");
      thread_yield();
      continue;
    }

    // Phase 2. Mark the far memory pointers within the picked regions.
#ifdef GC_LOG
    ts[1] = std::chrono::steady_clock::now();
#endif
#ifndef STW_GC
    mark_fm_ptrs(&preempt_guard);
#endif

    // Phase 3. Wait all mutator threads to observe the marking.
#ifdef GC_LOG
    ts[2] = std::chrono::steady_clock::now();
#endif
    wait_mutators_observation();

    // Phase 4. Write back the regions to far memory.
#ifdef GC_LOG
    ts[3] = std::chrono::steady_clock::now();
#endif
    write_back_regions();

    // Phase 5. Add regions to the free list.
#ifdef GC_LOG
    ts[4] = std::chrono::steady_clock::now();
#endif
    for (auto &from_region : from_regions_) {
      push_cache_free_region(from_region);
    }
    gc_lock_.Lock();
    if (!is_free_cache_almost_empty()) {
      ACCESS_ONCE(almost_empty) = false;
#ifndef STW_GC
      mutator_cache_condvar_.SignalAll();
#endif
    }
    gc_lock_.Unlock();

#ifdef GC_LOG
    ts[5] = std::chrono::steady_clock::now();
    for (uint32_t i = 1; i < sizeof(ts) / sizeof(ts[0]); i++) {
      LOG_PRINTF("%s%llu%s%d%s%d%s\n",
                 "Info: ts = ", helpers::chrono_to_timestamp(ts[i]),
                 ", GC phase ", (int)i, " takes ",
                 (int)(std::chrono::duration_cast<std::chrono::microseconds>(
                           ts[i] - ts[i - 1])
                           .count()),
                 " us.");
    }
    LOG_PRINTF("%s%lld%s%lf\n", "Info: GC frees ",
               (unsigned long long)from_regions_.size() * Region::kSize,
               " bytes space, free mem ratio = ",
               cache_region_manager_.get_free_region_ratio());
#endif
  }

#ifdef GC_LOG
  LOG_PRINTF("%s%lf\n", "Info: finish GC, free mem ratio = ",
             cache_region_manager_.get_free_region_ratio());
#endif
  // Final cleanup.
#ifdef STW_GC
  mutator_cache_condvar_.SignalAll();
#else
  stop_prioritizing();
#endif
  gc_master_spawned_ = false;
  store_release(&gc_master_active, false);
}

uint64_t FarMemManager::allocate_local_object(bool nt, uint16_t object_size) {
  preempt_disable();
  std::optional<uint64_t> optional_local_addr;
  bool per_core_local_region_refilled = false;
  auto guard = helpers::finally([&] {
    if (unlikely(per_core_local_region_refilled)) {
      gc_check();
    }
    preempt_enable();
  });
retry_allocate_local:
  auto &free_local_region = cache_region_manager_.core_local_free_region(nt);
  optional_local_addr = free_local_region.allocate_object(object_size);

  if (likely(optional_local_addr)) {
    return *optional_local_addr;
  } else {
    bool success = cache_region_manager_.try_refill_core_local_free_region(
        nt, &free_local_region);
    per_core_local_region_refilled = true;
    if (unlikely(!success)) {
      preempt_enable();
      mutator_wait_for_gc_cache();
      preempt_disable();
    }
    goto retry_allocate_local;
  }
}

std::optional<uint64_t>
FarMemManager::allocate_local_object_nb(bool nt, uint16_t object_size) {
  preempt_disable();
  std::optional<uint64_t> optional_local_addr;
  bool per_core_local_region_refilled = false;
  auto guard = helpers::finally([&] {
    if (unlikely(per_core_local_region_refilled)) {
      gc_check();
    }
    preempt_enable();
  });
retry_allocate_local:
  auto &free_local_region = cache_region_manager_.core_local_free_region(nt);
  optional_local_addr = free_local_region.allocate_object(object_size);

  if (likely(optional_local_addr)) {
    return *optional_local_addr;
  } else {
    bool success = cache_region_manager_.try_refill_core_local_free_region(
        nt, &free_local_region);
    per_core_local_region_refilled = true;
    if (unlikely(!success)) {
      return std::nullopt;
    }
    goto retry_allocate_local;
  }
}

uint64_t FarMemManager::allocate_remote_object(bool nt, uint16_t object_size) {
  preempt_disable();
  auto guard = helpers::finally([&]() { preempt_enable(); });
  std::optional<uint64_t> optional_remote_addr;
retry_allocate_far_mem:
  auto &free_remote_region = far_mem_region_manager_.core_local_free_region(nt);
  optional_remote_addr = free_remote_region.allocate_object(object_size);
  if (unlikely(!optional_remote_addr)) {
    bool success = far_mem_region_manager_.try_refill_core_local_free_region(
        nt, &free_remote_region);
    if (unlikely(!success)) {
      preempt_enable();
      mutator_wait_for_gc_far_mem();
      preempt_disable();
    }
    goto retry_allocate_far_mem;
  }
  return *optional_remote_addr;
}

void FarMemManager::mutator_wait_for_gc_cache() {
  assert(preempt_enabled());
  gc_lock_.Lock();
  auto guard = helpers::finally([&]() { gc_lock_.Unlock(); });
  if (unlikely(!ACCESS_ONCE(almost_empty))) {
    return;
  }
  if (unlikely(DerefScope::is_in_deref_scope())) {
    LOG_PRINTF("%s\n", "Error: runs out of space with an unexpected status.");
    Stats::print_free_mem_ratio_records();
    exit(-ENOSPC);
  }
#ifdef STW_GC
  launch_gc_master();
#endif
  do {
    mutator_cache_condvar_.Wait(&gc_lock_);
  } while (ACCESS_ONCE(almost_empty));
  guard.reset();
#ifdef DEBUG
  LOG_PRINTF("%s\n", "Warn: mutator paused due to insufficient memory.");
#endif
}

void FarMemManager::mutator_wait_for_gc_far_mem() {
  LOG_PRINTF("%s\n", "Warn: GCing far mem has not been implemented yet.");
}

void FarMemManager::launch_gc_master() {
  if (!load_acquire(&gc_master_active)) {
    auto func = [&]() {
      pending_gcs_++;
      gc_cache();
      pending_gcs_--;
    };
    if (preempt_enabled()) {
      rt::ParkAndSwitch(func);
    } else {
      // Prevent too many spawned threads.
      if (__sync_bool_compare_and_swap(&gc_master_spawned_, false, true)) {
        rt::Spawn(func);
      }
    }
  }
}

uint8_t FarMemManager::allocate_ds_id() {
  auto ds_id = available_ds_ids_.front();
  available_ds_ids_.pop();
  return ds_id;
}

void FarMemManager::free_ds_id(uint8_t ds_id) { available_ds_ids_.push(ds_id); }

bool FarMemManager::reallocate_generic_unique_ptr_nb(const DerefScope &scope,
                                                     GenericUniquePtr *ptr,
                                                     uint16_t new_item_size,
                                                     const uint8_t *data_buf) {
  ptr->deref(scope);
  // Allocate the new object space.
  auto old_obj = ptr->object();
  auto old_obj_id_len = old_obj.get_obj_id_len();
  auto old_obj_ds_id = old_obj.get_ds_id();
  auto new_obj_size = Object::kHeaderSize + new_item_size + old_obj_id_len;
  auto optional_local_object_addr =
      allocate_local_object_nb(false, new_obj_size);
  if (!optional_local_object_addr) {
    return false;
  }
  auto local_object_addr = *optional_local_object_addr;
  memcpy(reinterpret_cast<char *>(local_object_addr) + Object::kHeaderSize,
         data_buf, new_item_size);
  wmb();
  ptr->init(local_object_addr);
  if (old_obj_ds_id == kVanillaPtrDSID) {
    auto remote_object_addr = allocate_remote_object(false, new_obj_size);
    assert(old_obj_id_len == kVanillaPtrObjectIDSize);
    Object(local_object_addr, old_obj_ds_id,
           static_cast<uint16_t>(new_item_size), kVanillaPtrObjectIDSize,
           reinterpret_cast<const uint8_t *>(&remote_object_addr));
  } else {
    Object(local_object_addr, old_obj_ds_id,
           static_cast<uint16_t>(new_item_size), old_obj_id_len,
           reinterpret_cast<const uint8_t *>(old_obj.get_obj_id()));
  }
  wmb();
  // Free old object and update the pointer.
  old_obj.free();
  Region::atomic_inc_ref_cnt(local_object_addr, -1);
  return true;
}

GenericConcurrentHopscotch
FarMemManager::allocate_concurrent_hopscotch(uint32_t local_num_entries_shift,
                                             uint32_t remote_num_entries_shift,
                                             uint64_t remote_data_size) {
  return GenericConcurrentHopscotch(allocate_ds_id(), local_num_entries_shift,
                                    remote_num_entries_shift, remote_data_size);
}

GenericConcurrentHopscotch *FarMemManager::allocate_concurrent_hopscotch_heap(
    uint32_t local_num_entries_shift, uint32_t remote_num_entries_shift,
    uint64_t remote_data_size) {
  return new GenericConcurrentHopscotch(
      allocate_ds_id(), local_num_entries_shift, remote_num_entries_shift,
      remote_data_size);
}

} // namespace far_memory
