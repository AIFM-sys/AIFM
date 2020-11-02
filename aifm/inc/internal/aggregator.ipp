#pragma once

#include "dataframe_vector.hpp"
#include "helpers.hpp"
#include "manager.hpp"

namespace far_memory {

template <typename T>
FORCE_INLINE void AggregatorMax<T>::add(DerefScope *scope, T t) {
  tmp_ = std::max(tmp_, t);
}

template <typename T> FORCE_INLINE T AggregatorMax<T>::aggregate() {
  auto ret = tmp_;
  tmp_ = std::numeric_limits<T>::min();
  return ret;
}

template <typename T>
FORCE_INLINE void AggregatorMin<T>::add(DerefScope *scope, T t) {
  tmp_ = std::min(tmp_, t);
}

template <typename T> FORCE_INLINE T AggregatorMin<T>::aggregate() {
  auto ret = tmp_;
  tmp_ = std::numeric_limits<T>::max();
  return ret;
}

template <typename T>
FORCE_INLINE void AggregatorMedian<T>::add(DerefScope *scope, T t) {
  vec_.push_back(t);
}

template <typename T> FORCE_INLINE T AggregatorMedian<T>::aggregate() {
  std::nth_element(vec_.begin(), vec_.begin() + vec_.size() / 2, vec_.end());
  auto ret = vec_[vec_.size() / 2];
  if constexpr (helpers::Addable<T>) {
    if (vec_.size() % 2 == 0) {
      std::nth_element(vec_.begin(), vec_.begin() + vec_.size() / 2 - 1,
                       vec_.end());
      ret = (ret + vec_[vec_.size() / 2 - 1]) / 2;
    }
  }
  vec_.clear();
  return ret;
}

template <typename T>
FORCE_INLINE AggregatorMedianLimitedMem<T>::AggregatorMedianLimitedMem(
    FarMemManager *manager)
    : vec_(std::move(manager->allocate_dataframe_vector<T>())) {}

template <typename T>
FORCE_INLINE void AggregatorMedianLimitedMem<T>::add(DerefScope *scope, T t) {
  vec_.push_back(*scope, t);
}

template <typename T>
FORCE_INLINE T AggregatorMedianLimitedMem<T>::aggregate() {
  auto ret = vec_.nth_element(vec_.size() / 2);
  if constexpr (helpers::Addable<T> && helpers::DividableByInt<T>) {
    if (vec_.size() % 2 == 0) {
      ret = (ret + vec_.nth_element(vec_.size() / 2 - 1)) / 2;
    }
  }
  vec_.clear();

  return ret;
}

template <typename T>
FORCE_INLINE Aggregator<T> *
AggregatorFactory<T>::build(uint8_t opcode, bool limited_mem,
                            FarMemManager *manager) {
  BUG_ON(limited_mem && manager == nullptr);
  switch (opcode) {
  case GenericDataFrameVector::OpCode::AggregateMax:
    return new AggregatorMax<T>();
  case GenericDataFrameVector::OpCode::AggregateMin:
    return new AggregatorMin<T>();
  case GenericDataFrameVector::OpCode::AggregateMedian:
    return limited_mem
               ? reinterpret_cast<Aggregator<T> *>(
                     new AggregatorMedianLimitedMem<T>(manager))
               : reinterpret_cast<Aggregator<T> *>(new AggregatorMedian<T>());
  default:
    BUG();
  }
}

} // namespace far_memory
