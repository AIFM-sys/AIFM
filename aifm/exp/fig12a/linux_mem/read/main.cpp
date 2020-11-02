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
unique_ptr<Data_t> ptrs[kNumEntries];
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
  for (uint64_t i = 0; i < kNumEntries; i++) {
    ptrs[i].reset(new Data_t());
  }

  DerefScope scope;
  for (uint64_t i = 0; i < kMeasureTimes; i++) {
    auto idx = xorshf96() % kNumEntries;

    helpers::timer_start(&cycles_high_start, &cycles_low_start);
    {
      const Data_t *raw_const_ptr = ptrs[idx].get();
      ACCESS_ONCE(*raw_const_ptr);
    }
    helpers::timer_end(&cycles_high_end, &cycles_low_end);

    auto duration = helpers::get_elapsed_cycles(
        cycles_high_start, cycles_low_start, cycles_high_end, cycles_low_end);
    if (duration > kRawMemoryAccessCycles) {
      durations.push_back(duration);
    }
  }

  print_results(&durations);
}

int main(int argc, char *argv[]) {
  int ret;

  if (argc < 2) {
    std::cerr << "usage: [cfg_file]" << std::endl;
    return -EINVAL;
  }

  ret = runtime_init(argv[1], do_work, NULL);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
