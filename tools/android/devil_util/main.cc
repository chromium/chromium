// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Takes in a list of files and outputs a list of CRC32s in the same order.
// If a file does not exist, outputs a blank line for it.
// It historically used md5, but CRC32 is faster and exists in zlib already.

#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_split.h"
#include "third_party/zlib/google/compression_utils_portable.h"
#include "third_party/zlib/zlib.h"

namespace {

const std::string kFilePathDelimiter = ":";

// If |path| does not exist, return std::nullopt.
// Otherwise, return the checksum obtained by hashing the file at |path|.
std::optional<uint32_t> HashFile(const std::string& path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    // It is normal to list files that do not exist. No need for error message.
    return std::nullopt;
  }
  struct stat stats;
  if (fstat(fd, &stats) == -1) {
    std::cerr << "Could not stat " << path << std::endl;
    exit(1);
  }
  // mmap fails to map empty files.
  if (stats.st_size == 0) {
    close(fd);
    return 0;
  }
  // Don't try to hash directories or special files.
  if (!S_ISREG(stats.st_mode) && !S_ISLNK(stats.st_mode)) {
    close(fd);
    return -1;
  }
  void* file_data = mmap(nullptr, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (file_data == MAP_FAILED) {
    std::cerr << "Could not mmap " << path << std::endl;
    exit(1);
  }
  close(fd);

  uint32_t hash =
      crc32(0L, reinterpret_cast<const Bytef*>(file_data), stats.st_size);
  munmap(file_data, stats.st_size);
  return hash;
}

std::vector<std::string> MakeFileListFromCompressedList(const char* data) {
  std::string gzipdata;
  // Expected compressed input is using Base64 encoding, we got convert it
  // to a regular string before passing it to zlib.
  base::Base64Decode(std::string_view(data), &gzipdata);

  size_t compressed_size = gzipdata.size();
  unsigned long decompressed_size = zlib_internal::GetGzipUncompressedSize(
      reinterpret_cast<const Bytef*>(gzipdata.c_str()), compressed_size);
  std::string decompressed(decompressed_size, '\0');

  // We can skip an extraneous copy by relying on a C++11 std::string guarantee
  // of contiguous memory access to a string.
  zlib_internal::UncompressHelper(
      zlib_internal::WrapperType::GZIP,
      reinterpret_cast<unsigned char*>(&decompressed[0]), &decompressed_size,
      reinterpret_cast<const unsigned char*>(gzipdata.c_str()),
      compressed_size);

  return SplitString(decompressed, kFilePathDelimiter, base::KEEP_WHITESPACE,
                     base::SPLIT_WANT_ALL);
}

}  // namespace

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " hash" << std::endl;
    return 1;
  }

  std::string command = argv[1];
  if (command == "hash") {
    if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " hash base64-gzipped-'"
                << kFilePathDelimiter << "'-separated-files" << std::endl;
      std::cerr << "E.g.: " << argv[0]
                << " hash $(echo -n path1:path2 | gzip | base64)" << std::endl;
      return 1;
    }

    std::vector<std::string> files = MakeFileListFromCompressedList(argv[2]);

    for (const auto& file : files) {
      std::optional<uint32_t> hash = HashFile(file);
      if (!hash.has_value()) {
        std::cout << "\n";  // Blank line for missing file.
      } else {
        std::cout << std::hex << hash.value() << "\n";
      }
    }
    return 0;
  }

  else {
    std::cerr << "Usage: " << argv[0] << " hash" << std::endl;
    return 1;
  }
}
