extern "C" {
#include <runtime/runtime.h>
#include <runtime/timer.h>
}
#include "thread.h"

#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <unistd.h>

using namespace far_memory;
using namespace std;

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define DEFINE_DATA_TYPE(width)                                                \
  struct Data_##width {                                                        \
    uint8_t buf[width];                                                        \
  };                                                                           \
  using Data_t = Data_##width;

DEFINE_DATA_TYPE(4096);

constexpr uint64_t kCacheSize = 4ULL << 30;   // 4 GiB.
constexpr uint64_t kFarMemSize = 20ULL << 30; // 20 GiB.
constexpr uint64_t kNumEntries = 4
                                 << 20; // 4 million entries, 16 GiB working set
constexpr uint64_t kWorkSetSize = kNumEntries * sizeof(Data_t);
constexpr uint32_t kNumMutatorThreads = 10;
constexpr uint64_t kMutatorAccessSizePerScope = 4 << 20; // 4 MiB
#ifdef STW_GC
// STW GC stops all mutators during GC, therefore we should allocate all CPU
// resource to collectors.
constexpr uint64_t kNumGCThreads = 20;
#else
// Pauseless GC allows collectors to run with mutators simultaneously.
constexpr uint64_t kNumGCThreads = 10;
#endif
constexpr uint32_t kNumConnections = 200;

UniquePtr<Data_t> ptrs[kNumEntries];

void flush_cache() {
  for (uint64_t i = 0; i < kNumEntries; i++) {
    Data_t tmp;
    ptrs[i].write(tmp);
  }
}

void do_work(FarMemManager *manager) {
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i] = std::move(manager->allocate_unique_ptr<Data_t>());
  }

  flush_cache();
  delay_ms(1000);

  std::vector<rt::Thread> threads;
  const auto kNumEntriesPerIter = kMutatorAccessSizePerScope / sizeof(Data_t);
  const auto kTotalNumEntriesPerIter = kNumEntriesPerIter * kNumMutatorThreads;
  for (uint64_t k = 0; k + kTotalNumEntriesPerIter <= kNumEntries;
       k += kTotalNumEntriesPerIter) {
    auto start = chrono::steady_clock::now();
    for (uint32_t i = 0; i < kNumMutatorThreads; i++) {
      threads.emplace_back(rt::Thread([&, i]() {
        DerefScope scope;
        auto tid = i;
        for (uint32_t i = 0; i < kNumEntriesPerIter; i++) {
          auto *raw_mut_ptr =
              ptrs[k + tid * kNumEntriesPerIter + i].deref_mut(scope);
          DONT_OPTIMIZE(raw_mut_ptr);
        }
      }));
    }
    for (auto &thread : threads) {
      thread.Join();
    }
    auto end = chrono::steady_clock::now();
    cout << chrono::duration_cast<chrono::microseconds>(end - start).count()
         << endl;
    threads.clear();
  }

  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i].free();
  }
}

int argc;
void _main(void *arg) {
  cout << "Running " << __FILE__ "..." << endl;
  char **argv = (char **)arg;
  std::string ip_addr_port(argv[1]);
  auto raddr = helpers::str_to_netaddr(ip_addr_port);
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads,
          new TCPDevice(raddr, kNumConnections, kFarMemSize)));
  do_work(manager.get());
}

int main(int _argc, char *argv[]) {
  int ret;

  if (_argc < 3) {
    std::cerr << "usage: [cfg_file] [ip_addr:port]" << std::endl;
    return -EINVAL;
  }

  char conf_path[strlen(argv[1]) + 1];
  strcpy(conf_path, argv[1]);
  for (int i = 2; i < _argc; i++) {
    argv[i - 1] = argv[i];
  }
  argc = _argc - 1;

  ret = runtime_init(conf_path, _main, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
