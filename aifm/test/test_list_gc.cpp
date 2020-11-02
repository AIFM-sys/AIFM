extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "list.hpp"
#include "manager.hpp"

#include <iostream>
#include <memory>

using namespace far_memory;

struct Data {
  uint32_t data;
  uint32_t dummy[1023];

  Data(uint32_t _data) : data(_data) {}
};

constexpr uint64_t kCacheSize = (256ULL << 20);
constexpr uint64_t kFarMemSize = (8ULL << 30);
constexpr uint32_t kNumGCThreads = 12;
constexpr uint32_t kNumDataEntries = 8 * kCacheSize / sizeof(Data);
constexpr uint32_t kScopeResetInterval = 256;

namespace far_memory {

class FarMemTest {
public:
  void do_work(FarMemManager *manager) {
    std::cout << "Running " << __FILE__ "..." << std::endl;
    DerefScope scope;
    List<Data> list = FarMemManagerFactory::get()->allocate_list<Data>(scope);

    for (uint32_t i = 0; i < kNumDataEntries; i++) {
      if (unlikely(i % kScopeResetInterval == 0)) {
	scope.renew();
      }
      list.push_back(scope, Data(i));
    }

    uint32_t idx = 0;
    for (auto iter = list.begin(scope); iter != list.end(scope);
         iter.inc(scope), idx++) {
      if (unlikely(idx % kScopeResetInterval == 0)) {
	scope.renew();
      }
      TEST_ASSERT(iter.deref(scope).data == idx);
    }
    TEST_ASSERT(idx == kNumDataEntries);

    std::cout << "Passed" << std::endl;
  }
};
} // namespace far_memory

void _main(void *arg) {
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads, new FakeDevice(kFarMemSize)));
  FarMemTest test;
  test.do_work(manager.get());
}

int main(int argc, char *argv[]) {
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
