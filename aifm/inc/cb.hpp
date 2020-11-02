#pragma once

#include "helpers.hpp"
#include "sync.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace far_memory {

template <typename T, bool Sync, uint64_t Capacity = 0> class CircularBuffer {
private:
  using FixedArray = T[Capacity + 1];
  constexpr static bool kIsDynamic = (Capacity == 0);

  typename std::conditional<kIsDynamic, std::unique_ptr<T[]>, FixedArray>::type
      items_;
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
  uint32_t capacity_ = Capacity + 1;
  rt::Spin spin_;

public:
  CircularBuffer();
  template <bool B = kIsDynamic, typename = typename std::enable_if_t<B, void>>
  CircularBuffer(uint32_t size);
  CircularBuffer<T, Sync, Capacity> &
  operator=(CircularBuffer<T, Sync, Capacity> &&other);
  CircularBuffer(CircularBuffer<T, Sync, Capacity> &&other);
  uint32_t capacity() const;
  uint32_t size() const;
  template <typename D> bool push_front(D &&d);
  template <typename D> bool push_back(D &&d);
  template <typename D> std::optional<T> push_back_override(D &&d);
  template <typename D> bool pop_front(D *d);
  bool work_steal(CircularBuffer<T, Sync, Capacity> *cb);
  void clear();
  void for_each(const std::function<void(T)> &f);
};
} // namespace far_memory

#include "internal/cb.ipp"
