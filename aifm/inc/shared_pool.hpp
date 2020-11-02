#pragma once

#include "sync.h"

#include "cb.hpp"
#include "helpers.hpp"

#include <functional>

namespace far_memory {
template <typename T> class SharedPool {
private:
  constexpr static uint32_t kNumCachedItemsPerCPU = 8;

  alignas(64) CircularBuffer<T, /* sync = */ false,
                             kNumCachedItemsPerCPU> cache_[helpers::kNumCPUs];
  CircularBuffer<T, /* sync = */ false> global_pool_;
  rt::Spin global_spin_;

public:
  SharedPool(uint32_t capacity);
  void push(T item);
  T pop();
  void for_each(const std::function<void(T)> &f);
};
} // namespace far_memory

#include "internal/shared_pool.ipp"
