#pragma once

#include "sync.h"

#include "helpers.hpp"
#include "slab.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace far_memory {

class LocalGenericConcurrentHopscotch {
private:
#pragma pack(push, 1)
  struct KVDataHeader {
    uint8_t key_len;
    uint16_t val_len;
  };
#pragma pack(pop)
  static_assert(sizeof(KVDataHeader) == 3);

#pragma pack(push, 1)
  struct BucketEntry {
    constexpr static uint64_t kBusyPtr = 0x1;

    uint32_t bitmap;
    rt::Spin spin;
    uint64_t timestamp;
    KVDataHeader *ptr;

    BucketEntry();
  };
#pragma pack(pop)
  static_assert(sizeof(BucketEntry) == 24);

  constexpr static uint32_t kNeighborhood = 32;
  constexpr static uint32_t kMaxRetries = 2;

  const uint32_t kHashMask_;
  const uint32_t kNumEntries_;
  std::unique_ptr<uint8_t> buckets_mem_;
  uint64_t slab_base_addr_;
  Slab slab_;
  BucketEntry *buckets_;
  friend class FarMemTest;

  void do_remove(BucketEntry *bucket, BucketEntry *entry);

public:
  LocalGenericConcurrentHopscotch(uint32_t num_entries_shift,
                                  uint64_t data_size);
  ~LocalGenericConcurrentHopscotch();
  void get(uint8_t key_len, const uint8_t *key, uint16_t *val_len, uint8_t *val,
           bool remove = false);
  bool put(uint8_t key_len, const uint8_t *key, uint16_t val_len,
           const uint8_t *val);
  bool remove(uint8_t key_len, const uint8_t *key);
};

template <typename K, typename V>
class LocalConcurrentHopscotch : public LocalGenericConcurrentHopscotch {
private:
  constexpr static uint64_t kKVDataSize =
      sizeof(K) + sizeof(V) + sizeof(KVDataHeader);
  CachelineAligned(int64_t) per_core_size_[helpers::kNumCPUs];

public:
  LocalConcurrentHopscotch(uint32_t index_num_kv, uint64_t data_num_kv);
  bool empty() const;
  uint64_t size() const;
  std::optional<V> find(const K &key);
  void insert(const K &key, const V &val);
  bool erase(const K &key);
};

} // namespace far_memory

#include "internal/local_concurrent_hopscotch.ipp"
