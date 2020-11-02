#pragma once

#include "list.hpp"

namespace far_memory {
template <typename T> class Stack {
private:
  Stack(const DerefScope &scope);

  List<T> list_;
  friend class FarMemManager;

public:
  bool empty() const;
  uint64_t size() const;
  const T &ctop(const DerefScope &scope) const;
  T &top(const DerefScope &scope) const;
  void push(const DerefScope &scope, const T &data);
  void pop(const DerefScope &scope);
};

} // namespace far_memory

#include "internal/stack.ipp"
