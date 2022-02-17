#pragma once

#include <cstdint>

#include "helpers.hpp"

namespace far_memory {

class RCULock {
public:
  RCULock();
  void reader_lock();
  void reader_unlock();
  void writer_sync();

private:
  union Cnt {
    struct Data {
      int32_t c;
      int32_t ver;
    } data;
    uint64_t raw;
  };
  static_assert(sizeof(Cnt) == sizeof(Cnt::Data));

  bool sync_barrier_;
  CachelineAligned(Cnt) aligned_cnts_[helpers::kNumCPUs];

  void __reader_lock();
};
} // namespace far_memory

#include "internal/rcu_lock.ipp"
