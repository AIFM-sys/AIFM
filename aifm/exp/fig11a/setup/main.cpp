#include "snappy.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <numa.h>
#include <streambuf>
#include <string>
#include <unistd.h>

using namespace std;

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

void compress_file(const string &in_file_path, const string &out_file_path) {
  string in_str = read_file_to_string(in_file_path);
  string out_str;
  snappy::Compress(in_str.data(), in_str.size(), &out_str);
  write_file_to_string(out_file_path, out_str);
}

int main(int argc, char *argv[]) {
  compress_file("/mnt/enwik9.uncompressed", "/mnt/enwik9.compressed");

  return 0;
}
