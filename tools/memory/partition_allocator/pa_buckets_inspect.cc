// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Connects to a running Chrome process, and outputs statistics about its
// bucket usage.
//
// To use this tool, chrome needs to be compiled with the RECORD_ALLOC_INFO
// flag.

#include <algorithm>
#include <cstring>
#include <ios>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/thread_cache.h"

#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::internal::tools {
namespace {

uintptr_t FindAllocInfoAddress(pid_t pid, int mem_fd) {
  return IndexThreadCacheNeedleArray(pid, mem_fd, 2);
}

void DisplayPerBucketData(
    const std::unordered_map<uintptr_t, size_t>& live_allocs) {
  constexpr base::internal::BucketIndexLookup lookup{};
  std::cout << "Per-bucket stats:"
            << "\nIndex\tBucket Size\t#Allocs\tTotal Size\tFragmentation"
            << std::string(80, '-') << "\n";

  size_t alloc_size[kNumBuckets] = {};
  size_t alloc_nums[kNumBuckets] = {};
  size_t total_memory = 0;
  for (const auto& pair : live_allocs) {
    total_memory += pair.second;
    const auto index = base::internal::BucketIndexLookup::GetIndex(pair.second);
    alloc_size[index] += pair.second;
    alloc_nums[index]++;
  }

  for (size_t i = 0; i < kNumBuckets; i++) {
    const auto bucket_size = lookup.bucket_sizes()[i];
    const size_t fragmentation =
        alloc_nums[i] == 0
            ? 0
            : 100 - 100 * alloc_size[i] / (1.0 * bucket_size * alloc_nums[i]);
    std::cout << i << "\t" << bucket_size << "\t\t" << alloc_nums[i] << "\t"
              << (alloc_size[i] / 1024) << "KiB"
              << "\t\t" << fragmentation << "%"
              << "\n";
  }

  std::cout << "\nALL THREADS TOTAL: " << total_memory / 1024 << "kiB\n";
}

}  // namespace
}  // namespace partition_alloc::internal::tools

int main(int argc, char** argv) {
  if (argc < 2) {
    LOG(ERROR) << "Usage:" << argv[0] << " <PID> "
               << "[address. 0 to scan the process memory]";
    return 1;
  }

  int pid = atoi(argv[1]);
  uintptr_t registry_address = 0;

  auto mem_fd = partition_alloc::internal::tools::OpenProcMem(pid);

  if (argc == 3) {
    uint64_t address;
    CHECK(base::StringToUint64(argv[2], &address));
    registry_address = static_cast<uintptr_t>(address);
  } else {
    // Scan the memory.
    registry_address = partition_alloc::internal::tools::FindAllocInfoAddress(
        pid, mem_fd.get());
  }

  CHECK(registry_address);

  auto alloc_info = std::make_unique<partition_alloc::internal::AllocInfo>();
  partition_alloc::internal::tools::ReadMemory(
      mem_fd.get(), registry_address,
      sizeof(partition_alloc::internal::AllocInfo),
      reinterpret_cast<char*>(alloc_info.get()));

  size_t old_index = 0;
  size_t new_index = alloc_info->index;

  std::unordered_map<uintptr_t, size_t> live_allocs = {};
  while (true) {
    using partition_alloc::internal::kAllocInfoSize;
    base::TimeTicks tick = base::TimeTicks::Now();

    for (size_t i = 0; i < (new_index - old_index - 1) % kAllocInfoSize; i++) {
      size_t index = (i + old_index) % kAllocInfoSize;
      const auto& entry = alloc_info->allocs[index];
      if (entry.addr & 0x01) {  // alloc
        uintptr_t addr = entry.addr & ~0x01;
        live_allocs.insert({addr, entry.size});
      } else {  // free
        live_allocs.erase(entry.addr);
      }
    }

    int64_t gather_time_ms = (base::TimeTicks::Now() - tick).InMilliseconds();
    constexpr const char* kClearScreen = "\033[2J\033[1;1H";
    std::cout << kClearScreen << "Time to gather data = " << gather_time_ms
              << "ms\n";
    partition_alloc::internal::tools::DisplayPerBucketData(live_allocs);

    partition_alloc::internal::tools::ReadMemory(
        mem_fd.get(), registry_address,
        sizeof(partition_alloc::internal::AllocInfo),
        reinterpret_cast<char*>(alloc_info.get()));

    old_index = new_index;
    new_index = alloc_info->index;
    usleep(1'000'000);
  }
}
