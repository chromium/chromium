// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Md5sum implementation for Android. In gzip mode, takes in a list of files,
// and outputs a list of Md5sums in the same order. Otherwise,

#include <dirent.h>
#include <stddef.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/containers/heap_array.h"
#include "base/hash/md5.h"
#include "third_party/zlib/google/compression_utils_portable.h"

namespace {

// Only used in the gzip mode.
const char kFilePathDelimiter = ';';
const int kMD5HashLength = 16;

// Returns whether |path|'s MD5 was successfully written to |digest_string|.
bool MD5Sum(const std::string& path, std::string* digest_string) {
  FILE* fd = fopen(path.c_str(), "rb");
  if (!fd) {
    std::cerr << "Could not open file " << path << std::endl;
    return false;
  }
  base::MD5Context ctx;
  base::MD5Init(&ctx);
  const size_t kBufferSize = 1 << 16;
  auto buf = base::HeapArray<char>::Uninit(kBufferSize);
  size_t len;
  while ((len = fread(buf.data(), 1, buf.size(), fd)) > 0) {
    base::MD5Update(&ctx, std::string_view(buf.data(), len));
  }
  if (ferror(fd)) {
    fclose(fd);
    std::cerr << "Error reading file " << path << std::endl;
    return false;
  }
  fclose(fd);
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  *digest_string = base::MD5DigestToBase16(digest);
  return true;
}

void MakeFileSetHelper(const std::string& path,
                       std::set<std::string>& file_set) {
  DIR* dir = opendir(path.c_str());

  if (!dir) {
    file_set.insert(path);
    return;
  }

  dirent* entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    MakeFileSetHelper(path + '/' + entry->d_name, file_set);
  }
  closedir(dir);
}

// Returns the set of all files contained in |files|. This handles directories
// by walking them recursively. Excludes, .svn directories and file under them.
std::vector<std::string> MakeFileSet(const char** files) {
  std::set<std::string> file_set;
  for (const char** file = files; *file; ++file) {
    MakeFileSetHelper(*file, file_set);
  }

  return std::vector<std::string>(file_set.begin(), file_set.end());
}

std::vector<std::string> StringSplit(const std::string& str, char delim) {
  std::vector<std::string> ret;
  size_t found_idx = str.find(delim);
  size_t start_idx = 0;
  while (found_idx != std::string::npos) {
    ret.push_back(str.substr(start_idx, found_idx - start_idx));
    start_idx = found_idx + 1;
    found_idx = str.find(delim, start_idx);
  }
  ret.push_back(str.substr(start_idx, std::string::npos));
  return ret;
}

std::vector<std::string> MakeFileListFromCompressedList(const char* data) {
  std::string gzipdata;
  // Expected compressed input is using Base64 encoding, we got convert it
  // to a regular string before passing it to zlib.
  base::Base64Decode(std::string_view(data), &gzipdata);

  size_t compressed_size = gzipdata.size();
  unsigned long decompressed_size = zlib_internal::GetGzipUncompressedSize(
      reinterpret_cast<const Bytef*>(gzipdata.c_str()), compressed_size);
  std::string decompressed(decompressed_size, '#');

  // We can skip an extraneous copy by relying on a C++11 std::string guarantee
  // of contiguous memory access to a string.
  zlib_internal::UncompressHelper(
      zlib_internal::WrapperType::GZIP,
      reinterpret_cast<unsigned char*>(&decompressed[0]), &decompressed_size,
      reinterpret_cast<const unsigned char*>(gzipdata.c_str()),
      compressed_size);

  return StringSplit(decompressed, kFilePathDelimiter);
}

}  // namespace

int main(int argc, const char* argv[]) {
  bool gzip_mode = argc >= 2 && strcmp("-gz", argv[1]) == 0;
  if (argc < 2 || (gzip_mode && argc < 3)) {
    std::cerr << "Usage: md5sum <path/to/file_or_dir>... or md5sum "
              << "-gz base64-gzipped-'" << kFilePathDelimiter
              << "'-separated-files" << std::endl;
    return 1;
  }
  std::vector<std::string> files;
  if (gzip_mode) {
    files = MakeFileListFromCompressedList(argv[2]);
  } else {
    files = MakeFileSet(argv + 1);
  }

  bool failed = false;
  std::string digest;
  for (const auto& file : files) {
    if (!MD5Sum(file, &digest)) {
      failed = true;
      continue;
    }
    if (gzip_mode) {
      std::cout << digest.substr(0, kMD5HashLength) << std::endl;
    } else {
      std::cout << digest << "  " << file << std::endl;
    }
  }
  return failed;
}
