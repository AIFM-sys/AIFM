#pragma once

#include "helpers.hpp"
#include "sync.h"

#include <memory>
#include <vector>

namespace far_memory {

class Slab {
public:
  constexpr static uint8_t kNumSlabClasses = 7;
  constexpr static uint32_t kMinSlabClassShift = 5;
  constexpr static uint32_t kMinSlabClassSize = (1 << kMinSlabClassShift);
  constexpr static uint32_t kMaxSlabClassSize =
      (kMinSlabClassSize << (kNumSlabClasses - 1));
  constexpr static uint32_t kReplenishChunkSize = 512;

private:
  std::unique_ptr<uint8_t> base_;
  uint64_t len_;
  uint8_t *cur_;
  rt::Spin spin_;
  std::vector<uint8_t *> slabs_[helpers::kNumCPUs][kNumSlabClasses];
  friend class FarMemTest;

  static uint32_t get_slab_idx(uint32_t size);
  static uint32_t get_slab_size(uint32_t idx);
  void replenish(uint32_t slab_idx);

public:
  Slab(uint8_t *base, uint64_t len);
  ~Slab();
  uint8_t *allocate(uint32_t size);
  void free(uint8_t *ptr, uint32_t size);
};

} // namespace far_memory

#include "internal/slab.ipp"
