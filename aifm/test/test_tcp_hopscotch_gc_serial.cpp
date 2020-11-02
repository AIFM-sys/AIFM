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

constexpr static uint32_t kKeyMaxLen = 200;
constexpr static uint32_t kValueMaxLen = 700;
constexpr static uint32_t kHashTableNumEntriesShift = 19;
constexpr static uint32_t kHashTableRemoteDataSize =
    (Object::kHeaderSize + kKeyMaxLen + kValueMaxLen) *
    (1 << kHashTableNumEntriesShift);
constexpr static double kLoadFactor = 0.80;
constexpr static uint32_t kNumKVPairs =
    kLoadFactor * (1 << kHashTableNumEntriesShift);

constexpr static uint64_t kCacheSize = (128ULL << 20);
constexpr static uint64_t kFarMemSize = (1ULL << 30);
constexpr static uint32_t kNumGCThreads = 12;
constexpr static uint32_t kNumConnections = 300;

std::map<std::string, std::string> kvs;

std::string random_string(uint32_t max_len) {
  std::string str;
  auto len = rand() % max_len + 1;
  for (uint32_t i = 0; i < len; i++) {
    str += rand() % ('z' - 'a' + 1) + 'a';
  }
  return str;
}

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;

  auto hopscotch = manager->allocate_concurrent_hopscotch(
      kHashTableNumEntriesShift, kHashTableNumEntriesShift,
      kHashTableRemoteDataSize);

  for (uint32_t i = 0; i < kNumKVPairs; i++) {
    auto key = random_string(kKeyMaxLen);
    auto value = random_string(kValueMaxLen);
    hopscotch.put_tp(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                     value.size(),
                     reinterpret_cast<const uint8_t *>(value.c_str()));
    kvs[key] = value;
  }

  for (auto &[key, value] : kvs) {
    uint16_t val_len;
    char val[kValueMaxLen];
    hopscotch.get_tp(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                     &val_len, reinterpret_cast<uint8_t *>(val));
    TEST_ASSERT(value.size() == val_len);
    TEST_ASSERT(strncmp(val, value.c_str(), val_len) == 0);
  }

  for (auto &[key, value] : kvs) {
    TEST_ASSERT(hopscotch.remove_tp(
        key.size(), reinterpret_cast<const uint8_t *>(key.c_str())));
  }

  for (auto &[key, value] : kvs) {
    uint16_t val_len;
    char val[kValueMaxLen];
    hopscotch.get_tp(key.size(), reinterpret_cast<const uint8_t *>(key.c_str()),
                     &val_len, reinterpret_cast<uint8_t *>(val));
    TEST_ASSERT(val_len == 0);
  }

  std::cout << "Passed" << std::endl;
}

int argc;
void _main(void *arg) {
  char **argv = static_cast<char **>(arg);
  std::string ip_addr_port(argv[1]);
  auto raddr = helpers::str_to_netaddr(ip_addr_port);
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads,
          new TCPDevice(raddr, kNumConnections, kFarMemSize)));
  do_work(manager.get());
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

  ret = runtime_init(conf_path, _main, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
