#pragma once

#include "aggregator.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstring>
#include <ctime>
#include <unordered_set>

namespace far_memory {

template <typename T>
FORCE_INLINE DataFrameVector<T>::Pattern_t
DataFrameVector<T>::induce_fn(Index_t idx_0, Index_t idx_1) {
  return static_cast<Pattern_t>(idx_1) - static_cast<Pattern_t>(idx_0);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Index_t
DataFrameVector<T>::infer_fn(Index_t idx, Pattern_t stride) {
  return idx + stride;
}

template <typename T>
FORCE_INLINE GenericUniquePtr *DataFrameVector<T>::mapping_fn(uint8_t *&state,
                                                              Index_t idx) {
  auto &lock = *reinterpret_cast<ReaderWriterLock *>(state);
  auto reader_lock = lock.get_reader_lock();
  auto &chunk_ptrs = *reinterpret_cast<std::vector<GenericUniquePtr> *>(
      ACCESS_ONCE(state) + sizeof(lock));
  return idx < chunk_ptrs.size() ? &chunk_ptrs[idx] : nullptr;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::DataFrameVector(FarMemManager *manager)
    : GenericDataFrameVector(kRealChunkSize, kRealChunkNumEntries,
                             manager->allocate_ds_id(),
                             get_dataframe_type_id<T>()),
      prefetcher_(new Prefetcher<decltype(kInduceFn), decltype(kInferFn),
                                 decltype(kMappingFn)>(
          manager->get_device(), reinterpret_cast<uint8_t *>(&lock_),
          kRealChunkSize)) {}

template <typename T>
FORCE_INLINE DataFrameVector<T>::DataFrameVector(const DataFrameVector &other)
    : DataFrameVector(FarMemManagerFactory::get()) {
  assign(other.cbegin(), other.cend());
}

template <typename T>
template <typename U>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::aggregate_locally(FarMemManager *manager, const U &key_vec,
                                      OpCode opcode) {
  assert(size() == key_vec.size());
  auto result = DataFrameVector<T>(manager);
  std::unique_ptr<Aggregator<T>> aggregator(
      AggregatorFactory<T>::build(opcode, /* limited_mem */ true, manager));
  uint64_t i = 0;
  while (i < size_) {
    DerefScope scope;
    auto data_it = cfbegin(scope) + i;
    auto key_it = key_vec.cfbegin(scope) + i;
    for (auto last_key = *key_it; *key_it == last_key && i < size_;
         ++i, ++data_it, ++key_it) {
      if (unlikely(i % kNumElementsPerScope == 0)) {
        scope.renew();
        data_it.renew(scope);
        key_it.renew(scope);
      }
      aggregator->add(&scope, *data_it);
    }
    scope.exit();
    auto agg = aggregator->aggregate();
    scope.enter();
    result.push_back(scope, agg);
  }

  return result;
}

FORCE_INLINE void GenericDataFrameVector::clear() { size_ = 0; }

FORCE_INLINE bool GenericDataFrameVector::empty() const { return size() == 0; }

FORCE_INLINE uint64_t GenericDataFrameVector::size() const { return size_; }

FORCE_INLINE
GenericDataFrameVector::GenericDataFrameVector(GenericDataFrameVector &&other)
    : chunk_size_(other.chunk_size_),
      chunk_num_entries_(other.chunk_num_entries_), device_(other.device_),
      ds_id_(other.ds_id_), size_(other.size_),
      chunk_ptrs_(std::move(other.chunk_ptrs_)), moved_(false),
      dirty_(other.dirty_) {
  assert(!other.moved_);
  other.moved_ = true;
}

FORCE_INLINE GenericDataFrameVector &GenericDataFrameVector::
operator=(GenericDataFrameVector &&other) {
  cleanup();
  chunk_size_ = other.chunk_size_;
  chunk_num_entries_ = other.chunk_num_entries_;
  device_ = other.device_;
  ds_id_ = other.ds_id_;
  size_ = other.size_;
  chunk_ptrs_ = std::move(other.chunk_ptrs_);
  moved_ = false;
  dirty_ = other.dirty_;
  other.moved_ = true;
  return *this;
}

template <typename T>
FORCE_INLINE
DataFrameVector<T>::Iterator::Iterator(DataFrameVector<T> *dataframe_vec,
                                       uint64_t chunk_idx,
                                       uint64_t chunk_offset)
    : dataframe_vec_(dataframe_vec), chunk_idx_(chunk_idx),
      chunk_offset_(chunk_offset) {}

template <typename T>
FORCE_INLINE uint64_t DataFrameVector<T>::Iterator::get_idx() const {
  return chunk_idx_ * kRealChunkNumEntries + chunk_offset_;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator &DataFrameVector<T>::Iterator::
operator++() {
  chunk_offset_++;
  if (unlikely(chunk_offset_ == kRealChunkNumEntries)) {
    chunk_idx_++;
    chunk_offset_ = 0;
  }
  return *this;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator DataFrameVector<T>::Iterator::
operator++(int) {
  Iterator retval = *this;
  ++(*this);
  return retval;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator &DataFrameVector<T>::Iterator::
operator+=(difference_type dis) {
  auto flat_idx = get_idx() + dis;
  chunk_idx_ = flat_idx / kRealChunkNumEntries;
  chunk_offset_ = flat_idx % kRealChunkNumEntries;
  return *this;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator DataFrameVector<T>::Iterator::
operator+(difference_type dis) const {
  auto ret = *this;
  ret.operator+=(dis);
  return ret;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator &DataFrameVector<T>::Iterator::
operator--() {
  if (unlikely(chunk_offset_ == 0)) {
    chunk_idx_--;
    chunk_offset_ = kRealChunkNumEntries;
  }
  chunk_offset_--;
  return *this;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator DataFrameVector<T>::Iterator::
operator--(int) {
  Iterator retval = *this;
  --(*this);
  return retval;
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator==(const Iterator &other) const {
  return chunk_idx_ == other.chunk_idx_ && chunk_offset_ == other.chunk_offset_;
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator!=(const Iterator &other) const {
  return !(*this == other);
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator<(const Iterator &other) const {
  if (chunk_idx_ == other.chunk_idx_) {
    return chunk_offset_ < other.chunk_offset_;
  }
  return chunk_idx_ < other.chunk_idx_;
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator<=(const Iterator &other) const {
  if (chunk_idx_ == other.chunk_idx_) {
    return chunk_offset_ <= other.chunk_offset_;
  }
  return chunk_idx_ < other.chunk_idx_;
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator>(const Iterator &other) const {
  if (chunk_idx_ == other.chunk_idx_) {
    return chunk_offset_ > other.chunk_offset_;
  }
  return chunk_idx_ > other.chunk_idx_;
}

template <typename T>
FORCE_INLINE bool DataFrameVector<T>::Iterator::
operator>=(const Iterator &other) const {
  if (chunk_idx_ == other.chunk_idx_) {
    return chunk_offset_ >= other.chunk_offset_;
  }
  return chunk_idx_ > other.chunk_idx_;
}

template <typename T>
FORCE_INLINE T DataFrameVector<T>::Iterator::operator*() const {
  assert(dataframe_vec_->chunk_ptrs_.size() > chunk_idx_);
  DerefScope scope;
  // operator*() does not accept any argument, so we always leave prefetch on.
  dataframe_vec_->prefetch_record(/* nt = */ false, chunk_idx_);
  auto *raw_ptr = dataframe_vec_->chunk_ptrs_[chunk_idx_].template deref(scope);
  return *(reinterpret_cast<const T *>(raw_ptr) + chunk_offset_);
}

template <typename T>
template <bool Mut>
FORCE_INLINE uint64_t DataFrameVector<T>::FastIterator<Mut>::get_idx() const {
  return (data_ptr_ - data_ptr_begin_) +
         (chunk_ptr_ - &(dataframe_vec_->chunk_ptrs_.front())) *
             kRealChunkNumEntries;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator::difference_type
DataFrameVector<T>::Iterator::operator-(const Iterator &other) const {
  return (static_cast<int64_t>(chunk_idx_) -
          static_cast<int64_t>(other.chunk_idx_)) *
             kRealChunkNumEntries +
         (static_cast<int64_t>(chunk_offset_) -
          static_cast<int64_t>(other.chunk_offset_));
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE void DataFrameVector<T>::FastIterator<Mut>::update_on_new_chunk() {
  if (likely(chunk_ptr_ <= &dataframe_vec_->chunk_ptrs_.back() &&
             chunk_ptr_ >= &dataframe_vec_->chunk_ptrs_.front())) {
    dataframe_vec_->prefetcher_->add_trace(
        Nt, chunk_ptr_ - &(dataframe_vec_->chunk_ptrs_.front()));
    if constexpr (Mut) {
      data_ptr_begin_ =
          reinterpret_cast<T *>(chunk_ptr_->deref_mut<Nt>(*scope_));
    } else {
      data_ptr_begin_ =
          reinterpret_cast<const T *>(chunk_ptr_->deref<Nt>(*scope_));
    }
    data_ptr_end_ = data_ptr_begin_ + kRealChunkNumEntries;
  } else {
    data_ptr_begin_ = data_ptr_end_ = nullptr;
  }
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::FastIterator<Mut>::FastIterator(
    DerefScope &scope,
    std::conditional<Mut, DataFrameVector *, const DataFrameVector *>::type
        dataframe_vec,
    uint64_t idx)
    : scope_(&scope),
      dataframe_vec_(const_cast<DataFrameVector *>(dataframe_vec)) {
  auto [chunk_idx, chunk_offset] = dataframe_vec_->get_chunk_stats(idx);
  chunk_ptr_ = &(dataframe_vec_->chunk_ptrs_[chunk_idx]);
  update_on_new_chunk<Nt>();
  data_ptr_ = data_ptr_begin_ + chunk_offset;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut> &
DataFrameVector<T>::FastIterator<Mut>::operator++() {
  data_ptr_++;
  if (very_unlikely(data_ptr_ == data_ptr_end_)) {
    chunk_ptr_++;
    update_on_new_chunk<Nt>();
    data_ptr_ = data_ptr_begin_;
  }
  return *this;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut>
DataFrameVector<T>::FastIterator<Mut>::operator++(int) {
  FastIterator retval = *this;
  this->operator++<Nt>();
  return retval;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut> &
DataFrameVector<T>::FastIterator<Mut>::operator--() {
  if (very_unlikely(data_ptr_ == data_ptr_begin_)) {
    chunk_ptr_--;
    update_on_new_chunk<Nt>();
    data_ptr_ = data_ptr_end_;
  }
  data_ptr_--;
  return *this;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut>
DataFrameVector<T>::FastIterator<Mut>::operator--(int) {
  FastIterator retval = *this;
  this->operator--<Nt>();
  return retval;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut> &
DataFrameVector<T>::FastIterator<Mut>::operator+=(difference_type dis) {
  *this = FastIterator<Mut>(*scope_, dataframe_vec_, dis + get_idx());
  return *this;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE DataFrameVector<T>::template FastIterator<Mut>
DataFrameVector<T>::FastIterator<Mut>::operator+(difference_type dis) const {
  auto ret = *this;
  ret.operator+=(dis);
  return ret;
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator==(const FastIterator<Mut> &other) const {
  return data_ptr_ == other.data_ptr_;
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator!=(const FastIterator<Mut> &other) const {
  return !(*this == other);
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator<(const FastIterator<Mut> &other) const {
  if (chunk_ptr_ == other.chunk_ptr_) {
    return data_ptr_ < other.data_ptr_;
  }
  return chunk_ptr_ < other.chunk_ptr_;
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator<=(const FastIterator<Mut> &other) const {
  if (chunk_ptr_ == other.chunk_ptr_) {
    return data_ptr_ <= other.data_ptr_;
  }
  return chunk_ptr_ < other.chunk_ptr_;
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator>(const FastIterator<Mut> &other) const {
  if (chunk_ptr_ == other.chunk_ptr_) {
    return data_ptr_ > other.data_ptr_;
  }
  return chunk_ptr_ > other.chunk_ptr_;
}

template <typename T>
template <bool Mut>
FORCE_INLINE bool DataFrameVector<T>::FastIterator<Mut>::
operator>=(const FastIterator<Mut> &other) const {
  if (chunk_ptr_ == other.chunk_ptr_) {
    return data_ptr_ >= other.data_ptr_;
  }
  return chunk_ptr_ > other.chunk_ptr_;
}

template <typename T>
template <bool Mut>
template <bool Nt>
FORCE_INLINE void
DataFrameVector<T>::FastIterator<Mut>::renew(DerefScope &scope) {
  auto offset = data_ptr_ - data_ptr_begin_;
  if constexpr (Mut) {
    data_ptr_begin_ =
        reinterpret_cast<T *>(chunk_ptr_->template deref_mut<Nt>(*scope_));
  } else {
    data_ptr_begin_ =
        reinterpret_cast<const T *>(chunk_ptr_->template deref<Nt>(*scope_));
  }
  data_ptr_ = data_ptr_begin_ + offset;
  data_ptr_end_ = data_ptr_begin_ + kRealChunkNumEntries;
}

template <typename T>
template <bool Mut>
FORCE_INLINE std::conditional<Mut, T &, const T &>::type
    DataFrameVector<T>::FastIterator<Mut>::operator*() const {
  return *data_ptr_;
}

template <typename T>
template <bool Mut>
FORCE_INLINE std::conditional<Mut, T *, const T *>::type
    DataFrameVector<T>::FastIterator<Mut>::operator->() const {
  return data_ptr_;
}

template <typename T>
FORCE_INLINE DataFrameVector<T> &DataFrameVector<T>::
operator=(const DataFrameVector &other) {
  assign(other.cbegin(), other.cend());
  return *this;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::DataFrameVector(DataFrameVector &&other)
    : GenericDataFrameVector(std::move(other.lock())),
      prefetcher_(std::move(other.prefetcher_)) {
  prefetcher_->update_state(reinterpret_cast<uint8_t *>(&lock_));
  other.lock_.unlock_writer();
}

template <typename T>
FORCE_INLINE DataFrameVector<T> &DataFrameVector<T>::
operator=(DataFrameVector &&other) {
  auto writer_lock = other.lock_.get_writer_lock();
  GenericDataFrameVector::operator=(std::move(other));
  prefetcher_ = std::move(other.prefetcher_);
  prefetcher_->update_state(reinterpret_cast<uint8_t *>(&lock_));
  return *this;
}

template <typename T> FORCE_INLINE DataFrameVector<T>::~DataFrameVector() {
  prefetcher_.reset();
}

template <typename T>
FORCE_INLINE uint64_t DataFrameVector<T>::capacity() const {
  return chunk_ptrs_.size() * kRealChunkNumEntries;
}

template <typename T>
FORCE_INLINE std::pair<uint64_t, uint64_t>
DataFrameVector<T>::get_chunk_stats(uint64_t index) {
  // It's superfast since chunk size is the power of 2.
  return std::make_pair(index / kRealChunkNumEntries,
                        index % kRealChunkNumEntries);
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::expand(uint64_t num) {
  GenericDataFrameVector::expand((num - 1) / kRealChunkNumEntries + 1);
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::expand_no_alloc(uint64_t num) {
  GenericDataFrameVector::expand_no_alloc(
      (num == 0) ? 0 : (num - 1) / kRealChunkNumEntries + 1);
}

template <typename T>
template <typename U, bool Nt>
FORCE_INLINE void DataFrameVector<T>::push_back(const DerefScope &scope,
                                                U &&u) {
  static_assert(std::is_same<std::decay_t<U>, std::decay_t<T>>::value,
                "U must be the same as T");
  auto [chunk_idx, chunk_offset] = get_chunk_stats(size_++);
  if (unlikely(chunk_ptrs_.size() == chunk_idx)) {
    expand(kNumEntriesPerExpansion);
  }
  assert(chunk_ptrs_.size() >= chunk_idx);
  auto *raw_mut_ptr = chunk_ptrs_[chunk_idx].template deref_mut<Nt>(scope);
  __builtin_memcpy(reinterpret_cast<T *>(raw_mut_ptr) + chunk_offset, &u,
                   sizeof(u));
  prefetch_record(Nt, chunk_idx);
  dirty_ = true;
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::pop_back(const DerefScope &scope) {
  size_--;
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::reserve(uint64_t count) {
  assert(!DerefScope::is_in_deref_scope());
  auto diff = static_cast<int64_t>(count) - static_cast<int64_t>(capacity());
  if (diff > 0) {
    expand(diff);
  }
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::resize(uint64_t count) {
  if (count > size_) {
    reserve(count);
    size_ = count;
  }
}

template <typename T>
FORCE_INLINE T &DataFrameVector<T>::front_mut(const DerefScope &scope) {
  return at_mut</* Prefetch = */ false>(scope, 0);
}

template <typename T>
FORCE_INLINE const T &DataFrameVector<T>::front(const DerefScope &scope) {
  return at</* Prefetch = */ false>(scope, 0);
}

template <typename T>
FORCE_INLINE T &DataFrameVector<T>::back_mut(const DerefScope &scope) {
  return at_mut</* Prefetch = */ false>(scope, size() - 1);
}

template <typename T>
FORCE_INLINE const T &DataFrameVector<T>::back(const DerefScope &scope) {
  return at</* Prefetch = */ false>(scope, size() - 1);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator DataFrameVector<T>::begin() {
  return Iterator(this, 0, 0);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::Iterator DataFrameVector<T>::end() {
  auto pair = get_chunk_stats(size_);
  return Iterator(this, pair.first, pair.second);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::template FastIterator<true>
DataFrameVector<T>::fbegin(DerefScope &scope) {
  dirty_ = true;
  return FastIterator<true>(scope, this, 0);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::template FastIterator<true>
DataFrameVector<T>::fend(DerefScope &scope) {
  dirty_ = true;
  return FastIterator<true>(scope, this, size());
}

template <typename T>
FORCE_INLINE const DataFrameVector<T>::Iterator
DataFrameVector<T>::cbegin() const {
  return const_cast<DataFrameVector<T> *>(this)->begin();
}

template <typename T>
FORCE_INLINE const DataFrameVector<T>::Iterator
DataFrameVector<T>::cend() const {
  return const_cast<DataFrameVector<T> *>(this)->end();
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::template FastIterator<false>
DataFrameVector<T>::cfbegin(DerefScope &scope) const {
  return FastIterator<false>(scope, this, 0);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>::template FastIterator<false>
DataFrameVector<T>::cfend(DerefScope &scope) const {
  return FastIterator<false>(scope, this, size());
}

template <typename T>
FORCE_INLINE void DataFrameVector<T>::prefetch_record(bool nt, Index_t idx) {
  if (unlikely(last_idx_ != idx)) {
    if (ACCESS_ONCE(dynamic_prefetch_enabled_)) {
      prefetcher_->add_trace(nt, idx);
    }
    last_idx_ = idx;
  }
}

template <typename T>
template <bool Prefetch, bool Nt>
FORCE_INLINE T &DataFrameVector<T>::at_mut(const DerefScope &scope,
                                           uint64_t index) {
  auto [chunk_idx, chunk_offset] = get_chunk_stats(index);
  assert(chunk_ptrs_.size() > chunk_idx);
  if constexpr (Prefetch) {
    prefetch_record(Nt, chunk_idx);
  }
  dirty_ = true;
  auto *raw_mut_ptr = chunk_ptrs_[chunk_idx].template deref_mut<Nt>(scope);
  return *(reinterpret_cast<T *>(raw_mut_ptr) + chunk_offset);
}

template <typename T>
template <bool Prefetch, bool Nt>
FORCE_INLINE const T &DataFrameVector<T>::at(const DerefScope &scope,
                                             uint64_t index) {
  auto [chunk_idx, chunk_offset] = get_chunk_stats(index);
  assert(chunk_ptrs_.size() > chunk_idx);
  if constexpr (Prefetch) {
    prefetch_record(Nt, chunk_idx);
  }
  auto *raw_ptr = chunk_ptrs_[chunk_idx].template deref<Nt>(scope);
  return *(reinterpret_cast<const T *>(raw_ptr) + chunk_offset);
}

template <typename T>
FORCE_INLINE T DataFrameVector<T>::_nth_element(uint64_t begin, uint64_t len,
                                                uint64_t n) {
  uint64_t idx_pivot, idx_j;
  {
    DerefScope scope;
    std::swap(at_mut(scope, begin + len - 1),
              at_mut(scope, rand() % len + begin));
    auto it_i = FastIterator<true>(scope, this, begin);
    auto it_pivot = it_i;
    auto it_j = FastIterator<true>(scope, this, begin + len - 1);
    auto key = *it_j;
    auto cnt = 0;
    while (it_i != it_j) {
      if (unlikely(cnt++ % kNumElementsPerScope == 0)) {
        scope.renew();
        it_i.renew(scope);
        it_j.renew(scope);
        it_pivot.renew(scope);
      }
      if (*it_i > key) {
        ++it_i;
      } else if (*it_i < key) {
        std::swap(*it_i, *it_pivot);
        ++it_i, ++it_pivot;
      } else {
        std::swap(*it_i, *(--it_j));
      }
    }

    idx_pivot = it_pivot.get_idx() - begin;
    idx_j = it_j.get_idx() - begin;

    if (n >= idx_pivot && n <= idx_pivot + len - 1 - idx_j) {
      return key;
    }
  }
  return idx_pivot > n ? _nth_element(begin, idx_pivot, n)
                       : _nth_element(begin + idx_pivot, idx_j - idx_pivot,
                                      n - idx_pivot - (len - idx_j));
}

template <typename T>
FORCE_INLINE T DataFrameVector<T>::nth_element(uint64_t n) {
  return _nth_element(0, size_, n);
}

template <typename T>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::get_col_unique_values(FarMemManager *manager) {
  if constexpr (DISABLE_OFFLOAD_UNIQUE) {
    return get_col_unique_values_locally(manager);
  } else {
    return get_col_unique_values_remotely(manager);
  }
}

template <typename T>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::get_col_unique_values_locally(FarMemManager *manager) {
  auto unique_dataframe_vec = DataFrameVector<T>(manager);
  std::unordered_set<T> hashset;
  uint64_t i;
  {
    DerefScope scope;
    auto it = cfbegin(scope);
    for (i = 0; i < size_; ++i, ++it) {
      if (unlikely(i % kNumElementsPerScope == 0)) {
        scope.renew();
        it.renew(scope);
      }
      hashset.emplace(*it);
    }
  }
  unique_dataframe_vec.resize(hashset.size());
  {
    DerefScope scope;
    auto unique_it = unique_dataframe_vec.fbegin(scope);
    i = 0;
    for (const auto &unique_data : hashset) {
      if (unlikely(i % kNumElementsPerScope == 0)) {
        scope.renew();
        unique_it.renew(scope);
      }
      *unique_it = unique_data;
      ++unique_it, ++i;
    }
  }
  return unique_dataframe_vec;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::get_col_unique_values_remotely(FarMemManager *manager) {
  flush();
  auto unique_dataframe_vec = DataFrameVector<T>(manager);
  uint16_t input_len;
  uint8_t input_data[sizeof(ds_id_) + sizeof(size_)];
  input_len = sizeof(input_data);
  __builtin_memcpy(input_data, &unique_dataframe_vec.ds_id_, sizeof(ds_id_));
  __builtin_memcpy(input_data + sizeof(ds_id_), &size_, sizeof(size_));
  uint16_t output_len;
  uint64_t output_data[2];
  device_->compute(ds_id_, OpCode::Unique, input_len, input_data, &output_len,
                   reinterpret_cast<uint8_t *>(output_data));
  assert(output_len == sizeof(output_data));
  unique_dataframe_vec.size_ = output_data[0];
  unique_dataframe_vec.remote_vec_capacity_ = output_data[1];
  unique_dataframe_vec.expand_no_alloc(
      unique_dataframe_vec.remote_vec_capacity_);
  return unique_dataframe_vec;
}

template <typename T>
FORCE_INLINE DataFrameVector<T> DataFrameVector<T>::copy_data_by_idx(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  if constexpr (DISABLE_OFFLOAD_COPY_DATA_BY_IDX) {
    return copy_data_by_idx_locally(manager, idx_vec);
  } else {
    return copy_data_by_idx_remotely(manager, idx_vec);
  }
}

template <typename T>
FORCE_INLINE DataFrameVector<T> DataFrameVector<T>::copy_data_by_idx_locally(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  auto ret = DataFrameVector<T>(manager);
  ret.resize(idx_vec.size_);
  DerefScope scope;
  auto to_it = ret.fbegin(scope);
  auto idx_it = idx_vec.cfbegin(scope);

  for (uint64_t i = 0; i < idx_vec.size_; ++i, ++to_it, ++idx_it) {
    if (unlikely(i % kNumElementsPerScope == 0)) {
      scope.renew();
      to_it.renew(scope);
      idx_it.renew(scope);
    }
    *to_it = at(scope, *idx_it);
  }

  return ret;
}

template <typename T>
FORCE_INLINE DataFrameVector<T> DataFrameVector<T>::copy_data_by_idx_remotely(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  idx_vec.flush();
  flush();
  auto ret = DataFrameVector<T>(manager);
  uint64_t idx_vec_size = idx_vec.size();
  uint8_t input_data[sizeof(ret.ds_id_) + sizeof(idx_vec.ds_id_) +
                     sizeof(idx_vec_size)];
  uint16_t input_len = sizeof(input_data);
  __builtin_memcpy(input_data, &ret.ds_id_, sizeof(ret.ds_id_));
  __builtin_memcpy(input_data + sizeof(ret.ds_id_), &idx_vec.ds_id_,
                   sizeof(idx_vec.ds_id_));
  __builtin_memcpy(input_data + sizeof(ret.ds_id_) + sizeof(idx_vec.ds_id_),
                   &idx_vec_size, sizeof(idx_vec_size));
  uint16_t output_len;
  device_->compute(ds_id_, OpCode::CopyDataByIdx, input_len, input_data,
                   &output_len,
                   reinterpret_cast<uint8_t *>(&ret.remote_vec_capacity_));
  assert(output_len == sizeof(remote_vec_capacity_));
  ret.size_ = idx_vec.size();
  ret.expand_no_alloc(ret.remote_vec_capacity_);
  return ret;
}

template <typename T>
FORCE_INLINE DataFrameVector<T> DataFrameVector<T>::shuffle_data_by_idx(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  if constexpr (DISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX) {
    return shuffle_data_by_idx_locally(manager, idx_vec);
  } else {
    return shuffle_data_by_idx_remotely(manager, idx_vec);
  }
}

template <typename T>
FORCE_INLINE DataFrameVector<T> DataFrameVector<T>::shuffle_data_by_idx_locally(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  auto ret = DataFrameVector<T>(manager);
  ret.resize(idx_vec.size_);
  DerefScope scope;
  auto from_it = cfbegin(scope);
  auto idx_it = idx_vec.cfbegin(scope);

  for (uint64_t i = 0; i < idx_vec.size_; ++i, ++from_it, ++idx_it) {
    if (unlikely(i % kNumElementsPerScope == 0)) {
      scope.renew();
      from_it.renew(scope);
      idx_it.renew(scope);
    }
    ret.at_mut(scope, *idx_it) = *from_it;
  }

  return ret;
}

template <typename T>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::shuffle_data_by_idx_remotely(
    FarMemManager *manager, DataFrameVector<unsigned long long> &idx_vec) {
  idx_vec.flush();
  flush();
  auto ret = DataFrameVector<T>(manager);
  uint64_t idx_vec_size = idx_vec.size();
  uint8_t input_data[sizeof(ret.ds_id_) + sizeof(idx_vec.ds_id_) +
                     sizeof(idx_vec_size)];
  uint16_t input_len = sizeof(input_data);
  __builtin_memcpy(input_data, &ret.ds_id_, sizeof(ret.ds_id_));
  __builtin_memcpy(input_data + sizeof(ret.ds_id_), &idx_vec.ds_id_,
                   sizeof(idx_vec.ds_id_));
  __builtin_memcpy(input_data + sizeof(ret.ds_id_) + sizeof(idx_vec.ds_id_),
                   &idx_vec_size, sizeof(idx_vec_size));
  uint16_t output_len;
  device_->compute(ds_id_, OpCode::ShuffleDataByIdx, input_len, input_data,
                   &output_len,
                   reinterpret_cast<uint8_t *>(&ret.remote_vec_capacity_));
  assert(output_len == sizeof(remote_vec_capacity_));
  ret.size_ = idx_vec.size();
  ret.expand_no_alloc(ret.remote_vec_capacity_);
  return ret;
}

template <typename T>
FORCE_INLINE void
DataFrameVector<T>::assign(const DataFrameVector<T>::Iterator &begin,
                           const DataFrameVector<T>::Iterator &end) {
  if constexpr (DISABLE_OFFLOAD_ASSIGN) {
    assign_locally(begin, end);
  } else {
    assign_remotely(begin, end);
  }
}

template <typename T>
FORCE_INLINE void
DataFrameVector<T>::assign_locally(const DataFrameVector<T>::Iterator &begin,
                                   const DataFrameVector<T>::Iterator &end) {
  auto begin_flat_idx = begin.get_idx();
  auto end_flat_idx = end.get_idx();
  auto size = end_flat_idx - begin_flat_idx;
  resize(size);
  DerefScope scope;
  auto from_it =
      FastIterator<false>(scope, begin.dataframe_vec_, begin_flat_idx);
  auto to_it = fbegin(scope);
  for (uint64_t i = 0; i < size; ++i, ++from_it, ++to_it) {
    if (unlikely(i % kNumElementsPerScope == 0)) {
      scope.renew();
      from_it.renew(scope);
      to_it.renew(scope);
    }
    *to_it = *from_it;
  }
}

template <typename T>
FORCE_INLINE void
DataFrameVector<T>::assign_remotely(const DataFrameVector<T>::Iterator &begin,
                                    const DataFrameVector<T>::Iterator &end) {
  begin.dataframe_vec_->flush();
  auto begin_flat_idx = begin.get_idx();
  auto end_flat_idx = end.get_idx();
  uint8_t input_data[sizeof(ds_id_) + sizeof(begin_flat_idx) +
                     sizeof(end_flat_idx)];
  uint16_t input_len = sizeof(input_data);
  __builtin_memcpy(input_data, &begin.dataframe_vec_->ds_id_, sizeof(ds_id_));
  __builtin_memcpy(input_data + sizeof(ds_id_), &begin_flat_idx,
                   sizeof(begin_flat_idx));
  __builtin_memcpy(input_data + sizeof(ds_id_) + sizeof(begin_flat_idx),
                   &end_flat_idx, sizeof(end_flat_idx));
  uint16_t output_len;
  device_->compute(ds_id_, OpCode::Assign, input_len, input_data, &output_len,
                   reinterpret_cast<uint8_t *>(&remote_vec_capacity_));
  size_ = end - begin;
  expand_no_alloc(remote_vec_capacity_);
}

template <typename T>
template <bool Ascending>
FORCE_INLINE DataFrameVector<unsigned long long>
DataFrameVector<T>::get_sorted_indices(FarMemManager *manager,
                                       bool already_sorted_asc) {
  assert(!DerefScope::is_in_deref_scope());
  auto indices = DataFrameVector<unsigned long long>(manager);
  if (already_sorted_asc) {
    DerefScope scope;
    constexpr uint64_t kNumElementsPerScope = 1024;
    for (unsigned long long i = 0; i < size_; i++) {
      if (unlikely(i % kNumElementsPerScope == 0)) {
        scope.renew();
      }
      auto idx = Ascending ? i : size_ - i - 1;
      indices.push_back(scope, idx);
    }
  }
  if constexpr (sizeof(T) <= 2 && std::is_integral<T>::value) {
    // T is small. Use counting sort.
    _get_sorted_indices_counting_sort<Ascending>(&indices);
  } else {
    // T is large. Use radix sort which iteratively invokes counting on its
    // digits. Not implemented yet.
    BUG();
  }
  return indices;
}

template <typename T>
template <bool Ascending>
FORCE_INLINE void DataFrameVector<T>::_get_sorted_indices_counting_sort(
    DataFrameVector<unsigned long long> *indices) {
  indices->resize(size_);
  int64_t T_min = std::numeric_limits<T>::min();
  int64_t T_max = std::numeric_limits<T>::max();
  preempt_disable();
  auto cnts = std::make_unique<uint64_t[]>(T_max - T_min + 1);
  preempt_enable();
  memset(cnts.get(), 0, sizeof(uint64_t) * (T_max - T_min + 1));

  DerefScope scope;
  auto it = cfbegin(scope);
  for (uint64_t i = 0; i < size_; ++i, ++it) {
    if (unlikely(i % kNumElementsPerScope == 0)) {
      scope.renew();
      it.renew(scope);
    }
    cnts[*it - T_min]++;
  }
  for (int64_t i = 1; i < T_max - T_min + 1; i++) {
    cnts[i] += cnts[i - 1];
  }
  it = cfend(scope);
  auto idx_it = indices->fend(scope);
  for (int64_t i = static_cast<int64_t>(size_) - 1; i >= 0; --i) {
    --idx_it, --it;
    if (unlikely(i % kNumElementsPerScope == 0)) {
      scope.renew();
      it.renew(scope);
    }
    auto idx = --cnts[*it - T_min];
    if (!Ascending) {
      idx = size_ - idx - 1;
    }
    *idx_it = idx;
  }
}

template <typename T>
template <typename U>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::aggregate_remotely(FarMemManager *manager, const U &key_vec,
                                       OpCode opcode) {
  const_cast<U *>(&key_vec)->flush();
  assert(size() == key_vec.size());
  auto result = DataFrameVector<T>(manager);
  flush();
  uint16_t input_len;
  uint8_t key_vec_type_id = get_dataframe_type_id<typename U::value_type>();
  uint8_t input_data[sizeof(result.ds_id_) + sizeof(key_vec.ds_id_) +
                     sizeof(size_) + sizeof(key_vec_type_id)];
  input_len = sizeof(input_data);
  __builtin_memcpy(input_data, &result.ds_id_, sizeof(result.ds_id_));
  __builtin_memcpy(input_data + sizeof(result.ds_id_), &key_vec.ds_id_,
                   sizeof(key_vec.ds_id_));
  __builtin_memcpy(input_data + sizeof(result.ds_id_) + sizeof(key_vec.ds_id_),
                   &size_, sizeof(size_));
  __builtin_memcpy(input_data + sizeof(result.ds_id_) + sizeof(key_vec.ds_id_) +
                       sizeof(size_),
                   &key_vec_type_id, sizeof(key_vec_type_id));
  uint16_t output_len;
  uint64_t output_data[2];
  device_->compute(ds_id_, opcode, input_len, input_data, &output_len,
                   reinterpret_cast<uint8_t *>(output_data));
  assert(output_len == sizeof(output_data));
  result.size_ = output_data[0];
  result.remote_vec_capacity_ = output_data[1];
  result.expand_no_alloc(result.remote_vec_capacity_);
  return result;
}

template <typename T>
template <typename U>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::aggregate_min(FarMemManager *manager, const U &key_vec) {
  if constexpr (DISABLE_OFFLOAD_AGGREGATE) {
    return aggregate_locally(manager, key_vec, AggregateMin);
  } else {
    return aggregate_remotely(manager, key_vec, AggregateMin);
  }
}

template <typename T>
template <typename U>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::aggregate_max(FarMemManager *manager, const U &key_vec) {
  if constexpr (DISABLE_OFFLOAD_AGGREGATE) {
    return aggregate_locally(manager, key_vec, AggregateMax);
  } else {
    return aggregate_remotely(manager, key_vec, AggregateMax);
  }
}

template <typename T>
template <typename U>
FORCE_INLINE DataFrameVector<T>
DataFrameVector<T>::aggregate_median(FarMemManager *manager, const U &key_vec) {
  if constexpr (DISABLE_OFFLOAD_AGGREGATE) {
    return aggregate_locally(manager, key_vec, AggregateMedian);
  } else {
    return aggregate_remotely(manager, key_vec, AggregateMedian);
  }
}

template <typename T>
FORCE_INLINE DataFrameVector<T> &DataFrameVector<T>::lock() {
  lock_.lock_writer();
  return *this;
}

template <typename T> FORCE_INLINE void DataFrameVector<T>::disable_prefetch() {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = false;
}

template <typename T> FORCE_INLINE void DataFrameVector<T>::enable_prefetch() {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = true;
}

template <typename T>
FORCE_INLINE void
DataFrameVector<T>::static_prefetch(DataFrameVector<T>::Index_t start,
                                    DataFrameVector<T>::Index_t step,
                                    uint32_t num) {
  ACCESS_ONCE(dynamic_prefetch_enabled_) = false;
  prefetcher_->static_prefetch(start, step, num);
}

} // namespace far_memory
