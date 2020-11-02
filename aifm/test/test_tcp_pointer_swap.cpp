extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "device.hpp"
#include "manager.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

using namespace far_memory;
using namespace std;

constexpr static uint64_t kCacheSize = 256 * Region::kSize;
constexpr static uint64_t kFarMemSize = (1ULL << 32); // 4 GB.
constexpr static uint64_t kWorkSetSize = 1 << 30;
constexpr static uint64_t kNumGCThreads = 12;
constexpr static uint64_t kNumConnections = 300;

struct Data4096 {
  char data[4096];
};

using Data_t = struct Data4096;

constexpr static uint64_t kNumEntries = kWorkSetSize / sizeof(Data_t);

void do_work(FarMemManager *manager) {
  std::vector<UniquePtr<Data_t>> vec;

  for (uint64_t i = 0; i < kNumEntries; i++) {
    auto far_mem_ptr = manager->allocate_unique_ptr<Data_t>();
    {
      DerefScope scope;
      auto raw_mut_ptr = far_mem_ptr.deref_mut(scope);
      memset(raw_mut_ptr->data, static_cast<char>(i), sizeof(Data_t));
    }
    vec.emplace_back(std::move(far_mem_ptr));
  }

  for (uint64_t i = 0; i < kNumEntries; i++) {
    {
      DerefScope scope;
      const auto raw_const_ptr = vec[i].deref(scope);
      for (uint32_t j = 0; j < sizeof(Data_t); j++) {
        if (raw_const_ptr->data[j] != static_cast<char>(i)) {
          goto fail;
        }
      }
    }
  }

  cout << "Passed" << endl;
  return;

fail:
  cout << "Failed" << endl;
}

int argc;
void _main(void *arg) {
  cout << "Running " << __FILE__ "..." << endl;
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
