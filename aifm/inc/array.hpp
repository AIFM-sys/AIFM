#pragma once

#include "deref_scope.hpp"
#include "pointer.hpp"
#include "prefetcher.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>

namespace far_memory {

class FarMemManager;

class GenericArray {
protected:
  using Index_t = uint64_t;
  using Pattern_t = int64_t;

  static Pattern_t induce_fn(Index_t idx_0, Index_t idx_1);
  static Index_t infer_fn(Index_t idx, Pattern_t stride);
  static GenericUniquePtr *mapping_fn(uint8_t *&state, Index_t idx);
  std::unique_ptr<GenericUniquePtr[]> ptrs_;
  uint64_t kNumItems_;
  uint32_t kItemSize_;
  bool dynamic_prefetch_enabled_ = true;
  constexpr static auto kInduceFn = [](Index_t idx_0,
                                       Index_t idx_1) -> Pattern_t {
    return GenericArray::induce_fn(idx_0, idx_1);
  };
  constexpr static auto kInferFn = [](Index_t idx,
                                      Pattern_t stride) -> Index_t {
    return infer_fn(idx, stride);
  };
  constexpr static auto kMappingFn = [](uint8_t *&state,
                                        Index_t idx) -> GenericUniquePtr * {
    return mapping_fn(state, idx);
  };
  Prefetcher<decltype(kInduceFn), decltype(kInferFn), decltype(kMappingFn)>
      prefetcher_;

  GenericArray(FarMemManager *manager, uint32_t item_size, uint64_t num_items);
  ~GenericArray();
  NOT_COPYABLE(GenericArray);
  NOT_MOVEABLE(GenericArray);

public:
  void disable_prefetch();
  void enable_prefetch();
  void static_prefetch(Index_t start, Index_t step, uint32_t num);
  GenericUniquePtr *at(bool nt, Index_t idx);
};

template <typename T, uint64_t... Dims> class Array : public GenericArray {
private:
  using num_dim_t = uint8_t;
  static_assert(std::numeric_limits<num_dim_t>::max() >= sizeof...(Dims));

  friend class FarMemManager;
  friend class FarMemTest;

  Array(FarMemManager *manager);
  NOT_COPYABLE(Array);
  NOT_MOVEABLE(Array);

  template <auto DimIdx> static constexpr uint64_t get_dim_size();

  static constexpr uint64_t _size(uint64_t N) { return N; }

  template <typename... Args>
  static constexpr uint64_t _size(uint64_t N, Args... rest_dims) {
    return N * _size(rest_dims...);
  }

public:
  static constexpr uint64_t kSize = _size(Dims...);

  template <auto DimIdx, typename... Indices>
  static constexpr int64_t _get_flat_idx(Indices... indices);
  template <typename... Indices>
  static constexpr int64_t get_flat_idx(Indices... indices);
  template <typename... Indices> void check_indices(Indices... indices);
  template <bool Nt = false, typename... Indices>
  const T &at(const DerefScope &scope, Indices... indices) noexcept;
  template <bool Nt = false, typename... Indices>
  const T &at_safe(const DerefScope &scope, Indices... indices);
  template <bool Nt = false, typename... Indices> T read(Indices... indices);
  template <bool Nt = false, typename... Indices>
  T read_safe(Indices... indices);
  template <bool Nt = false, typename... Indices>
  T &at_mut(const DerefScope &scope, Indices... indices) noexcept;
  template <bool Nt = false, typename... Indices>
  T &at_mut_safe(const DerefScope &scope, Indices... indices);
  template <bool Nt = false, typename U, typename... Indices>
  void write(U &&u, Indices... indices);
  template <bool Nt = false, typename U, typename... Indices>
  void write_safe(U &&u, Indices... indices);
  template <typename... ArgsStart, typename... ArgsStep>
  void static_prefetch(std::tuple<ArgsStart...> start,
                       std::tuple<ArgsStep...> step, uint32_t num);
};

} // namespace far_memory

#include "internal/array.ipp"
