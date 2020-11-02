#pragma once

extern "C" {
#include <runtime/runtime.h>
}

#include "helpers.hpp"

#include <cstdint>
#include <vector>

namespace far_memory {

struct alignas(64) Cacheline {
  uint8_t data[64];
};

class Stats {
private:
  static bool enable_swap_;
#ifdef MONITOR_FREE_MEM_RATIO
  static std::vector<std::pair<uint64_t, double>>
      free_mem_ratio_records_[helpers::kNumCPUs];
#endif

#ifdef MONITOR_READ_OBJECT_CYCLES
  static unsigned read_object_cycles_high_start_;
  static unsigned read_object_cycles_low_start_;
  static unsigned read_object_cycles_high_end_;
  static unsigned read_object_cycles_low_end_;
#endif

#ifdef MONITOR_WRITE_OBJECT_CYCLES
  static unsigned write_object_cycles_high_start_;
  static unsigned write_object_cycles_low_start_;
  static unsigned write_object_cycles_high_end_;
  static unsigned write_object_cycles_low_end_;
#endif

  static void _add_free_mem_ratio_record();

public:
#define ADD_STAT(type, x, enable_flag)                                         \
private:                                                                       \
  static type x##_;                                                            \
                                                                               \
public:                                                                        \
  FORCE_INLINE static void inc_##x(type num) {                                 \
    if (enable_flag) {                                                         \
      ACCESS_ONCE(x##_) += num;                                                \
    }                                                                          \
  }                                                                            \
  FORCE_INLINE static type get_##x() { return ACCESS_ONCE(x##_); }

#define ADD_PER_CORE_STAT(type, x, enable_flag)                                \
private:                                                                       \
  static Cacheline x##_[helpers::kNumCPUs];                                    \
                                                                               \
public:                                                                        \
  FORCE_INLINE static void inc_##x(type num) {                                 \
    if (enable_flag) {                                                         \
      preempt_disable();                                                       \
      ACCESS_ONCE(*((type *)(x##_ + get_core_num()))) += num;                  \
      preempt_enable();                                                        \
    }                                                                          \
  }                                                                            \
  FORCE_INLINE static type get_##x() {                                         \
    type sum = 0;                                                              \
    FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(*((type *)(x##_ + i))); }    \
    return sum;                                                                \
  }

  static void enable_swap();
  static void disable_swap();
  static void clear_free_mem_ratio_records();
  static void print_free_mem_ratio_records();
  static uint64_t get_schedule_us();
  static uint64_t get_softirq_us();
  static uint64_t get_gc_us();
  static uint64_t get_tcp_rw_bytes();
  static void add_free_mem_ratio_record();
  static void start_measure_read_object_cycles();
  static void finish_measure_read_object_cycles();
  static void reset_measure_read_object_cycles();
  static uint64_t get_elapsed_read_object_cycles();
  static void start_measure_write_object_cycles();
  static void finish_measure_write_object_cycles();
  static void reset_measure_write_object_cycles();
  static uint64_t get_elapsed_write_object_cycles();
};
} // namespace far_memory

#include "internal/stats.ipp"
