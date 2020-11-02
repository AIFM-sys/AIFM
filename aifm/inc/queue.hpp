#pragma once

#include "list.hpp"

namespace far_memory {
template <typename T> class Queue {
private:
  Queue(const DerefScope &scope);

  List<T> list_;
  friend class FarMemManager;

public:
  bool empty() const;
  uint64_t size() const;
  const T &cfront(const DerefScope &scope) const;
  const T &cback(const DerefScope &scope) const;
  T &front(const DerefScope &scope) const;
  T &back(const DerefScope &scope) const;
  void push(const DerefScope &scope, const T &data);
  void pop(const DerefScope &scope);
};

} // namespace far_memory

#include "internal/queue.ipp"
