#pragma once

#include <cstring>

namespace far_memory {

template <typename K, typename V>
FORCE_INLINE
LocalConcurrentHopscotch<K, V>::LocalConcurrentHopscotch(uint32_t index_num_kv,
                                                         uint64_t data_num_kv)
    : LocalGenericConcurrentHopscotch(helpers::bsr_64(index_num_kv - 1) + 1,
                                      (data_num_kv - 1) / kKVDataSize + 1) {
  memset(per_core_size_, 0, sizeof(per_core_size_));
}

template <typename K, typename V>
FORCE_INLINE bool LocalConcurrentHopscotch<K, V>::empty() const {
  return size() == 0;
}

template <typename K, typename V>
FORCE_INLINE uint64_t LocalConcurrentHopscotch<K, V>::size() const {
  int64_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += per_core_size_[i].data; }
  return sum;
}

template <typename K, typename V>
FORCE_INLINE std::optional<V>
LocalConcurrentHopscotch<K, V>::find(const K &key) {
  uint16_t val_len;
  V val;
  get(sizeof(key), &key, &val_len, &val);
  if (val_len == 0) {
    return std::nullopt;
  } else {
    return val;
  }
}

template <typename K, typename V>
FORCE_INLINE void LocalConcurrentHopscotch<K, V>::insert(const K &key,
                                                         const V &val) {
  bool key_existed = put(sizeof(key), &key, sizeof(val), &val);
  if (!key_existed) {
    preempt_disable();
    per_core_size_[get_core_num()].data++;
    preempt_enable();
  }
}

template <typename K, typename V>
FORCE_INLINE bool LocalConcurrentHopscotch<K, V>::erase(const K &key) {
  bool key_existed = remove(sizeof(key), &key);
  if (key_existed) {
    preempt_disable();
    per_core_size_[get_core_num()].data--;
    preempt_enable();
  }
  return key_existed;
}

FORCE_INLINE LocalGenericConcurrentHopscotch::BucketEntry::BucketEntry() {
  bitmap = timestamp = 0;
  ptr = nullptr;
}
} // namespace far_memory