#pragma once

#include "sync.h"

#include "cb.hpp"
#include "deref_scope.hpp"
#include "helpers.hpp"
#include "pointer.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace far_memory {

class GenericConcurrentHopscotch {
private:
  struct BucketEntry {
    constexpr static uint64_t kBusyPtr = FarMemPtrMeta::kNull + 1;

    uint32_t bitmap;
    rt::Spin spin;
    uint64_t timestamp;
    GenericUniquePtr ptr;

    BucketEntry();
  };
  static_assert(sizeof(BucketEntry) == 24);

#pragma pack(push, 1)
  struct EvacNotifierMeta {
    uint64_t anchor_addr : 48;
    uint8_t offset;
  };
#pragma pack(pop)
  static_assert(sizeof(EvacNotifierMeta) == 7);

  constexpr static uint32_t kNeighborhood = 32;
  constexpr static uint32_t kMaxRetries = 2;
  constexpr static uint32_t kEvacNotifierStashSize = 1024;

  const uint32_t kHashMask_;
  const uint32_t kNumEntries_;
  std::unique_ptr<uint8_t> buckets_mem_;
  BucketEntry *buckets_;
  uint8_t ds_id_;
  CircularBuffer<EvacNotifierMeta, /* Sync = */ true, kEvacNotifierStashSize>
      evac_notifier_stash_;

  friend class FarMemTest;
  friend class FarMemManager;
  template <typename K, typename V> friend class ConcurrentHopscotch;

  GenericConcurrentHopscotch(uint8_t ds_id, uint32_t local_num_entries_shift,
                             uint32_t remote_num_entries_shift,
                             uint64_t remote_data_size);
  NOT_COPYABLE(GenericConcurrentHopscotch);
  NOT_MOVEABLE(GenericConcurrentHopscotch);
  bool __get(uint8_t key_len, const uint8_t *key, uint16_t *val_len,
             uint8_t *val);
  void forward_get(uint8_t key_len, const uint8_t *key, uint16_t *val_len,
                   uint8_t *val);
  void _get(uint8_t key_len, const uint8_t *key, uint16_t *val_len,
            uint8_t *val, bool *forwarded);
  bool _put(uint8_t key_len, const uint8_t *key, uint16_t val_len,
            const uint8_t *val, bool swap_in);
  bool _remove(uint8_t key_len, const uint8_t *key);
  void process_evac_notifier_stash();
  void do_evac_notifier(EvacNotifierMeta meta);
  void evac_notifier(Object object);

public:
  constexpr static uint32_t kMetadataSize = sizeof(EvacNotifierMeta);

  ~GenericConcurrentHopscotch();
  void get(const DerefScope &scope, uint8_t key_len, const uint8_t *key,
           uint16_t *val_len, uint8_t *val);
  void get_tp(uint8_t key_len, const uint8_t *key, uint16_t *val_len,
              uint8_t *val);
  bool put(const DerefScope &scope, uint8_t key_len, const uint8_t *key,
           uint16_t val_len, const uint8_t *val);
  bool put_tp(uint8_t key_len, const uint8_t *key, uint16_t val_len,
              const uint8_t *val);
  bool remove(const DerefScope &scope, uint8_t key_len, const uint8_t *key);
  bool remove_tp(uint8_t key_len, const uint8_t *key);
};

template <typename K, typename V>
class ConcurrentHopscotch : public GenericConcurrentHopscotch {
private:
  CachelineAligned(int64_t) per_core_size_[helpers::kNumCPUs];
  friend class FarMemTest;
  friend class FarMemManager;

  std::optional<V> _find(const K &key);
  void _insert(const K &key, const V &value);
  bool _erase(const K &key);
  ConcurrentHopscotch(uint8_t ds_id, uint32_t local_num_entries_shift,
                      uint32_t remote_num_entries_shift,
                      uint64_t remote_data_size);
  NOT_COPYABLE(ConcurrentHopscotch);
  NOT_MOVEABLE(ConcurrentHopscotch);

public:
  bool empty() const;
  uint64_t size() const;
  std::optional<V> find(const DerefScope &scope, const K &key);
  std::optional<V> find_tp(const K &key);
  void insert(const DerefScope &scope, const K &key, const V &value);
  void insert_tp(const K &key, const V &value);
  bool erase(const DerefScope &scope, const K &key);
  bool erase_tp(const K &key);
};

} // namespace far_memory

#include "internal/concurrent_hopscotch.ipp"
