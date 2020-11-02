#pragma once

#include "dataframe_vector.hpp"
#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "internal/dataframe_types.hpp"
#include "pointer.hpp"
#include "prefetcher.hpp"
#include "reader_writer_lock.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#ifdef DISABLE_OFFLOAD_UNIQUE
#define DISABLE_OFFLOAD_UNIQUE 1
#else
#define DISABLE_OFFLOAD_UNIQUE 0
#endif

#ifdef DISABLE_OFFLOAD_COPY_DATA_BY_IDX
#define DISABLE_OFFLOAD_COPY_DATA_BY_IDX 1
#else
#define DISABLE_OFFLOAD_COPY_DATA_BY_IDX 0
#endif

#ifdef DISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX
#define DISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX 1
#else
#define DISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX 0
#endif

#ifdef DISABLE_OFFLOAD_ASSIGN
#define DISABLE_OFFLOAD_ASSIGN 1
#else
#define DISABLE_OFFLOAD_ASSIGN 0
#endif

#ifdef DISABLE_OFFLOAD_AGGREGATE
#define DISABLE_OFFLOAD_AGGREGATE 1
#else
#define DISABLE_OFFLOAD_AGGREGATE 0
#endif

#define DISABLE_OFFLOAD                                                        \
  (DISABLE_OFFLOAD_UNIQUE & DISABLE_OFFLOAD_COPY_DATA_BY_IDX &                 \
   DISABLE_OFFLOAD_SHUFFLE_DATA_BY_IDX & DISABLE_OFFLOAD_ASSIGN &              \
   DISABLE_OFFLOAD_AGGREGATE)

namespace far_memory {

class FarMemManager;

class GenericDataFrameVector {
private:
  enum OpCode {
    Reserve = 0,
    Unique,
    CopyDataByIdx,
    ShuffleDataByIdx,
    Assign,
    AggregateMax,
    AggregateMin,
    AggregateMedian
  };

  uint32_t chunk_size_;
  uint32_t chunk_num_entries_;
  FarMemDevice *device_;
  uint8_t ds_id_;
  uint64_t size_ = 0;
  uint64_t remote_vec_capacity_ = 0;
  ReaderWriterLock lock_;
  std::vector<GenericUniquePtr> chunk_ptrs_;
  bool moved_ = false;
  bool dirty_ = false;
  uint64_t last_idx_ = std::numeric_limits<uint64_t>::max();
  template <typename T> friend class DataFrameVector;
  template <typename T> friend class ServerDataFrameVector;

  void expand(uint64_t num);
  void expand_no_alloc(uint64_t num);
  void reserve_remote(uint64_t num);
  void cleanup();

public:
  GenericDataFrameVector(const uint32_t chunk_size, uint32_t chunk_num_entries,
                         uint8_t ds_id, uint8_t dt_id);
  NOT_COPYABLE(GenericDataFrameVector);
  GenericDataFrameVector(GenericDataFrameVector &&other);
  GenericDataFrameVector &operator=(GenericDataFrameVector &&other);
  ~GenericDataFrameVector();

