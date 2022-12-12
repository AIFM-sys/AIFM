#pragma once

#include "sync.h"

#include <memory>
#include <optional>
#include <map>

namespace far_memory {

struct LockEntry {
  std::unique_ptr<rt::CondVar> cond;

  LockEntry() {}
};

class ObjLocker {
private:
  constexpr static uint32_t kNumMaps = 1024;

  std::map<uint64_t, LockEntry> maps_[kNumMaps];
  rt::Spin spins_[kNumMaps];

public:
  ObjLocker();
  uint32_t hash_func(uint64_t obj_id);
  bool try_insert(uint64_t obj_id);
  void remove(uint64_t obj_id);
};
}; // namespace far_memory
