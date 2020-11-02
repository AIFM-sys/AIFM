#pragma once

#include "helpers.hpp"

#include <cstdint>
#include <optional>

namespace far_memory {

class Region {
  // Format:
  // |ref_cnt(4B)|Nt(1B)|Resv(1B)|objects|
  //
  //    ref_cnt: The region can only be GCed when the ref_cnt goes to 0.
  //         Nt: is this region a non-temporal?
  //    objects: objects stored within the region.
public:
  constexpr static uint32_t kRefCntPos = 0;
  constexpr static uint32_t kRefCntSize = 4;
  constexpr static uint32_t kNtPos = 4;
  constexpr static uint32_t kNtSize = 1;
  constexpr static uint64_t kShift = 20;
  constexpr static uint64_t kSize = (1 << kShift);
  constexpr static uint8_t kGCParallelism = 2;
  constexpr static int32_t kInvalidIdx = -1;
  constexpr static uint32_t kHeaderSize = 6;
  constexpr static uint32_t kObjectPos = kHeaderSize;

  static_assert(kSize <= helpers::kHugepageSize);
  static_assert(helpers::kHugepageSize % kSize == 0);

private:
  uint32_t first_free_byte_idx_ = kObjectPos;
  uint8_t *buf_ptr_ = nullptr;
  int32_t region_idx_ = kInvalidIdx;
  uint8_t num_boundaries_ = 0;
  uint32_t gc_boundaries_[kGCParallelism];

  void update_boundaries(bool force);

public:
  Region();
  Region(uint32_t idx, bool is_local, bool nt, uint8_t *buf_ptr);
  NOT_COPYABLE(Region);
  Region(Region &&other);
  Region &operator=(Region &&other);
  ~Region();
  std::optional<uint64_t> allocate_object(uint16_t object_size);
  bool is_invalid() const;
  void invalidate();
  void reset();
  bool is_local() const;
  bool is_nt() const;
  void set_nt();
  void clear_nt();
  uint32_t get_ref_cnt() const;
  void clear_ref_cnt();
  bool is_gcable() const;
  uint8_t get_num_boundaries() const;
  std::pair<uint64_t, uint64_t> get_boundary(uint8_t idx) const;
  void atomic_inc_ref_cnt(int32_t delta);
  static bool is_nt(uint64_t buf_ptr_addr);
  static void atomic_inc_ref_cnt(uint64_t object_addr, int32_t delta);
};

} // namespace far_memory

#include "internal/region.ipp"
