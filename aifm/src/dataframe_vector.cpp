#include "dataframe_vector.hpp"
#include "internal/ds_info.hpp"
#include "manager.hpp"

namespace far_memory {
GenericDataFrameVector::GenericDataFrameVector(const uint32_t chunk_size,
                                               const uint32_t chunk_num_entries,
                                               uint8_t ds_id, uint8_t dt_id)
    : chunk_size_(chunk_size), chunk_num_entries_(chunk_num_entries),
      device_(FarMemManagerFactory::get()->get_device()), ds_id_(ds_id) {
  FarMemManagerFactory::get()->construct(kDataFrameVectorDSType, ds_id,
                                         sizeof(dt_id), &dt_id);
  // DataFrameVector essentially stores a std::vector of GenericUniquePtrs, so
  // it does not need a notifier.
}

GenericDataFrameVector::~GenericDataFrameVector() { cleanup(); }

void GenericDataFrameVector::cleanup() {
  auto writer_lock = lock_.get_writer_lock();
  if (!moved_) {
    std::vector<rt::Thread> threads;
    for (uint32_t tid = 0; tid < helpers::kNumCPUs; tid++) {
      threads.emplace_back([&, tid]() {
        auto num_tasks_per_threads =
            (chunk_ptrs_.size() == 0)
                ? 0
                : (chunk_ptrs_.size() - 1) / helpers::kNumCPUs + 1;
        auto left = num_tasks_per_threads * tid;
        auto right = std::min(left + num_tasks_per_threads, chunk_ptrs_.size());
        for (uint64_t i = left; i < right; i++) {
          chunk_ptrs_[i].free();
        }
      });
    }
    for (auto &thread : threads) {
      thread.Join();
    }
    FarMemManagerFactory::get()->destruct(ds_id_);
  }
}

void GenericDataFrameVector::reserve_remote(uint64_t num) {
  if (num > remote_vec_capacity_) {
    uint16_t output_len;
    device_->compute(ds_id_, OpCode::Reserve, sizeof(num),
                     reinterpret_cast<const uint8_t *>(&num), &output_len,
                     reinterpret_cast<uint8_t *>(&remote_vec_capacity_));
    assert(output_len == sizeof(remote_vec_capacity_));
  }
}

void GenericDataFrameVector::expand(uint64_t num) {
  auto old_chunk_ptrs_size = chunk_ptrs_.size();
  uint64_t new_capacity = (old_chunk_ptrs_size + num) * chunk_num_entries_;
  reserve_remote(new_capacity);
  auto writer_lock = lock_.get_writer_lock();
  chunk_ptrs_.resize(old_chunk_ptrs_size + num);

  std::vector<rt::Thread> threads;
  for (uint32_t tid = 0; tid < helpers::kNumCPUs; tid++) {
    threads.emplace_back([&, tid]() {
      auto num_tasks_per_threads =
          (num == 0) ? 0 : (num - 1) / helpers::kNumCPUs + 1;
      auto left = num_tasks_per_threads * tid;
      auto right = std::min(left + num_tasks_per_threads, num);
      for (uint64_t i = left; i < right; i++) {
        uint64_t obj_id = i + old_chunk_ptrs_size;
        auto &new_chunk_ptr = chunk_ptrs_[obj_id];
        while (unlikely(
            !FarMemManagerFactory::get()->allocate_generic_unique_ptr_nb(
                &new_chunk_ptr, ds_id_, chunk_size_, sizeof(obj_id),
                reinterpret_cast<uint8_t *>(&obj_id)))) {
          FarMemManagerFactory::get()->mutator_wait_for_gc_cache();
        }
      }
    });
  }
  for (auto &thread : threads) {
    thread.Join();
  }
}

void GenericDataFrameVector::expand_no_alloc(uint64_t num) {
  auto old_chunk_ptrs_size = chunk_ptrs_.size();
  auto writer_lock = lock_.get_writer_lock();
  chunk_ptrs_.resize(old_chunk_ptrs_size + num);

  const auto obj_size = Object::kHeaderSize + chunk_size_ + sizeof(uint64_t);
  std::vector<rt::Thread> threads;
  for (uint32_t tid = 0; tid < helpers::kNumCPUs; tid++) {
    threads.emplace_back([&, tid]() {
      auto num_tasks_per_threads =
          (num == 0) ? 0 : (num - 1) / helpers::kNumCPUs + 1;
      auto left = num_tasks_per_threads * tid;
      auto right = std::min(left + num_tasks_per_threads, num);
      for (uint64_t i = left; i < right; i++) {
        uint64_t obj_id = i + old_chunk_ptrs_size;
        auto &new_chunk_ptr = chunk_ptrs_[obj_id];
        new_chunk_ptr.meta().gc_wb(ds_id_, obj_size, obj_id);
      }
    });
  }
  for (auto &thread : threads) {
    thread.Join();
  }
}

void GenericDataFrameVector::flush() {
  if constexpr (!DISABLE_OFFLOAD) {
    if (!dirty_) {
      return;
    }
    dirty_ = false;
    std::vector<rt::Thread> threads;
    for (uint32_t tid = 0; tid < helpers::kNumCPUs; tid++) {
      threads.emplace_back([&, tid]() {
        auto num_tasks_per_threads =
            (chunk_ptrs_.size() == 0)
                ? 0
                : (chunk_ptrs_.size() - 1) / helpers::kNumCPUs + 1;
        auto left = num_tasks_per_threads * tid;
        auto right = std::min(left + num_tasks_per_threads, chunk_ptrs_.size());
        for (uint64_t i = left; i < right; i++) {
          chunk_ptrs_[i].flush();
        }
      });
    }
    for (auto &thread : threads) {
      thread.Join();
    }
  }
}

} // namespace far_memory
