#pragma once

namespace far_memory {

FORCE_INLINE void Stats::enable_swap() { enable_swap_ = true; }

FORCE_INLINE void Stats::disable_swap() { enable_swap_ = false; }

FORCE_INLINE uint64_t Stats::get_schedule_us() {
  uint64_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(duration_schedule_us[i].c); }
  return sum;
}

FORCE_INLINE uint64_t Stats::get_softirq_us() {
  uint64_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(duration_softirq_us[i].c); }
  return sum;
}

FORCE_INLINE uint64_t Stats::get_gc_us() {
  uint64_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += ACCESS_ONCE(duration_gc_us[i].c); }
  return sum;
}

FORCE_INLINE uint64_t Stats::get_tcp_rw_bytes() {
  return get_tcp_tx_bytes() + get_tcp_rx_bytes();
}

FORCE_INLINE void Stats::add_free_mem_ratio_record() {
#ifdef MONITOR_FREE_MEM_RATIO
  _add_free_mem_ratio_record();
#endif
}

FORCE_INLINE void Stats::start_measure_read_object_cycles() {
#ifdef MONITOR_READ_OBJECT_CYCLES
  helpers::timer_start(&read_object_cycles_high_start_,
                       &read_object_cycles_low_start_);
#endif
}

FORCE_INLINE void Stats::finish_measure_read_object_cycles() {
#ifdef MONITOR_READ_OBJECT_CYCLES
  helpers::timer_end(&read_object_cycles_high_end_,
                     &read_object_cycles_low_end_);
#endif
}

FORCE_INLINE void Stats::reset_measure_read_object_cycles() {
#ifdef MONITOR_READ_OBJECT_CYCLES
  read_object_cycles_high_start_ = read_object_cycles_low_start_ =
      read_object_cycles_high_end_ = read_object_cycles_low_end_;
#endif
}

FORCE_INLINE uint64_t Stats::get_elapsed_read_object_cycles() {
#ifdef MONITOR_READ_OBJECT_CYCLES
  return helpers::get_elapsed_cycles(
      read_object_cycles_high_start_, read_object_cycles_low_start_,
      read_object_cycles_high_end_, read_object_cycles_low_end_);
#else
  return 0;
#endif
}

FORCE_INLINE void Stats::start_measure_write_object_cycles() {
#ifdef MONITOR_WRITE_OBJECT_CYCLES
  helpers::timer_start(&write_object_cycles_high_start_,
                       &write_object_cycles_low_start_);
#endif
}

FORCE_INLINE void Stats::finish_measure_write_object_cycles() {
#ifdef MONITOR_WRITE_OBJECT_CYCLES
  helpers::timer_end(&write_object_cycles_high_end_,
                     &write_object_cycles_low_end_);
#endif
}

FORCE_INLINE void Stats::reset_measure_write_object_cycles() {
#ifdef MONITOR_WRITE_OBJECT_CYCLES
  write_object_cycles_high_start_ = write_object_cycles_low_start_ =
      write_object_cycles_high_end_ = write_object_cycles_low_end_;
#endif
}

FORCE_INLINE uint64_t Stats::get_elapsed_write_object_cycles() {
#ifdef MONITOR_WRITE_OBJECT_CYCLES
  return helpers::get_elapsed_cycles(
      write_object_cycles_high_start_, write_object_cycles_low_start_,
      write_object_cycles_high_end_, write_object_cycles_low_end_);
#else
  return 0;
#endif
}
} // namespace far_memory
