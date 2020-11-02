extern "C" {
#include <runtime/runtime.h>
}

#include "helpers.hpp"
#include "local_skiplist.hpp"

#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <utility>

using namespace far_memory;
using namespace std;

constexpr static uint64_t kLocalSkiplistDataSize = (1 << 30);

constexpr static uint32_t kKeyMaxLen = 10;
constexpr static uint32_t kNumEntries = 1 << 20;

std::set<uint32_t> my_set;

void do_work(void *arg) {
  LocalSkiplist<uint32_t> local_skiplist(kLocalSkiplistDataSize);
  for (uint32_t i = 0; i < kNumEntries; i++) {
    uint32_t key = rand();
    bool not_exist = (my_set.count(key) == 0);
    TEST_ASSERT(local_skiplist.insert(key) == not_exist);
    my_set.insert(key);
  }

  for (const auto &key : my_set) {
    TEST_ASSERT(local_skiplist.exist(key));
    TEST_ASSERT(local_skiplist.remove(key));
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
