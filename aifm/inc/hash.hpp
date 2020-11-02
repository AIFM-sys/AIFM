#pragma once

namespace far_memory {
uint32_t hash_32(const void *key, int len);
}

#include "internal/hash.ipp"
