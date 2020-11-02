extern "C" {
#include <runtime/runtime.h>
#include <runtime/timer.h>
}

#include "thread.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unistd.h>

using namespace std;

#ifndef DELAY_NS_PER_ITER
#define DELAY_NS_PER_ITER 0
#endif

constexpr uint64_t kEntrySize = 4096;
constexpr uint64_t kWorkingSetSize = 8ULL << 30;
constexpr uint32_t kCPUFreqMHz = 2594;

uint8_t buf[kWorkingSetSize];

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

static inline void my_delay_cycles(int32_t cycles) {
  for (int32_t i = 0; i < cycles; i++) {
    asm("nop");
  }
}

void do_work(void *arg) {
  memset(buf, 0, sizeof(buf));

  std::cout << "delay = " << DELAY_NS_PER_ITER << std::endl;
  auto start_ts = chrono::steady_clock::now();
  for (uint64_t i = 0; i < kWorkingSetSize; i += kEntrySize) {
    auto ptr = &buf[i];
    read_raw_array<kEntrySize>(ptr);
    my_delay_cycles(DELAY_NS_PER_ITER / 1000.0 * kCPUFreqMHz);
  }
  auto end_ts = chrono::steady_clock::now();

  auto time_US =
      chrono::duration_cast<chrono::microseconds>(end_ts - start_ts).count();
  std::cout << time_US << std::endl;
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
