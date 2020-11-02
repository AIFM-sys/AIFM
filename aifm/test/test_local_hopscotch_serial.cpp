extern "C" {
#include <runtime/runtime.h>
}

#include "helpers.hpp"
#include "local_concurrent_hopscotch.hpp"

#include <iostream>
#include <map>
#include <string>

using namespace far_memory;
using namespace std;

constexpr static uint64_t kLocalSlabMemSize = (1 << 30);
constexpr static uint32_t kHashTableNumEntriesShift = 16;

constexpr static uint32_t kKeyMaxLen = 200;
constexpr static uint32_t kValueMaxLen = 700;
constexpr static double kLoadFactor = 0.85;
constexpr static uint32_t kNumKVPairs =
    kLoadFactor * (1 << kHashTableNumEntriesShift);

std::map<std::string, std::string> kvs;

std::string random_string(uint32_t max_len) {
  std::string str;
  auto len = rand() % max_len + 1;
  for (uint32_t i = 0; i < len; i++) {
    str += rand() % ('z' - 'a' + 1) + 'a';
  }
  return str;
}

void do_work(void *args) {
  cout << "Running " << __FILE__ "..." << endl;

  auto hopscotch = LocalGenericConcurrentHopscotch(kHashTableNumEntriesShift,
                                                   kLocalSlabMemSize);

  for (uint32_t i = 0; i < kNumKVPairs; i++) {
    auto key = random_string(kKeyMaxLen);
    auto value = random_string(kValueMaxLen);
    hopscotch.put(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                  value.size(),
                  reinterpret_cast<const uint8_t *>(value.c_str()));
    kvs[key] = value;
  }

  for (auto &[key, value] : kvs) {
    uint16_t val_len;
    char val[kValueMaxLen];
    hopscotch.get(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                  &val_len, reinterpret_cast<uint8_t *>(val));
    TEST_ASSERT(val_len == value.size());
    if (strncmp(val, value.c_str(), value.size())) {
      hopscotch.get(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                    &val_len, reinterpret_cast<uint8_t *>(val));
    }
    TEST_ASSERT(strncmp(val, value.c_str(), value.size()) == 0);
  }

  for (auto &[key, value] : kvs) {
    TEST_ASSERT(hopscotch.remove(key.size(), reinterpret_cast<const uint8_t *>(
                                                 key.c_str())) == true);
  }

  for (auto &[key, value] : kvs) {
    uint16_t val_len;
    char val[kValueMaxLen];
    hopscotch.get(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                  &val_len, reinterpret_cast<uint8_t *>(val));
    TEST_ASSERT(val_len == 0);
  }

  std::cout << "Passed" << std::endl;
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
