// Copyright 2022 The Chromium Authors
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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
#include "partition_alloc/partition_root.h"
#include "partition_alloc/thread_cache.h"
#include "tools/memory/partition_allocator/inspect_utils.h"

namespace partition_alloc::tools {
namespace {

using partition_alloc::internal::BucketIndexLookup;
using partition_alloc::internal::kNumBuckets;

constexpr const char* kDumpName = "dump.dat";
constexpr const char* kTmpDumpName = "dump.dat.tmp";

uintptr_t FindAllocInfoAddress(RemoteProcessMemoryReader& reader) {
  return IndexThreadCacheNeedleArray(reader, 2);
}

void DisplayPerBucketData(
    const std::unordered_map<uintptr_t, size_t>& live_allocs,
    size_t allocations,
    double allocations_per_second) {
  constexpr BucketIndexLookup lookup{};
  std::cout << "Per-bucket stats:"
            << "\nIndex\tBucket Size\t#Allocs\tTotal size\tFragmentation"
            << std::string(80, '-') << "\n";

  // Direct mapped allocations have an index of |kNumBuckets|, so add 1 here.
  size_t alloc_size[kNumBuckets + 1] = {};
  size_t alloc_nums[kNumBuckets + 1] = {};
  size_t alt_alloc_size[kNumBuckets + 1] = {};
  size_t alt_alloc_nums[kNumBuckets + 1] = {};
  size_t total_memory = 0;
  for (const auto& pair : live_allocs) {
    const auto requested_size = pair.second;
    total_memory += requested_size;

    // We record 2 distributions below. They can be whatever you want; edit the
    // 2 blocks below to change which distributions are recorded.

    {
      const auto i =
          BucketIndexLookup::GetIndexForNeutralBuckets(requested_size);
      alloc_size[i] += requested_size;
      alloc_nums[i]++;
    }

    {
      const auto j =
          BucketIndexLookup::GetIndexForDenserBuckets(requested_size);
      alt_alloc_size[j] += requested_size;
      alt_alloc_nums[j]++;
    }
  }

  base::File f(base::FilePath(kTmpDumpName),
               base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  // Do not record the direct mapped allocations below, since we only care about
  // the bucket distribution, which direct mapped allocations do not affect.
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
    std::string written = base::StringPrintf(
        "%zu,%lu,%zu,%zu,%zu,%zu\n", i, bucket_size, alloc_nums[i],
        alloc_size[i], alt_alloc_nums[i], alt_alloc_size[i]);
    if (!f.WriteAtCurrentPosAndCheck(base::as_byte_span(written))) {
      std::cerr << "WARNING: Unable to write to temp file, data will be "
                   "stale/missing.\n";
      return;
    }
  }

  rename(kTmpDumpName, kDumpName);

  std::cout << "\nALL THREADS TOTAL: " << total_memory / 1024 << "kiB"
            << "\tLive Allocations = " << allocations
            << "\tAllocations per second = " << allocations_per_second
            << std::endl;
}

}  // namespace
}  // namespace partition_alloc::tools

int main(int argc, char** argv) {
  using partition_alloc::internal::AllocInfo;
  using partition_alloc::internal::kAllocInfoSize;

  if (argc < 2) {
    LOG(ERROR) << "Usage:" << argv[0] << " <PID> "
               << "[address. 0 to scan the process memory]";
    return 1;
  }

  int pid = atoi(argv[1]);
  uintptr_t registry_address = 0;

  partition_alloc::tools::RemoteProcessMemoryReader reader{pid};

  if (argc == 3) {
    uint64_t address;
    CHECK(base::StringToUint64(argv[2], &address));
    registry_address = static_cast<uintptr_t>(address);
  } else {
    // Scan the memory.
    registry_address = partition_alloc::tools::FindAllocInfoAddress(reader);
  }

  CHECK(registry_address);

  auto alloc_info = std::make_unique<AllocInfo>();
  reader.ReadMemory(registry_address, sizeof(AllocInfo),
                    reinterpret_cast<char*>(alloc_info.get()));

  // If this check fails, it means we have overflowed our circular buffer before
  // we had time to start this script. Either the circular buffer needs to be
  // bigger, or the script needs to be started sooner.
  CHECK_LT(alloc_info->index.load(), kAllocInfoSize);

  size_t old_index = 0;
  size_t new_index = alloc_info->index;
  base::TimeTicks last_collection_time = base::TimeTicks::Now();
  double allocations_per_second = 0.;

  std::unordered_map<uintptr_t, size_t> live_allocs = {};
  while (true) {
    base::TimeTicks tick = base::TimeTicks::Now();

    size_t len = old_index < new_index ? new_index - old_index
                                       : kAllocInfoSize - new_index + old_index;

    for (size_t i = 0; i < len; i++) {
      size_t index = i % kAllocInfoSize;
      const auto& entry = alloc_info->allocs[index];
      // Skip nulls.
      if (entry.addr == 0x0) {
        continue;
      }
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
    partition_alloc::tools::DisplayPerBucketData(
        live_allocs, live_allocs.size(), allocations_per_second);

    reader.ReadMemory(registry_address, sizeof(AllocInfo),
                      reinterpret_cast<char*>(alloc_info.get()));
    base::TimeTicks now = base::TimeTicks::Now();
    allocations_per_second = len / (now - last_collection_time).InSecondsF();

    old_index = new_index;
    new_index = alloc_info->index;
    last_collection_time = now;
    usleep(1'000'000);
  }
}
