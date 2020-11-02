extern "C" {
#include <runtime/runtime.h>
#include <runtime/timer.h>
}

#include "thread.h"

#include "array.hpp"
#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unistd.h>

using namespace far_memory;
using namespace std;

#define DEFINE_DATA_TYPE(width)                                                \
  struct Data_##width {                                                        \
    uint8_t buf[width];                                                        \
  };                                                                           \
  using Data_t = Data_##width;

DEFINE_DATA_TYPE(4096);

constexpr uint64_t kCacheSize = 1ULL << 30;
constexpr uint64_t kFarMemSize = 8ULL << 30;
constexpr uint64_t kArraySize = 660ULL << 20;
constexpr uint64_t kNumGCThreads = 21;
constexpr uint64_t kNumEntries = kArraySize / sizeof(Data_t);

namespace far_memory {
class FarMemTest {
public:
  void flush_core_local_regions(FarMemManager *mgr) {
    for (int i = 0; i < helpers::kNumCPUs; i++) {
      mgr->cache_region_manager_.try_refill_core_local_free_region(
          0, &(mgr->cache_region_manager_.core_local_free_regions_[i]));
      mgr->cache_region_manager_.try_refill_core_local_free_region(
          1, &(mgr->cache_region_manager_.core_local_free_nt_regions_[i]));
    }
  }

  uint64_t read_array(Array<Data_t, kNumEntries> *array, bool nt) {
    uint64_t num_not_present = 0;
    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      num_not_present += !(array->GenericArray::at(nt, i)->meta().is_present());
      if (nt) {
        array->at<true>(scope, i);
      } else {
        array->at<false>(scope, i);
      }
    }
    return num_not_present;
  }

  uint64_t write_array(Array<Data_t, kNumEntries> *array, bool nt) {
    uint64_t num_not_present = 0;
    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      num_not_present += !(array->GenericArray::at(nt, i)->meta().is_present());
      if (nt) {
        array->at_mut<true>(scope, i);
      } else {
        array->at_mut<false>(scope, i);
      }
    }
    return num_not_present;
  }

  void clear_hot(Array<Data_t, kNumEntries> *array) {
    for (uint64_t i = 0; i < kNumEntries; i++) {
      auto *ptr = array->GenericArray::at(false, i);
      if (ptr->meta().is_present()) {
        DerefScope scope;
        ptr->deref(scope);
        ptr->meta().clear_hot();
      }
    }
  }

  void run(FarMemManager *manager) {
    auto array_A = manager->allocate_array<Data_t, kNumEntries>();
    auto array_B = manager->allocate_array<Data_t, kNumEntries>();
    auto array_C = manager->allocate_array<Data_t, kNumEntries>();

    // Make cache clean.
    read_array(&array_A, false);
    read_array(&array_B, false);
    flush_core_local_regions(manager);
    read_array(&array_C, false);
    read_array(&array_A, false);
    read_array(&array_B, false);

    // Do non-temporal read.
    read_array(&array_C, true);
    read_array(&array_A, true);

    // Check.
    array_B.disable_prefetch();
    auto num_not_present = read_array(&array_B, false);
    if (num_not_present) {
      goto fail;
    }

    // Prevent CLOCK from working.
    clear_hot(&array_A);
    clear_hot(&array_B);
    clear_hot(&array_C);
    // Make cache clean.
    flush_core_local_regions(manager);
    read_array(&array_C, false);
    read_array(&array_A, false);
    read_array(&array_B, false);

    // Do non-temporal write.
    write_array(&array_C, true);
    write_array(&array_A, true);

    // Check.
    num_not_present = read_array(&array_B, false);
    if (num_not_present) {
      goto fail;
    }

    std::cout << "Passed" << std::endl;
    return;
  fail:
    std::cout << "Failed" << std::endl;
    std::cout << num_not_present << std::endl;
  }
};
} // namespace far_memory

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;
  FarMemTest test;
  test.run(manager);
}

void _main(void *arg) {
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
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
