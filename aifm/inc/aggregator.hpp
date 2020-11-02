#pragma once

#include "deref_scope.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace far_memory {

template <typename T> class Aggregator {
public:
  virtual void add(DerefScope *scope, T t) = 0;
  virtual T aggregate() = 0;
};

template <typename T> class AggregatorMax : public Aggregator<T> {
private:
  T tmp_ = std::numeric_limits<T>::min();

public:
  void add(DerefScope *scope, T t);
  T aggregate();
};

template <typename T> class AggregatorMin : public Aggregator<T> {
private:
  T tmp_ = std::numeric_limits<T>::max();

public:
  void add(DerefScope *scope, T t);
  T aggregate();
};

template <typename T> class AggregatorMedian : public Aggregator<T> {
private:
  std::vector<T> vec_;

public:
  void add(DerefScope *scope, T t);
  T aggregate();
};

template <typename T> class DataFrameVector;
class FarMemManager;

template <typename T> class AggregatorMedianLimitedMem : public Aggregator<T> {
private:
  DataFrameVector<T> vec_;

public:
  AggregatorMedianLimitedMem(FarMemManager *manager);
  void add(DerefScope *scope, T t);
  T aggregate();
};

template <typename T> class AggregatorFactory {
public:
  static Aggregator<T> *build(uint8_t opcode, bool limited_mem,
                              FarMemManager *manager);
};

} // namespace far_memory

#include "internal/aggregator.ipp"
