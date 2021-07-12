// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/file.h"
#include "util/random.h"
#include <cstring>
#if __cplusplus > 202002L
#include <filesystem>
#endif
#include <fstream>
#include <sys/stat.h>

using namespace std;

namespace util {

file_reader::file_reader(const char *filename, unsigned padding) {
  ifstream f(filename, ios::binary);
  if (!f)
    throw FileIOException();

  f.seekg(0, ios::end);
  sz = f.tellg();
  f.seekg(0, ios::beg);
  buf = new char[sz + padding];
  f.read(buf, sz);
  memset(buf + sz, 0, padding);
}

file_reader::~file_reader() {
  delete[] buf;
}

#if __cplusplus > 202002L
string get_random_filename(const string &dir, const char *extension) {
  using fs = std::filesystem;

  // there's a low probability of race here
  auto newname = [&]() { return get_random_str(12) + '.' + extension; };
  fs::path path = fs::path(dir) / newname();
  while (fs::exists(path)) {
    path.replace_filename(newname());
  }
  return path;
}
#elif __APPLE__
string get_random_filename(const string &dir, const char *extension) {
  // FIXME: This is bizarre. Seems like we could use mkstemp and have the OS
  // build us a globally-unique file path.
  auto newname = [&]() { return get_random_str(12) + '.' + extension; };

  std::string path = dir + "/" + newname();

  struct stat s;
  while (stat(path.c_str(), &s) == 0) {
    path.resize(dir.size() + 1);
    path += newname();
  }
  return path;
}
#endif
}
