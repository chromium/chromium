// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crc32_hasher.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <iostream>

#include "base/base64.h"
#include "base/strings/string_split.h"
#include "third_party/zlib/zlib.h"

Crc32Hasher::Crc32Hasher() = default;

Crc32Hasher::~Crc32Hasher() = default;

std::vector<std::string> Crc32Hasher::ParseFileList(
    const std::string& combined_paths) {
  return SplitString(combined_paths, kFilePathDelimiter, base::KEEP_WHITESPACE,
                     base::SPLIT_WANT_ALL);
}

std::optional<uint32_t> Crc32Hasher::HashFile(const std::string& path) {
  // If there is no file at the given path, return std::nullopt.
  int fd = open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    return std::nullopt;
  }

  struct stat stats;
  if (fstat(fd, &stats) == -1) {
    std::cerr << "Could not stat " << path << std::endl;
    exit(1);
  }

  // mmap fails to map empty files, so we just return a checksum of 0.
  if (stats.st_size == 0) {
    close(fd);
    return 0;
  }

  // We should not attempt to hash directories and special files.
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
