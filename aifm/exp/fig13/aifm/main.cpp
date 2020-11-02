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

#ifndef DELAY_NS_PER_ITER
#define DELAY_NS_PER_ITER 0
#endif

constexpr uint64_t kCacheSize = 512ULL << 20;
constexpr uint64_t kFarMemSize = 10ULL << 30;
constexpr uint64_t kWorkingSetSize = 8ULL << 30;
constexpr uint64_t kNumGCThreads = 3;
constexpr uint64_t kNumEntries = kWorkingSetSize / sizeof(Data_t);
constexpr uint32_t kCPUFreqMHz = 2594;
constexpr uint32_t kNumConnections = 400;

template <int64_t N> inline void read_raw_array(uint8_t *ptr) {
  if constexpr (N >= 8) {
    ACCESS_ONCE(*((uint64_t *)ptr));
    read_raw_array<N - 8>(ptr + 8);
  } else if constexpr (N >= 4) {
    ACCESS_ONCE(*((uint32_t *)ptr));
    read_raw_array<N - 4>(ptr + 4);
  } else if constexpr (N >= 2) {
    ACCESS_ONCE(*((uint16_t *)ptr));
    read_raw_array<N - 2>(ptr + 2);
  } else if constexpr (N == 1) {
    ACCESS_ONCE(*((uint8_t *)ptr));
    read_raw_array<N - 1>(ptr + 1);
  }
}

inline void read_array_element_api(Array<Data_t, kNumEntries> *fm_array) {
  for (uint64_t i = 0; i < kNumEntries; i++) {
    DerefScope scope;
    auto &data = fm_array->at(scope, i);
    ACCESS_ONCE(data.buf[0]);
  }
}

static inline void my_delay_cycles(int32_t cycles) {
  for (int32_t i = 0; i < cycles; i++) {
    asm("nop");
  }
}

void do_work(void *arg) {
  char **argv = (char **)arg;
  std::string ip_addr_port(argv[1]);
  auto raddr = helpers::str_to_netaddr(ip_addr_port);
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads,
          new TCPDevice(raddr, kNumConnections, kFarMemSize)));
  auto fm_array = std::unique_ptr<Array<Data_t, kNumEntries>>(
      manager->allocate_array_heap<Data_t, kNumEntries>());

  // Flush cache.
  read_array_element_api(fm_array.get());
  read_array_element_api(fm_array.get());

  std::cout << "delay = " << DELAY_NS_PER_ITER << std::endl;
  auto start_ts = chrono::steady_clock::now();
  for (uint64_t i = 0; i < kNumEntries; i++) {
    DerefScope scope;
    auto &data = fm_array->at(scope, i);
    read_raw_array<sizeof(Data_t)>((uint8_t *)data.buf);
    my_delay_cycles(DELAY_NS_PER_ITER / 1000.0 * kCPUFreqMHz);
  }
  auto end_ts = chrono::steady_clock::now();

  auto time_US =
      chrono::duration_cast<chrono::microseconds>(end_ts - start_ts).count();
  std::cout << time_US << std::endl;
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

  ret = runtime_init(conf_path, do_work, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
