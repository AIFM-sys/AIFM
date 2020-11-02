#pragma once

namespace far_memory {

template <typename T>
FORCE_INLINE Stack<T>::Stack(const DerefScope &scope)
    : list_(scope, /* enable_emerge = */ false, /* customized_split = */ true) {
}

template <typename T> FORCE_INLINE bool Stack<T>::empty() const {
  return list_.empty();
}

template <typename T> FORCE_INLINE uint64_t Stack<T>::size() const {
  return list_.size();
}

template <typename T>
FORCE_INLINE const T &Stack<T>::ctop(const DerefScope &scope) const {
  return list_.cback(scope);
}

template <typename T>
FORCE_INLINE T &Stack<T>::top(const DerefScope &scope) const {
  return list_.back(scope);
}

template <typename T>
FORCE_INLINE void Stack<T>::push(const DerefScope &scope, const T &data) {
  list_.push_back(scope, data);
}

template <typename T> FORCE_INLINE void Stack<T>::pop(const DerefScope &scope) {
  list_.pop_back(scope);
}
} // namespace far_memory