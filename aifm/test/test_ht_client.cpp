extern "C" {
#include <runtime/runtime.h>
}

#include "ht_client.hpp"

#include <cstdint>
#include <iostream>
#include <memory>

using namespace far_memory;
using namespace std;

std::unique_ptr<HashTable> ht;

void do_work(void *arg) {
  ht = std::make_unique<HashTable>();
  string real_key = "key";
  string real_value = "value";
  if (ht->get(real_key) != "") {
    goto fail;
  }
  ht->put(real_key, real_value);
  if (ht->get(real_key) != "value") {
    goto fail;
  }
  ht->remove(real_key);
  if (ht->get(real_key) != "") {
    goto fail;
  }

  cout << "Passed" << endl;
  return;

fail:
  cout << "Failed" << endl;
  return;
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
