#pragma once

namespace far_memory {

template <typename T>
FORCE_INLINE SharedPool<T>::SharedPool(uint32_t capacity)
    : global_pool_(capacity) {}

template <typename T> FORCE_INLINE void SharedPool<T>::push(T item) {
  preempt_disable();
  auto preempt_guard = helpers::finally([&]() { preempt_enable(); });

  int core_num = get_core_num();
  auto &cache = cache_[core_num];
  if (unlikely(cache.size() == kNumCachedItemsPerCPU)) {
    global_spin_.Lock();
    auto spin_guard = helpers::finally([&]() { global_spin_.Unlock(); });
    for (uint32_t i = 0; i < kNumCachedItemsPerCPU; i++) {
      T migrated;
      BUG_ON(!cache.pop_front(&migrated));
      BUG_ON(!global_pool_.push_front(migrated));
    }
  }
  BUG_ON(!cache.push_front(item));
}

template <typename T> FORCE_INLINE T SharedPool<T>::pop() {
  preempt_disable();
  auto preempt_guard = helpers::finally([&]() { preempt_enable(); });

  int core_num = get_core_num();
  auto &cache = cache_[core_num];
  if (unlikely(!cache.size())) {
    global_spin_.Lock();
    auto spin_guard = helpers::finally([&]() { global_spin_.Unlock(); });
    for (uint32_t i = 0; i < kNumCachedItemsPerCPU; i++) {
      T migrated;
      BUG_ON(!global_pool_.pop_front(&migrated));
      BUG_ON(!cache.push_front(migrated));
    }
  }
  T item;
  BUG_ON(!cache.pop_front(&item));
  return item;
}

template <typename T>
FORCE_INLINE void SharedPool<T>::for_each(const std::function<void(T)> &f) {
  FOR_ALL_SOCKET0_CORES(core_id) { cache_[core_id].for_each(f); }
  global_spin_.Lock();
  global_pool_.for_each(f);
  global_spin_.Unlock();
}
} // namespace far_memory
