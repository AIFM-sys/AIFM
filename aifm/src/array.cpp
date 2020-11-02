#include "array.hpp"
#include "internal/ds_info.hpp"
#include "manager.hpp"
#include "pointer.hpp"

namespace far_memory {

GenericArray::GenericArray(FarMemManager *manager, uint32_t item_size,
                           uint64_t num_items)
    : kNumItems_(num_items), kItemSize_(item_size),
      prefetcher_(manager->get_device(), reinterpret_cast<uint8_t *>(&ptrs_),
                  item_size) {
  preempt_disable();
  ptrs_.reset(new GenericUniquePtr[num_items]);
  preempt_enable();
  for (uint64_t i = 0; i < num_items; i++) {
    ptrs_[i] = manager->allocate_generic_unique_ptr(kVanillaPtrDSID, item_size);
  }
  kItemSize_ = item_size;
  kNumItems_ = num_items;
}

GenericArray::~GenericArray() {}

void GenericArray::disable_prefetch() {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = false;
}

void GenericArray::enable_prefetch() {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = true;
}

void GenericArray::static_prefetch(Index_t start, Index_t step, uint32_t num) {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = false;
  prefetcher_.static_prefetch(start, step, num);
}

} // namespace far_memory
