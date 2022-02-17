#include "sync.h"

#include "helpers.hpp"
#include "obj_locker.hpp"
#include "pointer.hpp"

namespace far_memory {

ObjLocker::ObjLocker() {}

uint32_t ObjLocker::hash_func(uint64_t x) {
#ifdef USE_UNIFORM_HASH
  constexpr uint32_t a = 17;
  constexpr uint32_t b = 47;
  constexpr uint32_t c = 211;

  auto low = (uint32_t)x;
  auto high = (uint32_t)(x >> 32);
  auto val = a * low + b * high + c;

  return val & (kNumMaps - 1);
#else
  return x & (kNumMaps - 1);
#endif
}

bool ObjLocker::try_insert(uint64_t obj_id) {
  bool success = true;
  auto idx = hash_func(obj_id);
  spins_[idx].Lock();
  auto guard = helpers::finally([&] { spins_[idx].Unlock(); });

  std::map<uint64_t, bool>::iterator iter;
  if ((iter = maps_[idx].find(obj_id)) == maps_[idx].end()) {
    maps_[idx].try_emplace(obj_id);
  } else {
    success = false;
    cvs_[idx].Wait(&spins_[idx]);
  }
  return success;
}

void ObjLocker::remove(uint64_t obj_id) {
  auto idx = hash_func(obj_id);
  spins_[idx].Lock();
  auto iter = maps_[idx].find(obj_id);
  assert(iter != maps_[idx].end());
  cvs_[idx].SignalAll();
  maps_[idx].erase(iter);
  spins_[idx].Unlock();
}
} // namespace far_memory
