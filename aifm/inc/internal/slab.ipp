#pragma once

namespace far_memory {

FORCE_INLINE uint32_t Slab::get_slab_idx(uint32_t size) {
  auto rounded_size = helpers::round_up_power_of_two(size);
  BUG_ON(rounded_size > kMaxSlabClassSize);
  return helpers::bsr_32(std::max(rounded_size >> kMinSlabClassShift,
                                  static_cast<unsigned int>(1)));
}

FORCE_INLINE uint32_t Slab::get_slab_size(uint32_t idx) {
  return (1 << (kMinSlabClassShift + idx));
}
} // namespace far_memory
