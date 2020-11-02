extern "C" {
#include <runtime/runtime.h>
#include <runtime/timer.h>
}
#include "thread.h"

#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"
#include "stats.hpp"

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

constexpr uint64_t kCacheSize = 1500ULL << 20; // 1.5 GiB.
constexpr uint64_t kFarMemSize = 10ULL << 30;  // 10 GiB.
constexpr uint64_t kNumEntries = 1
                                 << 20; // 1 million entries, 4 GiB working set.
constexpr uint64_t kWorkSetSize = kNumEntries * sizeof(Data_t);
constexpr uint32_t kNumMutatorThreads = 100;
constexpr uint64_t kNumEntriesPerThread = kNumEntries / kNumMutatorThreads;
constexpr uint64_t kAccessSizePerTask = 1 << 20; // 1 MiB.
constexpr uint64_t kNumAccessedEntriesPerTask =
    kAccessSizePerTask / sizeof(Data_t);
constexpr uint64_t kNumGCThreads = 20;
constexpr uint32_t kNumConnections = 200;
constexpr uint32_t kMeasureTimes = 1;
constexpr uint32_t kDelayMs = 20;

UniquePtr<Data_t> ptrs[kNumEntries];
UniquePtr<Data_t> trash_ptrs[kNumEntries];

void flush_cache() {
  for (uint64_t i = 0; i < kNumEntries; i++) {
    auto data = trash_ptrs[i].read();
    ACCESS_ONCE(data.buf[0]);
  }
}

void do_work(FarMemManager *manager) {
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i] = std::move(manager->allocate_unique_ptr<Data_t>());
    trash_ptrs[i] = std::move(manager->allocate_unique_ptr<Data_t>());
  }

  flush_cache();
  delay_ms(1000);

  Stats::clear_free_mem_ratio_records();
  std::vector<rt::Thread> threads;
  for (uint32_t t = 0; t < kMeasureTimes; t++) {
    cout << "Measure " << t << "th..." << endl;
    for (uint32_t i = 0; i < kNumMutatorThreads; i++) {
      threads.emplace_back(rt::Thread([&, i]() {
        auto start_idx = kNumEntriesPerThread * i;
        auto end_idx = start_idx + kNumEntriesPerThread;
        for (uint64_t j = start_idx; j < end_idx;
             j += kNumAccessedEntriesPerTask) {
          DerefScope scope;
          for (uint64_t t = j;
               t < std::min(end_idx, j + kNumAccessedEntriesPerTask); t++) {
            auto *raw_ptr = ptrs[t].deref(scope);
            ACCESS_ONCE(raw_ptr);
          }
          delay_ms(kDelayMs);
        }
      }));
    }
    for (auto &thread : threads) {
      thread.Join();
    }
    threads.clear();
    delay_ms(1000);
  }

  Stats::print_free_mem_ratio_records();
  std::cout << "Cleanup..." << std::endl;
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i].free();
    trash_ptrs[i].free();
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
