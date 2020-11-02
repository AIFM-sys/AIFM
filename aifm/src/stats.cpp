#include "stats.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <algorithm>

namespace far_memory {
bool Stats::enable_swap_;
#ifdef MONITOR_FREE_MEM_RATIO
std::vector<std::pair<uint64_t, double>>
    Stats::free_mem_ratio_records_[helpers::kNumCPUs];
#endif

#ifdef MONITOR_READ_OBJECT_CYCLES
unsigned Stats::read_object_cycles_high_start_;
unsigned Stats::read_object_cycles_low_start_;
unsigned Stats::read_object_cycles_high_end_;
unsigned Stats::read_object_cycles_low_end_;
#endif

#ifdef MONITOR_WRITE_OBJECT_CYCLES
unsigned Stats::write_object_cycles_high_start_;
unsigned Stats::write_object_cycles_low_start_;
unsigned Stats::write_object_cycles_high_end_;
unsigned Stats::write_object_cycles_low_end_;
#endif

void Stats::_add_free_mem_ratio_record() {
#ifdef MONITOR_FREE_MEM_RATIO
  preempt_disable();
  uint64_t t_us =
      helpers::chrono_to_timestamp(std::chrono::steady_clock::now());
  double free_mem_ratio = FarMemManagerFactory::get()->get_free_mem_ratio();
  free_mem_ratio_records_[get_core_num()].push_back(
      std::make_pair(t_us, free_mem_ratio));
  preempt_enable();
#endif
}

void Stats::print_free_mem_ratio_records() {
#ifdef MONITOR_FREE_MEM_RATIO
  std::vector<std::pair<uint64_t, double>> all_records;
  FOR_ALL_SOCKET0_CORES(core_id) {
    all_records.insert(all_records.end(),
                       free_mem_ratio_records_[core_id].begin(),
                       free_mem_ratio_records_[core_id].end());
  }
  sort(all_records.begin(), all_records.end());
  for (auto [t_us, ratio] : all_records) {
    std::cout << t_us << " " << ratio << std::endl;
  }
#endif
}

void Stats::clear_free_mem_ratio_records() {
#ifdef MONITOR_FREE_MEM_RATIO
  FOR_ALL_SOCKET0_CORES(core_id) { free_mem_ratio_records_[core_id].clear(); }
#endif
}

} // namespace far_memory
