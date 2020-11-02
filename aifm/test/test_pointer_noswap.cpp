extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

using namespace far_memory;
using namespace std;

struct Data512 {
  char data[512];
};

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;

  auto far_mem_ptr_0 = manager->allocate_unique_ptr<Data512>();
  auto far_mem_ptr_1 = manager->allocate_unique_ptr<unsigned int>();

  DerefScope scope;
  {
    auto raw_ptr_0 = far_mem_ptr_0.deref_mut(scope);
    auto raw_ptr_1 = far_mem_ptr_1.deref_mut(scope);
    for (int i = 0; i < 512; i++) {
      raw_ptr_0->data[i] = static_cast<char>(i);
    }
    *raw_ptr_1 = 0xDEADBEEF;
  }

  {
    const auto raw_ptr_0 = far_mem_ptr_0.deref(scope);
    const auto raw_ptr_1 = far_mem_ptr_1.deref(scope);
    for (int i = 0; i < 512; i++) {
      if (raw_ptr_0->data[i] != static_cast<char>(i)) {
        goto fail;
      }
    }
    if ((*raw_ptr_1) != 0xDEADBEEF) {
      goto fail;
    }
  }

  cout << "Passed" << endl;
  return;

fail:
  cout << "Failed" << endl;
  return;
}

void _main(void *arg) {
  uint64_t cache_size = 1ULL << 32;
  uint64_t far_mem_size = 1ULL << 33;
  uint8_t num_gc_threads = 12;

  auto manager = std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
      cache_size, num_gc_threads, new FakeDevice(far_mem_size)));
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