  bool empty() const;
  uint64_t size() const;
  void clear();
  void flush();
};

template <typename T> class DataFrameVector : public GenericDataFrameVector {
private:
  static_assert(is_basic_dataframe_types<T>());
  using Index_t = uint64_t;
  using Pattern_t = int64_t;

  constexpr static uint32_t kPreferredChunkSize = 4096;
  constexpr static uint32_t kRealChunkNumEntries =
      std::max(static_cast<uint32_t>(1),
               helpers::round_up_power_of_two(kPreferredChunkSize / sizeof(T)));
  constexpr static uint32_t kRealChunkSize = sizeof(T) * kRealChunkNumEntries;
  constexpr static uint32_t kSizePerExpansion = 4 << 20; // 4 MiB.
  constexpr static uint32_t kNumEntriesPerExpansion =
      (kSizePerExpansion - 1) / sizeof(T) + 1;
  constexpr static uint64_t kNumElementsPerScope = 1024;

  static Pattern_t induce_fn(Index_t idx_0, Index_t idx_1);
  static Index_t infer_fn(Index_t idx, Pattern_t stride);
  static GenericUniquePtr *mapping_fn(uint8_t *&state, Index_t idx);
  constexpr static auto kInduceFn = [](Index_t idx_0,
                                       Index_t idx_1) -> Pattern_t {
    return induce_fn(idx_0, idx_1);
  };
  constexpr static auto kInferFn = [](Index_t idx,
                                      Pattern_t stride) -> Index_t {
    return infer_fn(idx, stride);
  };
  constexpr static auto kMappingFn = [](uint8_t *&state,
                                        Index_t idx) -> GenericUniquePtr * {
    return mapping_fn(state, idx);
  };
  std::unique_ptr<
      Prefetcher<decltype(kInduceFn), decltype(kInferFn), decltype(kMappingFn)>>
      prefetcher_;
  bool dynamic_prefetch_enabled_ = true;  

  friend class FarMemTest;
  template <typename U> friend class ServerDataFrameVector;

  // STL compatible, but slower (since it takes GC sync overhead per
  // object access).
  class Iterator {
  private:
    DataFrameVector *dataframe_vec_;
    uint64_t chunk_idx_;
    uint64_t chunk_offset_;
    friend class DataFrameVector;

    Iterator(DataFrameVector<T> *dataframe_vec, uint64_t chunk_idx,
             uint64_t chunk_offset);
    uint64_t get_idx() const;

  public:
    using difference_type = int64_t;
    using value_type = T;
    using pointer = const T *;
    using reference = const T &;
    using iterator_category = std::random_access_iterator_tag;

    Iterator &operator++();
    Iterator operator++(int);
    Iterator &operator+=(difference_type dis);
    Iterator operator+(difference_type dis) const;
    Iterator &operator--();
    Iterator operator--(int);
    difference_type operator-(const Iterator &other) const;
    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;
    bool operator<(const Iterator &other) const;
    bool operator<=(const Iterator &other) const;
    bool operator>(const Iterator &other) const;
    bool operator>=(const Iterator &other) const;
    T operator*() const;
  };

  // Not STL compatible, but faster. Its lifetime is bound to the DerefScope
  // that is used to create the iterator. It amortizes the GC sync overhead with
  // the chunk size.
  template <bool Mut> class FastIterator {
  private:
    DerefScope *scope_;
    GenericUniquePtr *chunk_ptr_;
    DataFrameVector *dataframe_vec_;
    std::conditional<Mut, T *, const T *>::type data_ptr_;
    std::conditional<Mut, T *, const T *>::type data_ptr_begin_;
    std::conditional<Mut, T *, const T *>::type data_ptr_end_;
    template <bool Nt = false>
    FastIterator(DerefScope &scope,
                 std::conditional<Mut, DataFrameVector *,
                                  const DataFrameVector *>::type dataframe_vec,
                 uint64_t idx);
    friend class DataFrameVector;

    uint64_t get_idx() const;
    template <bool Nt> void update_on_new_chunk();

  public:
    using difference_type = int64_t;

    template <bool Nt = false> FastIterator<Mut> &operator++();
    template <bool Nt = false> FastIterator<Mut> operator++(int);
    template <bool Nt = false> FastIterator<Mut> &operator--();
    template <bool Nt = false> FastIterator<Mut> operator--(int);
    template <bool Nt = false>
    FastIterator<Mut> &operator+=(difference_type dis);
    template <bool Nt = false>
    FastIterator<Mut> operator+(difference_type dis) const;
    bool operator==(const FastIterator &other) const;
    bool operator!=(const FastIterator &other) const;
    bool operator<(const FastIterator &other) const;
    bool operator<=(const FastIterator &other) const;
    bool operator>(const FastIterator &other) const;
    bool operator>=(const FastIterator &other) const;
    // Renew the iterator. Its lifetime is bound to the argument scope.
    template <bool Nt = false> void renew(DerefScope &scope);
    std::conditional<Mut, T &, const T &>::type operator*() const;
    std::conditional<Mut, T *, const T *>::type operator->() const;
  };

  std::pair<uint64_t, uint64_t> get_chunk_stats(uint64_t index);
  void expand(uint64_t num);
  void expand_no_alloc(uint64_t num);
  void prefetch_record(bool nt, Index_t idx);
  DataFrameVector &lock();
  template <bool Ascending = true>
  void _get_sorted_indices_counting_sort(
      DataFrameVector<unsigned long long> *indices);
  template <typename U>
  DataFrameVector<T> aggregate_locally(FarMemManager *manager, const U &key_vec,
                                       OpCode opcode);
  template <typename U>
  DataFrameVector<T> aggregate_remotely(FarMemManager *manager,
                                        const U &key_vec, OpCode opcode);
  DataFrameVector<T> get_col_unique_values_locally(FarMemManager *manager);
  DataFrameVector<T> get_col_unique_values_remotely(FarMemManager *manager);
  DataFrameVector<T>
  copy_data_by_idx_locally(FarMemManager *manager,
                           DataFrameVector<unsigned long long> &idx_vec);
  DataFrameVector<T>
  copy_data_by_idx_remotely(FarMemManager *manager,
                            DataFrameVector<unsigned long long> &idx_vec);
  DataFrameVector<T>
  shuffle_data_by_idx_locally(FarMemManager *manager,
                              DataFrameVector<unsigned long long> &idx_vec);
  DataFrameVector<T>
  shuffle_data_by_idx_remotely(FarMemManager *manager,
                               DataFrameVector<unsigned long long> &idx_vec);
  void assign_locally(const Iterator &begin, const Iterator &end);
  void assign_remotely(const Iterator &begin, const Iterator &end);
  T _nth_element(uint64_t begin, uint64_t len, uint64_t n);

public:
  using value_type = T;

  DataFrameVector(FarMemManager *manager);
  // Copy constructor is not allowed since the new instance has to acquire a
  // new ds_id.
  DataFrameVector(const DataFrameVector &other);
  DataFrameVector &operator=(const DataFrameVector &other);
  DataFrameVector(DataFrameVector &&other);
  DataFrameVector &operator=(DataFrameVector &&other);
  ~DataFrameVector();

  uint64_t capacity() const;
  template <typename U, bool Nt = false>
  void push_back(const DerefScope &scope, U &&u);
  void pop_back(const DerefScope &scope);
  void reserve(uint64_t count);
  void resize(uint64_t count);
  T &front_mut(const DerefScope &scope);
  const T &front(const DerefScope &scope);
  T &back_mut(const DerefScope &scope);
  const T &back(const DerefScope &scope);
  template <bool Prefetch = true, bool Nt = false>
  T &at_mut(const DerefScope &scope, uint64_t index);
  template <bool Prefetch = true, bool Nt = false>
  const T &at(const DerefScope &scope, uint64_t index);
  T nth_element(uint64_t index);
  Iterator begin();
  Iterator end();
  const Iterator cbegin() const;
  const Iterator cend() const;
  FastIterator</* Mut = */ true> fbegin(DerefScope &scope);
  FastIterator</* Mut = */ true> fend(DerefScope &scope);
  FastIterator</* Mut = */ false> cfbegin(DerefScope &scope) const;
  FastIterator</* Mut = */ false> cfend(DerefScope &scope) const;

  DataFrameVector<T> get_col_unique_values(FarMemManager *manager);
  DataFrameVector<T>
  copy_data_by_idx(FarMemManager *manager,
                   DataFrameVector<unsigned long long> &idx_vec);
  DataFrameVector<T>
  shuffle_data_by_idx(FarMemManager *manager,
                      DataFrameVector<unsigned long long> &idx_vec);
  void assign(const Iterator &begin, const Iterator &end);
  template <typename U>
  DataFrameVector<T> aggregate_min(FarMemManager *manager, const U &key_vec);
  template <typename U>
  DataFrameVector<T> aggregate_max(FarMemManager *manager, const U &key_vec);
  template <typename U>
  DataFrameVector<T> aggregate_median(FarMemManager *manager, const U &key_vec);
  void disable_prefetch();
  void enable_prefetch();
  void static_prefetch(Index_t start, Index_t step, uint32_t num);
  template <bool Ascending = true>
  DataFrameVector<unsigned long long>
  get_sorted_indices(FarMemManager *manager, bool already_sorted_asc);
};

} // namespace far_memory

#include "internal/dataframe_vector.ipp"
