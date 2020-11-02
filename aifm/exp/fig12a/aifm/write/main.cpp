extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

#include <iostream>

using namespace far_memory;
using namespace std;

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

using Data_t = uint64_t;

constexpr uint64_t kCacheSize = 5ULL << 30;   // 5 GB.
constexpr uint64_t kFarMemSize = 5ULL << 30;  // 5 GB.
constexpr uint64_t kWorkSetSize = 1ULL << 30; // 1 GB.
constexpr uint64_t kNumEntries = kWorkSetSize / sizeof(Data_t);
constexpr uint64_t kMeasureTimes = 1 << 20;      // 1 million times.
constexpr uint64_t kRawMemoryAccessCycles = 170; // 1 million times.
constexpr uint64_t kNumConnections = 600;
constexpr uint8_t kNumGCThreads = 21;

unsigned cycles_low_start, cycles_high_start;
unsigned cycles_low_end, cycles_high_end;
uint32_t g_seed = 0xDEADBEEF;
UniquePtr<Data_t> ptrs[kNumEntries];
std::vector<uint64_t> durations;

uint32_t x = 123456789, y = 362436069, z = 521288629;

uint32_t xorshf96(void) {
  uint32_t t;
  x ^= x << 16;
  x ^= x >> 5;
  x ^= x << 1;

  t = x;
  x = y;
  y = z;
  z = t ^ x ^ y;

  return z;
}

template <typename T> void print_percentile(int percentile, T *container) {
  auto idx = percentile / 100.0 * container->size();
  cout << percentile << "\t" << (*container)[idx] << endl;
}

template <typename T> void print_results(T *container) {
  sort(container->begin(), container->end());
  print_percentile(90, container);
}

void do_work(void *arg) {
  char **argv = (char **)arg;
  std::string ip_addr_port(argv[1]);
  auto raddr = helpers::str_to_netaddr(ip_addr_port);
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads,
          new TCPDevice(raddr, kNumConnections, kFarMemSize)));
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i] = std::move(manager->allocate_unique_ptr<Data_t>());
  }

  DerefScope scope;
  for (uint64_t i = 0; i < kMeasureTimes; i++) {
    auto idx = xorshf96() % kNumEntries;

    helpers::timer_start(&cycles_high_start, &cycles_low_start);
    {
      auto *far_mem_ptr = &ptrs[idx];
      Data_t *raw_mut_ptr = far_mem_ptr->deref_mut(scope);
      ACCESS_ONCE(*raw_mut_ptr) = 0xDEADBEEFDEADBEEFULL;
    }
    helpers::timer_end(&cycles_high_end, &cycles_low_end);

    auto duration = helpers::get_elapsed_cycles(
        cycles_high_start, cycles_low_start, cycles_high_end, cycles_low_end);
    if (duration > kRawMemoryAccessCycles) {
      durations.push_back(duration);
    }
  }

  print_results(&durations);
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i].free();
  }
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
