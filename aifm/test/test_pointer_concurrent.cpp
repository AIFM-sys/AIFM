extern "C" {
#include <runtime/runtime.h>
}
#include "thread.h"

#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace far_memory;
using namespace std;

constexpr uint64_t kCacheSize = 128 * Region::kSize;
constexpr uint64_t kFarMemSize = (2ULL << 30); // 2 GB
constexpr uint32_t kNumMutators = 32;
constexpr uint32_t kNumGCThreads = 12;
constexpr uint64_t kNumTestIterations = 64;

struct Data4096 {
  uint8_t data[4096];
};

constexpr uint32_t kChunkSize = sizeof(Data4096) / kNumMutators;
static_assert(sizeof(Data4096) % kNumMutators == 0, "");

using Data_t = struct Data4096;

void thread_fn(int tid, UniquePtr<Data_t> *shared_data_ptr) {
  // Should be aware that DerefScope is thread-local and cannot be
  // shared among threads.
  DerefScope scope;
  auto raw_mut_ptr = shared_data_ptr->deref_mut(scope);
  int begin_idx = tid * kChunkSize;
  int end_idx = begin_idx + kChunkSize;
  for (int i = begin_idx; i < end_idx; i++) {
    raw_mut_ptr->data[i] = tid;
  }
}

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;

  std::vector<UniquePtr<Data_t>> flush_cache_vec;
  UniquePtr<Data_t> shared_data_ptr;

  for (uint64_t i = 0; i < kCacheSize / sizeof(Data_t); i++) {
    auto far_mem_ptr = manager->allocate_unique_ptr<Data_t>();
    {
      DerefScope scope;
      auto raw_mut_ptr = far_mem_ptr.deref_mut(scope);
      memset(raw_mut_ptr->data, static_cast<char>(i), sizeof(Data_t));
    }
    flush_cache_vec.push_back(std::move(far_mem_ptr));
  }
  shared_data_ptr = manager->allocate_unique_ptr<Data_t>();

  for (uint32_t i = 0; i < kNumTestIterations; i++) {
    // Flush the cache.
    for (auto &far_mem_ptr : flush_cache_vec) {
      DerefScope scope;
      const auto raw_const_ptr = far_mem_ptr.deref(scope);
      DONT_OPTIMIZE(raw_const_ptr);
    }
    std::vector<rt::Thread> threads;
    threads.reserve(kNumMutators);
    for (uint32_t j = 0; j < kNumMutators; j++) {
      threads.emplace_back(
          rt::Thread([&, j] { thread_fn(j, &shared_data_ptr); }));
    }
    for (auto &thread : threads) {
      thread.Join();
    }
    DerefScope scope;
    auto raw_mut_ptr = shared_data_ptr.deref_mut(scope);
    for (uint32_t tid = 0; tid < kNumMutators; tid++) {
      int begin_idx = tid * kChunkSize;
      int end_idx = begin_idx + kChunkSize;
      for (int j = begin_idx; j < end_idx; j++) {
        if (raw_mut_ptr->data[j] != tid) {
          goto fail;
        }
      }
    }
  }

  cout << "Passed" << endl;
  return;

fail:
  cout << "Failed" << endl;
  return;
}

void _main(void *arg) {
  auto manager = std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
      kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
  do_work(manager.get());
}

int main(int argc, char *argv[]) {
  int ret;

  if (argc < 2) {
    std::cerr << "usage: [cfg_file]" << std::endl;
    return -EINVAL;
  }

  ret = runtime_init(argv[1], _main, NULL);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
