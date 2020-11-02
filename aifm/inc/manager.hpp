#pragma once

#include "sync.h"

#include "array.hpp"
#include "cb.hpp"
#include "concurrent_hopscotch.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "internal/ds_info.hpp"
#include "list.hpp"
#include "obj_locker.hpp"
#include "parallel.hpp"
#include "pointer.hpp"
#include "queue.hpp"
#include "region.hpp"
#include "stack.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

namespace far_memory {

template <typename T> class DataFrameVector;

// A GCTask is an interval of (to be GCed) local region.
using GCTask = std::pair<uint64_t, uint64_t>;

class GCParallelizer : public Parallelizer<GCTask> {
public:
  std::vector<Region> *from_regions_;
  GCParallelizer(uint32_t num_slaves, uint32_t task_queues_depth,
                 std::vector<Region> *from_regions);
  void master_fn();
};

class GCParallelMarker : public GCParallelizer {
  void slave_fn(uint32_t tid);

public:
  GCParallelMarker(uint32_t num_slaves, uint32_t task_queues_depth,
                   std::vector<Region> *from_regions);
};

class GCParallelWriteBacker : public GCParallelizer {
  void slave_fn(uint32_t tid);

public:
  GCParallelWriteBacker(uint32_t num_slaves, uint32_t task_queues_depth,
                        std::vector<Region> *from_regions);
};

class FarMemManager {
private:
  constexpr static double kFreeCacheAlmostEmptyThresh = 0.03;
  constexpr static double kFreeCacheLowThresh = 0.12;
  constexpr static double kFreeCacheHighThresh = 0.22;
  constexpr static uint8_t kGCSlaveThreadTaskQueueDepth = 8;
  constexpr static uint32_t kMaxNumRegionsPerGCRound = 128;
  constexpr static double kMaxRatioRegionsPerGCRound = 0.1;
  constexpr static double kMinRatioRegionsPerGCRound = 0.03;

  class RegionManager {
  private:
    constexpr static double kPickRegionMaxRetryTimes = 3;

    std::unique_ptr<uint8_t> local_cache_ptr_;
    CircularBuffer<Region, false> free_regions_;
    CircularBuffer<Region, false> used_regions_;
    CircularBuffer<Region, false> nt_used_regions_;
    rt::Spin region_spin_;
    Region core_local_free_regions_[helpers::kNumCPUs];
    Region core_local_free_nt_regions_[helpers::kNumCPUs];
    friend class FarMemTest;

  public:
    RegionManager(uint64_t size, bool is_local);
    void push_free_region(Region &region);
    std::optional<Region> pop_used_region();
    bool try_refill_core_local_free_region(bool nt, Region *full_region);
    Region &core_local_free_region(bool nt);
    double get_free_region_ratio() const;
    uint32_t get_num_regions() const;
  };

  RegionManager cache_region_manager_;
  RegionManager far_mem_region_manager_;
  std::atomic<uint32_t> pending_gcs_{0};
  bool gc_master_spawned_;
  std::unique_ptr<FarMemDevice> device_ptr_;
  rt::CondVar mutator_cache_condvar_;
  rt::CondVar mutator_far_mem_condvar_;
  rt::Spin gc_lock_;
  GCParallelMarker parallel_marker_;
  GCParallelWriteBacker parallel_write_backer_;
  std::vector<Region> from_regions_{kMaxNumRegionsPerGCRound};
  int ksched_fd_;
  std::queue<uint8_t> available_ds_ids_;
  static ObjLocker obj_locker_;

  friend class FarMemTest;
  friend class FarMemManagerFactory;
  friend class GenericFarMemPtr;
  friend class FarMemPtrMeta;
  friend class GenericArray;
  friend class GCParallelWriteBacker;
  friend class DerefScope;
  friend class GenericDataFrameVector;
  friend class GenericConcurrentHopscotch;
  template <typename T> friend class DataFrameVector;

  FarMemManager(uint64_t cache_size, uint64_t far_mem_size,
                uint32_t num_gc_threads, FarMemDevice *device);
  bool is_free_cache_almost_empty() const;
  bool is_free_cache_low() const;
  bool is_free_cache_high() const;
  std::optional<Region> pop_cache_used_region();
  void push_cache_free_region(Region &region);
  void swap_in(bool nt, GenericFarMemPtr *ptr);
  void swap_out(GenericFarMemPtr *ptr, Object obj);
  void launch_gc_master();
  void gc_cache();
  void gc_far_mem();
  uint64_t allocate_local_object(bool nt, uint16_t object_size);
  std::optional<uint64_t> allocate_local_object_nb(bool nt,
                                                   uint16_t object_size);
  uint64_t allocate_remote_object(bool nt, uint16_t object_size);
  void mutator_wait_for_gc_far_mem();
  void pick_from_regions();
  void mark_fm_ptrs(auto *preempt_guard);
  void wait_mutators_observation();
  void write_back_regions();
  void gc_check();
  void start_prioritizing(Status status);
  void stop_prioritizing();
  uint8_t allocate_ds_id();
  void free_ds_id(uint8_t ds_id);

public:
  using WriteObjectFn = std::function<void(uint32_t data_len)>;
  using EvacNotifier = std::function<bool(Object, WriteObjectFn)>;
  using CopyNotifier = std::function<void(Object dest, Object src)>;

