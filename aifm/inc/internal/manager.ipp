#pragma once

#include "dataframe_vector.hpp"
#include "stats.hpp"
#include "thread.h"


namespace far_memory {

extern bool gc_master_active;

FORCE_INLINE Region &
FarMemManager::RegionManager::core_local_free_region(bool nt) {
  assert(!preempt_enabled());
  auto core_num = get_core_num();
  return nt ? core_local_free_nt_regions_[core_num]
            : core_local_free_regions_[core_num];
}

FORCE_INLINE double
FarMemManager::RegionManager::get_free_region_ratio() const {
  return static_cast<double>(free_regions_.size()) / get_num_regions();
}

FORCE_INLINE uint32_t FarMemManager::RegionManager::get_num_regions() const {
  return free_regions_.capacity();
}

FORCE_INLINE double FarMemManager::get_free_mem_ratio() const {
  return cache_region_manager_.get_free_region_ratio();
}

FORCE_INLINE bool FarMemManager::is_free_cache_low() const {
  return get_free_mem_ratio() <= kFreeCacheLowThresh;
}

FORCE_INLINE bool FarMemManager::is_free_cache_almost_empty() const {
  return get_free_mem_ratio() <= kFreeCacheAlmostEmptyThresh;
}

FORCE_INLINE bool FarMemManager::is_free_cache_high() const {
  return get_free_mem_ratio() >= kFreeCacheHighThresh;
}

FORCE_INLINE void FarMemManager::push_cache_free_region(Region &region) {
  cache_region_manager_.push_free_region(region);
}

FORCE_INLINE std::optional<Region> FarMemManager::pop_cache_used_region() {
  return cache_region_manager_.pop_used_region();
}

template <typename T>
FORCE_INLINE UniquePtr<T> FarMemManager::allocate_unique_ptr(uint8_t ds_id) {
  static_assert(sizeof(T) <= Object::kMaxObjectDataSize);
  auto object_size = Object::kHeaderSize + sizeof(T) + kVanillaPtrObjectIDSize;
  auto local_object_addr = allocate_local_object(false, object_size);
  auto remote_object_addr = allocate_remote_object(false, object_size);
  Object(local_object_addr, ds_id, static_cast<uint16_t>(sizeof(T)),
         static_cast<uint8_t>(sizeof(remote_object_addr)),
         reinterpret_cast<const uint8_t *>(&remote_object_addr));
  auto ptr = UniquePtr<T>(local_object_addr);
  Region::atomic_inc_ref_cnt(local_object_addr, -1);
  return ptr;
}

template <typename T>
FORCE_INLINE SharedPtr<T> FarMemManager::allocate_shared_ptr(uint8_t ds_id) {
  static_assert(sizeof(T) <= Object::kMaxObjectDataSize);
  auto object_size = Object::kHeaderSize + sizeof(T) + kVanillaPtrObjectIDSize;
  auto local_object_addr = allocate_local_object(false, object_size);
  auto remote_object_addr = allocate_remote_object(false, object_size);
  Object(local_object_addr, ds_id, static_cast<uint16_t>(sizeof(T)),
         static_cast<uint8_t>(sizeof(remote_object_addr)),
         reinterpret_cast<const uint8_t *>(&remote_object_addr));
  auto ptr = SharedPtr<T>(local_object_addr);
  Region::atomic_inc_ref_cnt(local_object_addr, -1);
  return ptr;
}

template <typename T, uint64_t... Dims>
FORCE_INLINE Array<T, Dims...> FarMemManager::allocate_array() {
  return Array<T, Dims...>(this);
}

template <typename T, uint64_t... Dims>
FORCE_INLINE Array<T, Dims...> *FarMemManager::allocate_array_heap() {
  return new Array<T, Dims...>(this);
}

template <typename T>
FORCE_INLINE List<T> FarMemManager::allocate_list(const DerefScope &scope,
                                                  bool enable_merge) {
  return List<T>(scope, enable_merge);
}

template <typename T>
FORCE_INLINE Queue<T> FarMemManager::allocate_queue(const DerefScope &scope) {
  return Queue<T>(scope);
}

template <typename T>
FORCE_INLINE Stack<T> FarMemManager::allocate_stack(const DerefScope &scope) {
  return Stack<T>(scope);
}

template <typename T>
FORCE_INLINE DataFrameVector<T> FarMemManager::allocate_dataframe_vector() {
  return DataFrameVector<T>(this);
}

template <typename T>
FORCE_INLINE DataFrameVector<T> *
FarMemManager::allocate_dataframe_vector_heap() {
  return new DataFrameVector<T>(this);
}

FORCE_INLINE FarMemManager *FarMemManagerFactory::get() { return ptr_; }

FORCE_INLINE void FarMemManager::register_eval_notifier(uint8_t ds_id,
                                                        EvacNotifier notifier) {
  evac_notifiers_[ds_id] = notifier;
}

FORCE_INLINE void FarMemManager::register_copy_notifier(uint8_t ds_id,
                                                        CopyNotifier notifier) {
  copy_notifiers_[ds_id] = notifier;
}

FORCE_INLINE void FarMemManager::read_object(uint8_t ds_id, uint8_t obj_id_len,
                                             const uint8_t *obj_id,
                                             uint16_t *data_len,
                                             uint8_t *data_buf) {
  device_ptr_->read_object(ds_id, obj_id_len, obj_id, data_len, data_buf);
}

FORCE_INLINE bool FarMemManager::remove_object(uint64_t ds_id,
                                               uint8_t obj_id_len,
                                               const uint8_t *obj_id) {
  return device_ptr_->remove_object(ds_id, obj_id_len, obj_id);
}

FORCE_INLINE void FarMemManager::construct(uint8_t ds_type, uint8_t ds_id,
                                           uint32_t param_len,
                                           uint8_t *params) {
  device_ptr_->construct(ds_type, ds_id, param_len, params);
}

FORCE_INLINE void FarMemManager::destruct(uint8_t ds_id) {
  free_ds_id(ds_id);
  device_ptr_->destruct(ds_id);
}

FORCE_INLINE uint64_t get_obj_id_fragment(uint8_t obj_id_len,
                                          const uint8_t *obj_id) {
  uint64_t obj_id_fragment;
  if (likely(obj_id_len >= 8)) {
    obj_id_fragment = *reinterpret_cast<const uint64_t *>(obj_id);
  } else {
    obj_id_fragment = 0;
    for (uint32_t i = 0; i < obj_id_len; i++) {
      *(reinterpret_cast<uint8_t *>(&obj_id_fragment) + i) = obj_id[i];
    }
  }
  return obj_id_fragment;
}

FORCE_INLINE void FarMemManager::lock_object(uint8_t obj_id_len,
                                             const uint8_t *obj_id) {
  // So far we only use at most 8 bytes of obj_id in locker.
  auto obj_id_fragment = get_obj_id_fragment(obj_id_len, obj_id);
  while (!obj_locker_.try_insert(obj_id_fragment))
    ;
}

FORCE_INLINE void FarMemManager::unlock_object(uint8_t obj_id_len,
                                               const uint8_t *obj_id) {
  auto obj_id_fragment = get_obj_id_fragment(obj_id_len, obj_id);
  obj_locker_.remove(obj_id_fragment);
}

FORCE_INLINE void FarMemManager::gc_check() {
  if (unlikely(is_free_cache_low())) {
    Stats::add_free_mem_ratio_record();
    ACCESS_ONCE(almost_empty) = is_free_cache_almost_empty();
#ifndef STW_GC
    launch_gc_master();
#endif
  }
}

template <typename K, typename V>
FORCE_INLINE ConcurrentHopscotch<K, V>
FarMemManager::allocate_concurrent_hopscotch(uint32_t local_num_entries_shift,
                                             uint32_t remote_num_entries_shift,
                                             uint64_t remote_data_size) {
  return ConcurrentHopscotch<K, V>(allocate_ds_id(), local_num_entries_shift,
                                   remote_num_entries_shift, remote_data_size);
}

template <typename K, typename V>
FORCE_INLINE ConcurrentHopscotch<K, V> *
FarMemManager::allocate_concurrent_hopscotch_heap(
    uint32_t local_num_entries_shift, uint32_t remote_num_entries_shift,
    uint64_t remote_data_size) {
  return new ConcurrentHopscotch<K, V>(
      allocate_ds_id(), local_num_entries_shift, remote_num_entries_shift,
      remote_data_size);
}

} // namespace far_memory
