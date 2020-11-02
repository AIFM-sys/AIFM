extern "C" {
#include <runtime/runtime.h>
}
#include "thread.h"

#include "concurrent_hopscotch.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"
#include "slab.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace far_memory;
using namespace std;

namespace far_memory {

class FarMemTest {

  constexpr static uint32_t kKeyLen = 128;
  constexpr static uint32_t kValueLen = 400;
  constexpr static double kLoadFactor = 0.3;
  constexpr static uint64_t kHashTableNumEntriesShift = 23; // 8 M entries.
  constexpr static uint64_t kHashTableRemoteDataSize = 8ULL << 30; // 8 GiB
  constexpr static uint32_t kNumKVPairs =
      kLoadFactor * (1 << kHashTableNumEntriesShift);
  constexpr static uint32_t kHotKVSLen = 64;
  constexpr static uint32_t kNumNormalGetsToTriggerHotLookup = 256;

  constexpr static uint64_t kCacheSize = 400ULL << 20;
  constexpr static uint64_t kFarMemSize = 1ULL << 30;
  constexpr static uint64_t kNumGCThreads = 21;

  struct Key {
    char data[kKeyLen];
  };
  std::vector<Key> keys;
  std::vector<Key> hot_keys;

  struct Value {
    char data[kValueLen];
  };
  std::vector<Value> values;

  inline void random_string(char *data, uint32_t len) {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution('a', 'z' + 1);
    for (uint32_t i = 0; i < len; i++) {
      data[i] = char(distribution(generator));
    }
  }

  void generate_kvs() {
    Key key;
    Value value;
    for (uint32_t i = 0; i < kNumKVPairs; i++) {
      random_string(key.data, kKeyLen);
      random_string(value.data, kValueLen);
      keys.push_back(key);
      values.push_back(value);
    }
  }

  void choose_hot_kvs() {
    for (uint32_t i = 0; i < kHotKVSLen; i++) {
      hot_keys.push_back(keys[i]);
    }
  }

  void put_kvs(GenericConcurrentHopscotch *hopscotch) {
    for (uint32_t i = 0; i < keys.size(); i++) {
      auto &key = keys[i];
      auto &value = values[i];
      hopscotch->put_tp(kKeyLen, reinterpret_cast<const uint8_t *>(key.data),
                        kValueLen, reinterpret_cast<uint8_t *>(value.data));
    }
  }

  void get_kvs(GenericConcurrentHopscotch *hopscotch) {
    uint32_t sum_forwarded = 0;
    for (uint32_t i = 0; i < keys.size(); i++) {
      auto key = keys[i];
      Value value;
      uint16_t value_len;
      if (i == 0) {
        // Ensure all hot keys are in cache now.
        for (const auto &key : hot_keys) {
          hopscotch->get_tp(
              kKeyLen, reinterpret_cast<const uint8_t *>(key.data), &value_len,
              reinterpret_cast<uint8_t *>(value.data));
        }
      }

      if (i % kNumNormalGetsToTriggerHotLookup == 0) {
        for (const auto &key : hot_keys) {
          DerefScope scope;
          bool forwarded = false;
          hopscotch->_get(kKeyLen, reinterpret_cast<const uint8_t *>(key.data),
                          &value_len, reinterpret_cast<uint8_t *>(value.data),
                          &forwarded);
          sum_forwarded += forwarded;
        }
      }
      hopscotch->get_tp(kKeyLen, reinterpret_cast<const uint8_t *>(key.data),
                        &value_len, reinterpret_cast<uint8_t *>(value.data));
      if (value_len != kValueLen ||
          strncmp(values[i].data, value.data, value_len) != 0) {
        goto unmatched;
      }
    }

    if (sum_forwarded == 0) {
      std::cout << "Passed" << std::endl;
    } else {
      std::cout << "Failed" << std::endl;
    }
    return;
  unmatched:
    std::cout << "Value does not match" << std::endl;
  }

public:
  void do_work() {
    cout << "Running " << __FILE__ "..." << endl;

    std::unique_ptr<FarMemManager> manager =
        std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
            kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));

    auto hopscotch = manager->allocate_concurrent_hopscotch(
        kHashTableNumEntriesShift, kHashTableNumEntriesShift,
        kHashTableRemoteDataSize);
    generate_kvs();
    choose_hot_kvs();
    put_kvs(&hopscotch);
    get_kvs(&hopscotch);
  }
};
} // namespace far_memory

void _main(void *args) {
  FarMemTest test;
  test.do_work();
}

int main(int argc, char **argv) {
  int ret;

  if (argc < 2) {
    std::cerr << "usage: [cfg_file]" << std::endl;
    return -EINVAL;
  }

  ret = runtime_init(argv[1], _main, NULL);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
