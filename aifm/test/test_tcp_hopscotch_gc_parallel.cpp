extern "C" {
#include <runtime/runtime.h>
}
#include "thread.h"

#include "concurrent_hopscotch.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace far_memory;
using namespace std;

constexpr static uint32_t kNumThreads = 23;
constexpr static uint32_t kKeyMaxLen = 50;
constexpr static uint32_t kValMaxLen = 100;
constexpr static uint32_t kHashTableNumEntriesShift = 19;
constexpr static uint32_t kHashTableRemoteDataSize =
    (Object::kHeaderSize + kKeyMaxLen + kValMaxLen) *
    (1 << kHashTableNumEntriesShift) * kNumThreads;
constexpr static double kLoadFactor = 0.85;
constexpr static uint32_t kNumKVPairs =
    kLoadFactor * (1 << kHashTableNumEntriesShift);

constexpr static uint64_t kCacheSize = (128ULL << 20);
constexpr static uint64_t kFarMemSize = (4ULL << 30);
constexpr static uint32_t kNumGCThreads = 12;
constexpr static uint64_t kNumConnections = 300;

struct Op {
  enum OpCode { Get, Put, Remove } opcode;
  std::string key;
  std::string val;
};

std::vector<Op> ops[kNumThreads];
std::vector<rt::Thread> threads;

std::string random_string(uint32_t max_len) {
  std::string str;
  auto len = rand() % max_len + 1;
  for (uint32_t i = 0; i < len; i++) {
    str += rand() % ('z' - 'a' + 1) + 'a';
  }
  return str;
}

void thread_work_fn(int tid, GenericConcurrentHopscotch *hopscotch) {
  for (auto &op : ops[tid]) {
    if (op.opcode == Op::Get) {
      uint16_t val_len;
      char raw_val[kValMaxLen];
      hopscotch->get_tp(op.key.size(),
                        reinterpret_cast<const uint8_t *>(op.key.c_str()),
                        &val_len, reinterpret_cast<uint8_t *>(raw_val));
      if (!val_len) {
        continue;
      }
      auto val = std::string(raw_val, val_len);
      auto prefix_end = val.find("_");
      BUG_ON(prefix_end == std::string::npos);
      std::string prefix = val.substr(0, prefix_end);
      auto suffix_begin = val.find("_");
      BUG_ON(suffix_begin == std::string::npos);
      auto suffix = std::stoi(val.substr(suffix_begin + 1));
      if (prefix != op.key || suffix < 0 ||
          suffix >= static_cast<int>(kNumThreads)) {
        std::cout << "Failed" << std::endl;
        return;
      }
    } else if (op.opcode == Op::Put) {
      hopscotch->put_tp(
          op.key.size(), reinterpret_cast<const uint8_t *>(op.key.c_str()),
          op.val.size(), reinterpret_cast<const uint8_t *>(op.val.c_str()));
    } else if (op.opcode == Op::Remove) {
      hopscotch->remove_tp(op.key.size(),
                           reinterpret_cast<const uint8_t *>(op.key.c_str()));
    }
  }
}

void do_work(FarMemManager *manager) {
  cout << "Running " << __FILE__ "..." << endl;

  auto hopscotch = manager->allocate_concurrent_hopscotch(
      kHashTableNumEntriesShift, kHashTableNumEntriesShift,
      kHashTableRemoteDataSize);

  for (uint32_t i = 0; i < kNumKVPairs; i++) {
    auto key = random_string(kKeyMaxLen);
    for (uint32_t j = 0; j < kNumThreads; j++) {
      auto val = key + "_" + std::to_string(j);
      struct Op op {
        .opcode = Op::Get, .key = key, .val = val
      };
      ops[j].push_back(op);
      op.opcode = Op::Put;
      ops[j].push_back(op);
      op.opcode = Op::Remove;
      ops[j].push_back(op);
    }
  }

  for (uint32_t i = 0; i < kNumThreads; i++) {
    std::random_shuffle(ops[i].begin(), ops[i].end());
  }

  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads.push_back(rt::Thread([&, i]() { thread_work_fn(i, &hopscotch); }));
  }

  for (uint32_t i = 0; i < kNumThreads; i++) {
    threads[i].Join();
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
