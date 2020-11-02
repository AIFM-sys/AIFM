#pragma once

namespace far_memory {

FORCE_INLINE Region::Region() {}

FORCE_INLINE Region::~Region() {}

FORCE_INLINE Region::Region(Region &&other) { *this = std::move(other); }

FORCE_INLINE bool Region::is_invalid() const {
  return region_idx_ == kInvalidIdx;
}

FORCE_INLINE void Region::invalidate() { region_idx_ = kInvalidIdx; }

FORCE_INLINE void Region::reset() {
  first_free_byte_idx_ = kObjectPos;
  num_boundaries_ = 0;
  clear_nt();
}

FORCE_INLINE bool Region::is_local() const { return buf_ptr_; }

FORCE_INLINE void Region::update_boundaries(bool force) {
  if (force || unlikely(first_free_byte_idx_ >
                        kSize / kGCParallelism * (num_boundaries_ + 1))) {
    gc_boundaries_[num_boundaries_++] = first_free_byte_idx_;
  }
}

FORCE_INLINE uint8_t Region::get_num_boundaries() const {
  return num_boundaries_;
}

FORCE_INLINE std::pair<uint64_t, uint64_t>
Region::get_boundary(uint8_t idx) const {
  assert(idx < num_boundaries_);
  auto left_offset = (idx == 0) ? kHeaderSize : gc_boundaries_[idx - 1];
  auto right_offset = gc_boundaries_[idx];
  return std::make_pair(reinterpret_cast<uint64_t>(buf_ptr_) + left_offset,
                        reinterpret_cast<uint64_t>(buf_ptr_) + right_offset);
}

FORCE_INLINE uint32_t Region::get_ref_cnt() const {
  return ACCESS_ONCE(*reinterpret_cast<uint32_t *>(buf_ptr_ + kRefCntPos));
}

FORCE_INLINE void Region::clear_ref_cnt() {
  ACCESS_ONCE(*reinterpret_cast<uint32_t *>(buf_ptr_ + kRefCntPos)) = 0;
}

FORCE_INLINE void Region::atomic_inc_ref_cnt(uint64_t object_addr,
                                             int32_t delta) {
  auto region_addr = (object_addr) & (~(Region::kSize - 1));
  auto ref_cnt_ptr = (reinterpret_cast<uint8_t *>(region_addr)) + kRefCntPos;
  __atomic_add_fetch(reinterpret_cast<int32_t *>(ref_cnt_ptr), delta,
                     __ATOMIC_SEQ_CST);
}

FORCE_INLINE void Region::atomic_inc_ref_cnt(int32_t delta) {
  auto ref_cnt_ptr = buf_ptr_ + kRefCntPos;
  __atomic_add_fetch(reinterpret_cast<int32_t *>(ref_cnt_ptr), delta,
                     __ATOMIC_SEQ_CST);
}

FORCE_INLINE bool Region::is_gcable() const { return get_ref_cnt() == 0; }

FORCE_INLINE bool Region::is_nt() const {
  return ACCESS_ONCE(*reinterpret_cast<uint8_t *>(buf_ptr_ + kNtPos));
}

FORCE_INLINE bool Region::is_nt(uint64_t buf_ptr_addr) {
  return ACCESS_ONCE(*reinterpret_cast<uint8_t *>(buf_ptr_addr + kNtPos));
}

FORCE_INLINE void Region::set_nt() {
  ACCESS_ONCE(*reinterpret_cast<uint8_t *>(buf_ptr_ + kNtPos)) = 1;
}

FORCE_INLINE void Region::clear_nt() {
  ACCESS_ONCE(*reinterpret_cast<uint8_t *>(buf_ptr_ + kNtPos)) = 0;
}
} // namespace far_memory
