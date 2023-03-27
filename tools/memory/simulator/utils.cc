// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/simulator/utils.h"

#include "base/memory/page_size.h"

namespace memory_simulator {

double BytesToGB(uint64_t bytes) {
  return static_cast<double>(bytes) / (1024 * 1024 * 1024);
}

double BytesToMB(uint64_t bytes) {
  return static_cast<double>(bytes) / (1024 * 1024);
}

double PagesToGB(uint64_t pages) {
  return BytesToGB(pages * base::GetPageSize());
}

double PagesToMB(uint64_t pages) {
  return BytesToMB(pages * base::GetPageSize());
}

int64_t MBToPages(uint64_t mb) {
  return mb * 1024 * 1024 / base::GetPageSize();
}

double PagesToMBPerSec(int64_t pages_begin,
                       int64_t pages_end,
                       base::TimeDelta duration) {
  return PagesToMB(pages_end - pages_begin) / duration.InSecondsF();
}

}  // namespace memory_simulator
