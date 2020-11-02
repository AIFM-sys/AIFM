extern "C" {
#include <runtime/runtime.h>
}

#include "snappy.h"

#include "array.hpp"
#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <numa.h>
#include <streambuf>
#include <string>
#include <unistd.h>

using namespace std;

constexpr uint32_t kCompressedFileSize = 507860747;
constexpr uint32_t kNumCompressedFiles = 30;
void *buffers[kNumCompressedFiles - 1];

string read_file_to_string(const string &file_path) {
  ifstream fs(file_path);
  auto guard = helpers::finally([&]() { fs.close(); });
  return string((std::istreambuf_iterator<char>(fs)),
                std::istreambuf_iterator<char>());
}

void write_file_to_string(const string &file_path, const string &str) {
  std::ofstream fs(file_path);
  fs << str;
  fs.close();
}

void uncompress_files_bench(const string &in_file_path,
                            const string &out_file_path) {
  string in_str = read_file_to_string(in_file_path);
  string out_str;

  for (uint32_t i = 0; i < kNumCompressedFiles - 1; i++) {
    buffers[i] = numa_alloc_onnode(kCompressedFileSize, 1);
    if (buffers[i] == nullptr) {
      helpers::dump_core();
    }
    memcpy(buffers[i], in_str.data(), in_str.size());
  }

  auto start = chrono::steady_clock::now();
  for (uint32_t i = 0; i < kNumCompressedFiles; i++) {
    std::cout << "Uncompressing file " << i << std::endl;
    if (i == 0) {
      snappy::Uncompress(in_str.data(), in_str.size(), &out_str);
    } else {
      snappy::Uncompress((const char *)buffers[i - 1], kCompressedFileSize,
                         &out_str);
    }
  }
  auto end = chrono::steady_clock::now();
  cout << "Elapsed time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(end - start).count()
       << " µs" << endl;

  for (uint32_t i = 0; i < kNumCompressedFiles - 1; i++) {
    numa_free(buffers[i], kCompressedFileSize);
  }

  // write_file_to_string(out_file_path, out_str);
}

void do_work(void *arg) {
  uncompress_files_bench("/mnt/enwik9.compressed",
                         "/mnt/enwik9.uncompressed.tmp");
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
