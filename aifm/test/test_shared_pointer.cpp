extern "C" {
#include <runtime/runtime.h>
}

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

constexpr uint64_t kCacheSize = 256 * Region::kSize;
constexpr uint64_t kFarMemSize = (1ULL << 33); // 8 GB.
constexpr uint64_t kWorkSetSize = 1 << 30;
constexpr uint64_t kNumGCThreads = 12;

struct Data4096 {
  char data[4096];
};

using Data_t = struct Data4096;

constexpr uint64_t kNumEntries = kWorkSetSize / sizeof(Data_t);

void do_work(FarMemManager *manager) {
  std::vector<SharedPtr<Data_t>> vec0;
  cout << "Running " << __FILE__ "..." << endl;

  for (uint64_t i = 0; i < kNumEntries; i++) {
    auto far_mem_ptr = manager->allocate_shared_ptr<Data_t>();
    {
      DerefScope scope;
      auto raw_mut_ptr = far_mem_ptr.deref_mut(scope);
      memset(raw_mut_ptr->data, static_cast<char>(i), sizeof(Data_t));
    }
    vec0.emplace_back(std::move(far_mem_ptr));
  }
  auto vec1 = vec0;

  auto check_fn = [](auto vec) {
    for (uint64_t i = 0; i < kNumEntries; i++) {
      {
        DerefScope scope;
        const auto raw_const_ptr = vec[i].deref(scope);
        for (uint32_t j = 0; j < sizeof(Data_t); j++) {
          if (raw_const_ptr->data[j] != static_cast<char>(i)) {
	    return false;
          }
        }
      }
    }
    return true;
  };

  if (!check_fn(vec0)) {
    goto fail;
  }
  if (!check_fn(vec1)) {
    goto fail;
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
