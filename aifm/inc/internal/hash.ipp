#pragma once

#include "MurmurHash3.h"
#include "helpers.hpp"

namespace far_memory {

FORCE_INLINE uint32_t hash_32(const void *key, int len) {
  uint32_t ret;
  MurmurHash3_x86_32(key, len, 0xDEADBEEF, &ret);
  return ret;
}
} // namespace far_memory
