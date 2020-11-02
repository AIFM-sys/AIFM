#pragma once

#include "../DataFrame/AIFM/include/simple_time.hpp"
#include "helpers.hpp"

#include <cstdint>
#include <type_traits>

namespace far_memory {

enum DataFrameTypeID {
  Char = 0,
  Short,
  Int,
  UnsignedInt,
  Long,
  UnsignedLong,
  LongLong,
  UnsignedLongLong,
  Float,
  Double,
  Time
};

template <typename T> FORCE_INLINE constexpr int8_t get_dataframe_type_id() {
  if (std::is_same<T, char>::value) {
    return DataFrameTypeID::Char;
  }
  if (std::is_same<T, short>::value) {
    return DataFrameTypeID::Short;
  }
  if (std::is_same<T, int>::value) {
    return DataFrameTypeID::Int;
  }
  if (std::is_same<T, unsigned int>::value) {
    return DataFrameTypeID::UnsignedInt;
  }
  if (std::is_same<T, long>::value) {
    return DataFrameTypeID::Long;
  }
  if (std::is_same<T, unsigned long>::value) {
    return DataFrameTypeID::UnsignedLong;
  }
  if (std::is_same<T, long long>::value) {
    return DataFrameTypeID::LongLong;
  }
  if (std::is_same<T, unsigned long long>::value) {
    return DataFrameTypeID::UnsignedLongLong;
  }
  if (std::is_same<T, float>::value) {
    return DataFrameTypeID::Float;
  }
  if (std::is_same<T, double>::value) {
    return DataFrameTypeID::Double;
  }
  if (std::is_same<T, SimpleTime>::value) {
    return DataFrameTypeID::Time;
  }
  return -1;
}

template <typename T> FORCE_INLINE constexpr bool is_basic_dataframe_types() {
  return get_dataframe_type_id<T>() != -1;
}
} // namespace far_memory