  uint32_t num_gc_threads_;
  EvacNotifier evac_notifiers_[kMaxNumDSIDs];
  CopyNotifier copy_notifiers_[kMaxNumDSIDs];

  ~FarMemManager();
  FarMemDevice *get_device() const { return device_ptr_.get(); }
  double get_free_mem_ratio() const;
  bool allocate_generic_unique_ptr_nb(
      GenericUniquePtr *ptr, uint8_t ds_id, uint16_t item_size,
      std::optional<uint8_t> optional_id_len = {},
      std::optional<const uint8_t *> optional_id = {});
  GenericUniquePtr
  allocate_generic_unique_ptr(uint8_t ds_id, uint16_t item_size,
                              std::optional<uint8_t> optional_id_len = {},
                              std::optional<const uint8_t *> optional_id = {});
  bool reallocate_generic_unique_ptr_nb(const DerefScope &scope,
                                        GenericUniquePtr *ptr,
                                        uint16_t new_item_size,
                                        const uint8_t *data_buf);
  template <typename T>
  UniquePtr<T> allocate_unique_ptr(uint8_t ds_id = kVanillaPtrDSID);
  template <typename T>
  SharedPtr<T> allocate_shared_ptr(uint8_t ds_id = kVanillaPtrDSID);
  template <typename T, uint64_t... Dims> Array<T, Dims...> allocate_array();
  template <typename T, uint64_t... Dims>
  Array<T, Dims...> *allocate_array_heap();
  GenericConcurrentHopscotch
  allocate_concurrent_hopscotch(uint32_t local_num_entries_shift,
                                uint32_t remote_num_entries_shift,
                                uint64_t remote_data_size);
  GenericConcurrentHopscotch *
  allocate_concurrent_hopscotch_heap(uint32_t local_num_entries_shift,
                                     uint32_t remote_num_entries_shift,
                                     uint64_t remote_data_size);
  template <typename K, typename V>
  ConcurrentHopscotch<K, V>
  allocate_concurrent_hopscotch(uint32_t local_num_entries_shift,
                                uint32_t remote_num_entries_shift,
                                uint64_t remote_data_size);
  template <typename K, typename V>
  ConcurrentHopscotch<K, V> *
  allocate_concurrent_hopscotch_heap(uint32_t local_num_entries_shift,
                                     uint32_t remote_num_entries_shift,
                                     uint64_t remote_data_size);
  template <typename T> DataFrameVector<T> allocate_dataframe_vector();
  template <typename T> DataFrameVector<T> *allocate_dataframe_vector_heap();
  template <typename T>
  List<T> allocate_list(const DerefScope &scope, bool enable_merge = false);
  template <typename T> Queue<T> allocate_queue(const DerefScope &scope);
  template <typename T> Stack<T> allocate_stack(const DerefScope &scope);
  void register_eval_notifier(uint8_t ds_id, EvacNotifier notifier);
  void register_copy_notifier(uint8_t ds_id, CopyNotifier notifier);
  void read_object(uint8_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id,
                   uint16_t *data_len, uint8_t *data_buf);
  bool remove_object(uint64_t ds_id, uint8_t obj_id_len, const uint8_t *obj_id);
  void construct(uint8_t ds_type, uint8_t ds_id, uint32_t param_len,
                 uint8_t *params);
  void destruct(uint8_t ds_id);
  void mutator_wait_for_gc_cache();
  static void lock_object(uint8_t obj_id_len, const uint8_t *obj_id);
  static void unlock_object(uint8_t obj_id_len, const uint8_t *obj_id);
};

class FarMemManagerFactory {
private:
  constexpr static uint32_t kDefaultNumGCThreads = 10;

  static FarMemManager *ptr_;
  friend class FarMemManager;

public:
  static FarMemManager *build(uint64_t cache_size,
                              std::optional<uint32_t> optional_num_gc_threads,
                              FarMemDevice *device);
  static FarMemManager *get();
};

} // namespace far_memory

#include "internal/manager.ipp"
