extern "C" {
#include <runtime/runtime.h>
}

#include "concurrent_hopscotch.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

using namespace far_memory;
using namespace std;

constexpr static uint32_t kKeyLen = 200;
constexpr static uint32_t kValueLen = 700;
constexpr static uint32_t kHashTableNumEntriesShift = 19;
constexpr static uint32_t kHashTableRemoteDataSize =
    (Object::kHeaderSize + kKeyLen + kValueLen) *
    (1 << kHashTableNumEntriesShift);
constexpr static double kLoadFactor = 0.80;
constexpr static uint32_t kNumKVPairs =
    kLoadFactor * (1 << kHashTableNumEntriesShift);

constexpr static uint64_t kCacheSize = (128ULL << 20);
constexpr static uint64_t kFarMemSize = (1ULL << 30);
constexpr static uint32_t kNumGCThreads = 12;

struct Key {
  char data[kKeyLen];
  bool operator<(const Key &other) const {
    return strncmp(data, other.data, kKeyLen) < 0;
  }
};

struct Value {
  char data[kValueLen];
  bool operator<(const Value &other) const {
    return strncmp(data, other.data, kValueLen) < 0;
  }
};

std::map<Key, Value> kvs;

void random_string(char *data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    data[i] = rand() % ('z' - 'a' + 1) + 'a';
  }
}

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;

  auto hopscotch = manager->allocate_concurrent_hopscotch<Key, Value>(
      kHashTableNumEntriesShift, kHashTableNumEntriesShift,
      kHashTableRemoteDataSize);

  for (uint32_t i = 0; i < kNumKVPairs; i++) {
    Key key;
    Value value;
    random_string(key.data, kKeyLen);
    random_string(value.data, kValueLen);
    hopscotch.insert_tp(key, value);
    kvs[key] = value;
  }

  for (auto &[key, value] : kvs) {
    std::optional<Value> optional_value;
    optional_value = hopscotch.find_tp(key);
    TEST_ASSERT(optional_value);
    TEST_ASSERT(strncmp(optional_value->data, value.data, kValueLen) == 0);
  }

  for (auto &[key, value] : kvs) {
    TEST_ASSERT(hopscotch.erase_tp(key));
  }

  for (auto &[key, value] : kvs) {
    std::optional<Value> optional_value;
    hopscotch.find_tp(key);
    TEST_ASSERT(!optional_value);
  }

  std::cout << "Passed" << std::endl;
}

void _main(void *args) {
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
  do_work(manager.get());
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
