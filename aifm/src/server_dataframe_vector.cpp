extern "C" {
#include <base/assert.h>
#include <base/compiler.h>
#include <base/stddef.h>
}

#include "../DataFrame/AIFM/include/simple_time.hpp"
#include "aggregator.hpp"
#include "dataframe_vector.hpp"
#include "internal/dataframe_types.hpp"
#include "server_dataframe_vector.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace far_memory {

template <typename T> ServerDataFrameVector<T>::ServerDataFrameVector() {}

template <typename T> ServerDataFrameVector<T>::~ServerDataFrameVector() {}

template <typename T>
void ServerDataFrameVector<T>::read_object(uint8_t obj_id_len,
                                           const uint8_t *obj_id,
                                           uint16_t *data_len,
                                           uint8_t *data_buf) {
  auto reader_lock = lock_.get_reader_lock();
  uint64_t index;
  assert(obj_id_len == sizeof(index));
  index = *reinterpret_cast<const uint64_t *>(obj_id);
  auto chunk_size = DataFrameVector<T>::kRealChunkSize;
  *data_len = chunk_size;
  __builtin_memcpy(
      data_buf, reinterpret_cast<uint8_t *>(vec_.data()) + index * chunk_size,
      std::min(static_cast<std::size_t>(chunk_size),
               vec_.capacity() * sizeof(T) - index * chunk_size));
}

template <typename T>
void ServerDataFrameVector<T>::write_object(uint8_t obj_id_len,
                                            const uint8_t *obj_id,
                                            uint16_t data_len,
                                            const uint8_t *data_buf) {
  auto reader_lock = lock_.get_reader_lock();
  uint64_t index;
  assert(obj_id_len == sizeof(index));
  index = *reinterpret_cast<const uint64_t *>(obj_id);
  auto chunk_size = DataFrameVector<T>::kRealChunkSize;
  assert(data_len == chunk_size);
  __builtin_memcpy(
      reinterpret_cast<uint8_t *>(vec_.data()) + index * chunk_size, data_buf,
      std::min(static_cast<std::size_t>(chunk_size),
               vec_.capacity() * sizeof(T) - index * chunk_size));
}

template <typename T>
bool ServerDataFrameVector<T>::remove_object(uint8_t obj_id_len,
                                             const uint8_t *obj_id) {
  // This should never be called.
  BUG();
}

template <typename T>
void ServerDataFrameVector<T>::compute_reserve(uint16_t input_len,
                                               const uint8_t *input_buf,
                                               uint16_t *output_len,
                                               uint8_t *output_buf) {
  auto writer_lock_np = lock_.get_writer_lock_np();
  uint64_t new_capacity;
  assert(input_len == sizeof(new_capacity));
  new_capacity = *reinterpret_cast<const uint64_t *>(input_buf);
  assert(new_capacity > vec_.capacity());
  vec_.resize(new_capacity);
  vec_.resize(vec_.capacity());
  *output_len = sizeof(uint64_t);
  *(reinterpret_cast<uint64_t *>(output_buf)) = vec_.capacity();
}

template <typename T>
void ServerDataFrameVector<T>::compute_unique(uint16_t input_len,
                                              const uint8_t *input_buf,
                                              uint16_t *output_len,
                                              uint8_t *output_buf) {
  uint8_t ds_id;
  uint64_t local_vec_size;
  assert(input_len == sizeof(ds_id) + sizeof(local_vec_size));
  ds_id = *reinterpret_cast<const decltype(ds_id) *>(input_buf);
  local_vec_size = *reinterpret_cast<const decltype(local_vec_size) *>(
      input_buf + sizeof(ds_id));
  auto *unique_dataframe_vec = reinterpret_cast<ServerDataFrameVector<T> *>(
      Server::get_server_ds(ds_id));
  auto &unique_stl_vec = unique_dataframe_vec->vec_;
  _compute_unique(local_vec_size, unique_stl_vec);
  *output_len = 2 * sizeof(uint64_t);
  *reinterpret_cast<uint64_t *>(output_buf) = unique_stl_vec.size();
  *(reinterpret_cast<uint64_t *>(output_buf) + 1) = unique_stl_vec.capacity();
}

template <typename T>
template <typename U>
void ServerDataFrameVector<T>::_compute_unique(uint64_t vec_size,
                                               std::vector<U> &unique_vec) {
  auto hash_func = [](std::reference_wrapper<const T> v) -> std::size_t {
    return (std::hash<T>{}(v.get()));
  };
  auto equal_func = [](std::reference_wrapper<const T> lhs,
                       std::reference_wrapper<const T> rhs) -> bool {
    return (lhs.get() == rhs.get());
  };
  std::unordered_set<typename std::reference_wrapper<T>::type,
                     decltype(hash_func), decltype(equal_func)>
      table(vec_size, hash_func, equal_func);

  unique_vec.reserve(vec_size);
  for (uint64_t i = 0; i < vec_size; i++) {
    auto citer = vec_[i];
    const auto insert_ret = table.emplace(std::ref(citer));

    if (insert_ret.second)
      unique_vec.push_back(citer);
  }
}

