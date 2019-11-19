// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

void PrintUsage(char* prog) {
  std::cout << "Usage: " << prog << " FILE" << std::endl
            << "Determine what portion of the FILE is resident in memory."
            << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    PrintUsage(argv[0]);
    return 1;
  }
  char* file_name = argv[1];
  if (!std::string("--help").compare(file_name)) {
    PrintUsage(argv[0]);
    return 0;
  }

  int fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    perror(file_name);
    return 1;
  }

  struct stat st;
  if (fstat(fd, &st)) {
    perror("fstat");
    return 1;
  }

  size_t len = static_cast<size_t>(st.st_size);
  if (len > SIZE_MAX) {
    std::cerr << "File too large" << std::endl;
    return 1;
  }
  void* start_address = mmap(nullptr, len, PROT_READ, MAP_SHARED, fd, 0);
  if (start_address == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  size_t total_pages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
  std::unique_ptr<uint8_t[]> page_residency(new uint8_t[total_pages]);
  if (mincore(start_address, len, page_residency.get())) {
    perror("mincore");
    return 1;
  }

  size_t resident_pages = 0;
  for (size_t i = 0; i < total_pages; ++i) {
    if (page_residency[i] != 0) {
      resident_pages++;
    }
  }

  std::cout << "File size: " << len << ", resident pages: " << resident_pages
            << ", which is " << std::setprecision(4)
            << 100.0 * resident_pages / total_pages << "\% of all pages ("
            << resident_pages / 256 << "MiB)." << std::endl;

  return 0;
}
