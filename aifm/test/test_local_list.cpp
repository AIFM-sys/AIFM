extern "C" {
#include <runtime/runtime.h>
}

#include "local_list.hpp"

#include <iostream>
#include <list>

using namespace far_memory;

void do_work(void *args) {
  LocalList<int> local_list;

  for (uint32_t i = 0; i < 128; i++) {
    local_list.push_back(i);
  }

  int idx = 0;
  for (const auto &data : local_list) {
    TEST_ASSERT(data == idx++);
  }

  auto old_idx = idx;
  for (auto iter = local_list.rbegin(); iter != local_list.rend(); iter++) {
    TEST_ASSERT(*iter == --idx);
  }
  idx = old_idx;
  for (auto iter = local_list.rbegin(); iter != local_list.rend(); ++iter) {
    TEST_ASSERT(*iter == --idx);
  }
  idx = old_idx;
  auto back_iter = local_list.end();
  back_iter--;
  auto prev_begin_iter = local_list.begin();
  prev_begin_iter--;
  for (auto iter = back_iter; iter != prev_begin_iter; iter--) {
    TEST_ASSERT(*iter == --idx);
  }
  // 0...127

  TEST_ASSERT(local_list.size() == 128);
  TEST_ASSERT(local_list.front() == 0);
  TEST_ASSERT(local_list.back() == 127);
  TEST_ASSERT(!local_list.empty());
  local_list.pop_front();
  local_list.pop_back();
  // 1...126

  TEST_ASSERT(local_list.size() == 126);
  TEST_ASSERT(local_list.front() == 1);
  TEST_ASSERT(local_list.back() == 126);

  auto iter = local_list.begin();
  ++iter;
  iter++;
  local_list.insert(iter, 0);
  // 1,2,0,3,4...126

  TEST_ASSERT(local_list.size() == 127);
  iter--;
  --iter;
  iter--;
  TEST_ASSERT(local_list.front() == *iter);
  iter = local_list.erase(iter);
  iter = local_list.erase(iter);
  // 0,3,4...126

  TEST_ASSERT(local_list.front() == 0);
  TEST_ASSERT(local_list.size() == 125);

  auto rev_iter = local_list.rbegin();
  rev_iter = local_list.erase(rev_iter);
  rev_iter = local_list.erase(rev_iter);
  // 0,3,4...124

  TEST_ASSERT(local_list.back() == 124);
  rev_iter++;
  ++rev_iter;
  local_list.insert(rev_iter, 0);
  // 0,3,4...122,0,123,124

  rev_iter--;
  --rev_iter;
  rev_iter--;
  TEST_ASSERT(local_list.back() == *rev_iter);

  rev_iter = local_list.erase(rev_iter);
  rev_iter = local_list.erase(rev_iter);
  TEST_ASSERT(*rev_iter == 0);
  // 0,3,4...122,0

  uint32_t cnt = 0;
  for (iter = local_list.begin(); iter != local_list.end();
       iter = local_list.erase(iter), ++cnt)
    ;
  TEST_ASSERT(cnt == 122);
  TEST_ASSERT(local_list.size() == 0);
  TEST_ASSERT(local_list.empty());

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