template <typename T>
void ServerDataFrameVector<T>::compute_copy_data_by_idx(
    uint16_t input_len, const uint8_t *input_buf, uint16_t *output_len,
    uint8_t *output_buf) {
  uint8_t ret_ds_id = input_buf[0];
  uint8_t idx_vec_ds_id = input_buf[1];
  uint64_t idx_vec_size = *reinterpret_cast<const uint64_t *>(input_buf + 2);
  auto &ret_vec = reinterpret_cast<ServerDataFrameVector<T> *>(
                      Server::get_server_ds(ret_ds_id))
                      ->vec_;
  auto &idx_vec = reinterpret_cast<ServerDataFrameVector<unsigned long long> *>(
                      Server::get_server_ds(idx_vec_ds_id))
                      ->vec_;
  ret_vec.reserve(idx_vec_size);
  for (uint64_t i = 0; i < idx_vec_size; i++) {
    ret_vec.push_back(vec_[idx_vec[i]]);
  }
  *output_len = sizeof(uint64_t);
  *reinterpret_cast<uint64_t *>(output_buf) = ret_vec.capacity();
}

template <typename T>
void ServerDataFrameVector<T>::compute_shuffle_data_by_idx(
    uint16_t input_len, const uint8_t *input_buf, uint16_t *output_len,
    uint8_t *output_buf) {
  uint8_t ret_ds_id = input_buf[0];
  uint8_t idx_vec_ds_id = input_buf[1];
  uint64_t idx_vec_size = *reinterpret_cast<const uint64_t *>(input_buf + 2);
  auto &ret_vec = reinterpret_cast<ServerDataFrameVector<T> *>(
                      Server::get_server_ds(ret_ds_id))
                      ->vec_;
  auto &idx_vec = reinterpret_cast<ServerDataFrameVector<unsigned long long> *>(
                      Server::get_server_ds(idx_vec_ds_id))
                      ->vec_;
  ret_vec.reserve(idx_vec_size);
  for (uint64_t i = 0; i < idx_vec_size; i++) {
    ret_vec[idx_vec[i]] = vec_[i];
  }
  *output_len = sizeof(uint64_t);
  *reinterpret_cast<uint64_t *>(output_buf) = ret_vec.capacity();
}

template <typename T>
void ServerDataFrameVector<T>::compute_assign(uint16_t input_len,
                                              const uint8_t *input_buf,
                                              uint16_t *output_len,
                                              uint8_t *output_buf) {
  uint8_t from_vec_ds_id = input_buf[0];
  uint64_t from_vec_begin_idx =
      *reinterpret_cast<const uint64_t *>(input_buf + sizeof(from_vec_ds_id));
  uint64_t from_vec_end_idx = *reinterpret_cast<const uint64_t *>(
      input_buf + sizeof(from_vec_ds_id) + sizeof(from_vec_begin_idx));
  auto size = from_vec_end_idx - from_vec_begin_idx;
  auto &from_vec = reinterpret_cast<ServerDataFrameVector<T> *>(
                       Server::get_server_ds(from_vec_ds_id))
                       ->vec_;
  vec_.resize(size);
  memcpy(vec_.data(), from_vec.data() + from_vec_begin_idx, size * sizeof(T));
  *output_len = sizeof(uint64_t);
  *reinterpret_cast<uint64_t *>(output_buf) = vec_.capacity();
}

template <typename T>
void ServerDataFrameVector<T>::compute_aggregate(uint8_t opcode,
                                                 uint16_t input_len,
                                                 const uint8_t *input_buf,
                                                 uint16_t *output_len,

                                                 uint8_t *output_buf) {
  uint8_t result_ds = input_buf[0];
  uint8_t key_ds = input_buf[sizeof(result_ds)];
  uint64_t size = *reinterpret_cast<const uint64_t *>(
      input_buf + sizeof(result_ds) + sizeof(key_ds));
  uint8_t key_dt_id =
      input_buf[sizeof(result_ds) + sizeof(key_ds) + sizeof(size)];
  uint64_t result_size, result_capacity;
  switch (key_dt_id) {
  case DataFrameTypeID::Char:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<char>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Short:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<short>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Int:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<int>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::UnsignedInt:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<unsigned int>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Long:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<long>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::UnsignedLong:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<unsigned long>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::LongLong:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<long long>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::UnsignedLongLong:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<unsigned long long>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Float:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<float>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Double:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<double>(opcode, result_ds, key_ds, size);
    break;
  case DataFrameTypeID::Time:
    std::tie(result_size, result_capacity) =
        _compute_aggregate<SimpleTime>(opcode, result_ds, key_ds, size);
    break;
  default:
    BUG();
  }
  *output_len = 2 * sizeof(uint64_t);
  *reinterpret_cast<uint64_t *>(output_buf) = result_size;
  *(reinterpret_cast<uint64_t *>(output_buf) + 1) = result_capacity;
}

