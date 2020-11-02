#pragma once

namespace far_memory {

template <typename T>
FORCE_INLINE LocalSkiplist<T>::LocalSkiplist(uint64_t data_size)
    : GenericLocalSkiplist(sizeof(T), data_size) {
  // Param a must be the key from the API argument instead of the in-list
  // pointer.
  is_equal_ = [&](const void *a, const void *b) -> bool {
    assert(b != positive_infinite_);
    if (unlikely(b == negative_infinite_)) {
      return false;
    }
    return *(reinterpret_cast<const T *>(a)) ==
           *(reinterpret_cast<const T *>(b));
  };
  is_smaller_ = [&](const void *a, const void *b) -> bool {
    assert(b != negative_infinite_);
    if (unlikely(b == positive_infinite_)) {
      return true;
    }
    return *(reinterpret_cast<const T *>(a)) <
           *(reinterpret_cast<const T *>(b));
  };
  is_greater_ = [&](const void *a, const void *b) -> bool {
    assert(b != positive_infinite_);
    if (unlikely(b == negative_infinite_)) {
      return true;
    }
    return *(reinterpret_cast<const T *>(a)) >
           *(reinterpret_cast<const T *>(b));
  };
}

template <typename T> FORCE_INLINE LocalSkiplist<T>::~LocalSkiplist() {}

template <typename T> FORCE_INLINE bool LocalSkiplist<T>::insert(const T &key) {
  return GenericLocalSkiplist::insert(&key);
}

template <typename T> FORCE_INLINE bool LocalSkiplist<T>::exist(const T &key) {
  return GenericLocalSkiplist::exist(&key);
}

template <typename T> FORCE_INLINE bool LocalSkiplist<T>::remove(const T &key) {
  return GenericLocalSkiplist::remove(&key);
}

} // namespace far_memory
