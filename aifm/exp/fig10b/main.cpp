extern "C" {
#include <runtime/runtime.h>
}
#include "thread.h"

#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"
#include "stats.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>

using namespace far_memory;
using namespace std;

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

std::atomic_flag flag;
std::unique_ptr<std::mt19937> generators[helpers::kNumCPUs];
__thread uint32_t per_core_req_idx = 0;
std::unique_ptr<FarMemManager> manager;

namespace far_memory {
class FarMemTest {
private:
  constexpr static uint64_t kCacheSize = 1000 * Region::kSize;
  constexpr static uint64_t kFarMemSize = (1ULL << 30);
  constexpr static uint32_t kNumGCThreads = 100;
  constexpr static uint32_t kKeyLen = 12;
  constexpr static uint32_t kValueLen = 4;
  constexpr static uint32_t kLocalHashTableNumEntriesShift = 28;
  constexpr static uint32_t kRemoteHashTableNumEntriesShift = 28;
  constexpr static uint64_t kRemoteHashTableDataSize = (8ULL << 30);
  constexpr static uint32_t kNumKVPairs = 1 << 27;
  constexpr static uint32_t kNumItersPerScope = 64;
  constexpr static uint32_t kMaxNumMutatorThreads = 400;
  constexpr static uint32_t kNumMutatorThreads = 10;
  constexpr static uint32_t kReqSeqLenPerCore = kNumKVPairs;
  constexpr static uint32_t kNumConnections = 650;
  constexpr static uint32_t kMonitorPerIter = 262144;
  constexpr static uint32_t kMinMonitorIntervalUs = 10 * 1000 * 1000;
  constexpr static uint32_t kMaxRunningUs = 200 * 1000 * 1000; // 200 seconds

  struct Key {
    char data[kKeyLen];
  };

  struct Value {
    char data[kValueLen];
  };

  struct alignas(64) Cnt {
    uint64_t c;
  };

  alignas(helpers::kHugepageSize) Key all_gen_keys[kNumKVPairs];
  uint32_t all_uniform_key_indices[helpers::kNumCPUs][kReqSeqLenPerCore];
  Cnt cnts[kMaxNumMutatorThreads];
  std::vector<double> mops_vec;

  uint64_t prev_sum_cnts = 0;
  uint64_t prev_us = 0;
  uint64_t running_us = 0;

  template <typename T> T avg_of_last_N(std::vector<T> &vec, uint32_t n) {
    std::vector<T> last_n(
        vec.end() - std::min(static_cast<uint32_t>(vec.size()), n), vec.end());
    return std::accumulate(last_n.begin(), last_n.end(), 0.0) / last_n.size();
  }

  inline void append_uint32_to_char_array(uint32_t n, uint32_t suffix_len,
                                          char *array) {
    uint32_t len = 0;
    while (n) {
      auto digit = n % 10;
      array[len++] = digit + '0';
      n = n / 10;
    }
    while (len < suffix_len) {
      array[len++] = '0';
    }
    std::reverse(array, array + suffix_len);
  }

  inline void random_string(char *data, uint32_t len) {
    preempt_disable();
    auto guard = helpers::finally([&]() { preempt_enable(); });
    auto &generator = *generators[get_core_num()];
    std::uniform_int_distribution<int> distribution('a', 'z' + 1);
    for (uint32_t i = 0; i < len; i++) {
      data[i] = char(distribution(generator));
    }
  }

  inline void random_key(char *data, uint32_t tid) {
    auto tid_len = helpers::static_log(10, kMaxNumMutatorThreads);
    random_string(data, kKeyLen - tid_len);
    append_uint32_to_char_array(tid, tid_len, data + kKeyLen - tid_len);
  }