template <typename T>
template <typename Key_t>
std::pair<uint64_t, uint64_t>
ServerDataFrameVector<T>::_compute_aggregate(uint8_t opcode, uint8_t result_ds,
                                             uint8_t key_ds, uint64_t size) {
  auto &result_vec = reinterpret_cast<ServerDataFrameVector<T> *>(
                         Server::get_server_ds(result_ds))
                         ->vec_;
  auto &key_vec = reinterpret_cast<ServerDataFrameVector<Key_t> *>(
                      Server::get_server_ds(key_ds))
                      ->vec_;
  std::unique_ptr<Aggregator<T>> aggregator(
      AggregatorFactory<T>::build(opcode, /* limited_mem */ false, nullptr));
  DerefScope *scope =
      nullptr; // Never used. Just for complying with add()'s interface.
  Key_t last_key = key_vec[0];
  for (uint64_t i = 0; i < size; i++) {
    auto cur_key = key_vec[i];
    if (last_key != cur_key) {
      result_vec.push_back(aggregator->aggregate());
      last_key = cur_key;
    }
    aggregator->add(scope, vec_[i]);
  }
  result_vec.push_back(aggregator->aggregate());
  return std::make_pair(result_vec.size(), result_vec.capacity());
}

template <typename T>
void ServerDataFrameVector<T>::compute(uint8_t opcode, uint16_t input_len,
                                       const uint8_t *input_buf,
                                       uint16_t *output_len,
                                       uint8_t *output_buf) {
  switch (opcode) {
  case GenericDataFrameVector::OpCode::Reserve:
    compute_reserve(input_len, input_buf, output_len, output_buf);
    break;
  case GenericDataFrameVector::OpCode::Unique:
    compute_unique(input_len, input_buf, output_len, output_buf);
    break;
  case GenericDataFrameVector::OpCode::CopyDataByIdx:
    compute_copy_data_by_idx(input_len, input_buf, output_len, output_buf);
    break;
  case GenericDataFrameVector::OpCode::ShuffleDataByIdx:
    compute_shuffle_data_by_idx(input_len, input_buf, output_len, output_buf);
    break;
  case GenericDataFrameVector::OpCode::Assign:
    compute_assign(input_len, input_buf, output_len, output_buf);
    break;
  case GenericDataFrameVector::OpCode::AggregateMax:
  case GenericDataFrameVector::OpCode::AggregateMin:
  case GenericDataFrameVector::OpCode::AggregateMedian:
    compute_aggregate(opcode, input_len, input_buf, output_len, output_buf);
    break;
  default:
    BUG();
  }
}

ServerDS *ServerDataFrameVectorFactory::build(uint32_t param_len,
                                              uint8_t *params) {
  uint8_t dt_id;
  BUG_ON(param_len != sizeof(dt_id));
  dt_id = *params;

  switch (dt_id) {
  case DataFrameTypeID::Char:
    return new ServerDataFrameVector<char>();
  case DataFrameTypeID::Short:
    return new ServerDataFrameVector<short>();
  case DataFrameTypeID::Int:
    return new ServerDataFrameVector<int>();
  case DataFrameTypeID::UnsignedInt:
    return new ServerDataFrameVector<unsigned int>();
  case DataFrameTypeID::Long:
    return new ServerDataFrameVector<long>();
  case DataFrameTypeID::UnsignedLong:
    return new ServerDataFrameVector<unsigned long>();
  case DataFrameTypeID::LongLong:
    return new ServerDataFrameVector<long long>();
  case DataFrameTypeID::UnsignedLongLong:
    return new ServerDataFrameVector<unsigned long long>();
  case DataFrameTypeID::Float:
    return new ServerDataFrameVector<float>();
  case DataFrameTypeID::Double:
    return new ServerDataFrameVector<double>();
  case DataFrameTypeID::Time:
    return new ServerDataFrameVector<SimpleTime>();
  default:
    BUG();
  }
}
} // namespace far_memory