  void prepare(GenericConcurrentHopscotch *hopscotch_ptr) {
    for (uint32_t i = 0; i < helpers::kNumCPUs; i++) {
      std::random_device rd;
      generators[i].reset(new std::mt19937(rd()));
    }

    std::vector<rt::Thread> threads;
    for (uint32_t tid = 0; tid < kMaxNumMutatorThreads; tid++) {
      threads.emplace_back(rt::Thread([&, tid]() {
        auto num_kv_pairs = kNumKVPairs / kMaxNumMutatorThreads;
        if (tid == kMaxNumMutatorThreads - 1) {
          num_kv_pairs += kNumKVPairs % kMaxNumMutatorThreads;
        }
	DerefScope scope;
        auto *thread_gen_keys =
            &all_gen_keys[tid * (kNumKVPairs / kMaxNumMutatorThreads)];
        Key key;
        Value val;
        for (uint32_t i = 0; i < num_kv_pairs; i++) {
          if (unlikely(i % kNumItersPerScope == 0)) {
	    scope.renew();
          }
          random_key(key.data, tid);
          random_string(val.data, kValueLen);
          hopscotch_ptr->put(scope, kKeyLen, (const uint8_t *)key.data,
                             kValueLen, (const uint8_t *)val.data);
          thread_gen_keys[i] = key;
        }
      }));
    }
    for (auto &thread : threads) {
      thread.Join();
    }
    preempt_disable();
    std::uniform_int_distribution<uint32_t> uniform(0, kNumKVPairs - 1);
    auto &generator = generators[get_core_num()];
    for (uint32_t i = 0; i < kReqSeqLenPerCore; i++) {
      auto idx = uniform(*generator);
      BUG_ON(idx >= kNumKVPairs);
      all_uniform_key_indices[0][i] = idx;
    }
    for (uint32_t k = 1; k < helpers::kNumCPUs; k++) {
      memcpy(all_uniform_key_indices[k], all_uniform_key_indices[0],
             sizeof(uint32_t) * kReqSeqLenPerCore);
    }
    preempt_enable();
  }

  void monitor_perf() {
    if (!flag.test_and_set()) {
      auto us = microtime();
      if (us - prev_us > kMinMonitorIntervalUs) {
        uint64_t sum_cnts = 0;
        for (uint32_t i = 0; i < kNumMutatorThreads; i++) {
          sum_cnts += ACCESS_ONCE(cnts[i].c);
        }
        us = microtime();
        auto mops = (double)(sum_cnts - prev_sum_cnts) / (us - prev_us);
        mops_vec.push_back(mops);
        running_us += (us - prev_us);
        if (running_us >= kMaxRunningUs) {
          std::cout << "mops = " << avg_of_last_N(mops_vec, 5) << std::endl;
          std::cout << "Done. Force exiting..." << std::endl;
          exit(0);
        }
        prev_us = us;
        prev_sum_cnts = sum_cnts;
      }
      flag.clear();
    }
  }

  void bench_get(GenericConcurrentHopscotch *hopscotch_ptr) {
    prev_us = microtime();
    std::vector<rt::Thread> threads;
    for (uint32_t tid = 0; tid < kNumMutatorThreads; tid++) {
      threads.emplace_back(rt::Thread([&, tid]() {
        DerefScope scope;
        uint32_t cnt = 0;
        while (1) {
          if (unlikely(cnt++ % kNumItersPerScope == 0)) {
	    scope.renew();
          }
          preempt_disable();
          if (unlikely(cnt % kMonitorPerIter == 0)) {
            monitor_perf();
          }
          auto key_idx =
              all_uniform_key_indices[get_core_num()][per_core_req_idx++];
          if (unlikely(per_core_req_idx == kReqSeqLenPerCore)) {
            per_core_req_idx = 0;
          }
          preempt_enable();
          auto &key = all_gen_keys[key_idx];
          uint16_t val_len;
          Value val;
          hopscotch_ptr->get(scope, kKeyLen, (const uint8_t *)key.data,
                             &val_len, (uint8_t *)val.data);
          ACCESS_ONCE(cnts[tid].c)++;
          DONT_OPTIMIZE(val);
        }
      }));
    }
    for (auto &thread : threads) {
      thread.Join();
    }
  }

public:
  void run(netaddr raddr) {
    BUG_ON(madvise(all_gen_keys, sizeof(all_gen_keys), MADV_HUGEPAGE) != 0);
    manager.reset(FarMemManagerFactory::build(
        kCacheSize, kNumGCThreads,
        new TCPDevice(raddr, kNumConnections, kFarMemSize)));
    auto hopscotch = std::unique_ptr<GenericConcurrentHopscotch>(
        manager->allocate_concurrent_hopscotch_heap(
            kLocalHashTableNumEntriesShift, kRemoteHashTableNumEntriesShift,
            kRemoteHashTableDataSize));
    std::cout << "Prepare..." << std::endl;
    prepare(hopscotch.get());
    std::cout << "Get..." << std::endl;
    bench_get(hopscotch.get());
    hopscotch.reset();
    manager.reset();
  }
};
} // namespace far_memory

int argc;
FarMemTest test;
void my_main(void *arg) {
  char **argv = (char **)arg;
  std::string ip_addr_port(argv[1]);
  test.run(helpers::str_to_netaddr(ip_addr_port));
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

  ret = runtime_init(conf_path, my_main, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
