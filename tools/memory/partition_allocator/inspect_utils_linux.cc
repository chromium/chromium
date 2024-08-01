// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/inspect_utils.h"

#include <sys/mman.h>

#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "partition_alloc/thread_cache.h"

namespace partition_alloc::tools {

base::ScopedFD OpenProcMem(pid_t pid) {
  std::string path = base::StringPrintf("/proc/%d/mem", pid);
  int fd = open(path.c_str(), O_RDONLY);
  CHECK_NE(fd, -1)
      << "Do you have 0 set in /proc/sys/kernel/yama/ptrace_scope?";

  return base::ScopedFD(fd);
}

RemoteProcessMemoryReader::RemoteProcessMemoryReader(pid_t pid) : pid_(pid) {
  mem_fd_ = OpenProcMem(pid_);
  is_valid_ = mem_fd_.get() != -1;
}

bool RemoteProcessMemoryReader::ReadMemory(uintptr_t remote_address,
                                           size_t size,
                                           char* buffer) {
  if (HANDLE_EINTR(pread(mem_fd_.get(), buffer, size,
                         static_cast<off_t>(remote_address))) ==
      static_cast<ssize_t>(size)) {
    return true;
  }
  return false;
}

base::ScopedFD OpenPagemap(pid_t pid) {
  std::string path = base::StringPrintf("/proc/%d/pagemap", pid);
  int fd = open(path.c_str(), O_RDONLY);
  CHECK_NE(fd, -1)
      << "Do you have 0 set in /proc/sys/kernel/yama/ptrace_scope?";

  return base::ScopedFD(fd);
}

uintptr_t IndexThreadCacheNeedleArray(RemoteProcessMemoryReader& reader,
                                      size_t index) {
  std::vector<base::debug::MappedMemoryRegion> regions;
  DCHECK_LT(index, kThreadCacheNeedleArraySize);

  {
    // Ensures that the mappings are not going to change.
    ScopedSigStopper stop{reader.pid()};

    // There are subtleties when trying to read this file, which we blissfully
    // ignore here. See //base/debug/proc_maps_linux.h for details. We don't use
    // it, since we don't read the maps for ourselves, and everything is already
    // extremely racy. At worst we have to retry.
    LOG(INFO) << "Opening /proc/PID/maps";
    std::string path = base::StringPrintf("/proc/%d/maps", reader.pid());
    auto file = base::File(base::FilePath(path),
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    CHECK(file.IsValid());
    constexpr size_t kMaxFileSize = 10 * (1 << 20);
    std::vector<char> data(kMaxFileSize);
    int bytes_read =
        file.ReadAtCurrentPos(&data[0], static_cast<int>(data.size()) - 1);
    CHECK_GT(bytes_read, 0) << "Cannot read " << path;
    data[bytes_read] = '\0';
    std::string proc_maps(&data[0]);

    LOG(INFO) << "Parsing the maps";
    CHECK(base::debug::ParseProcMaps(proc_maps, &regions));
    LOG(INFO) << "Found " << regions.size() << " regions";
  }

  for (auto& region : regions) {
    using base::debug::MappedMemoryRegion;

    // The array is in .data, meaning that it's mapped from the executable, and
    // has rw-p permissions. For Chrome, .data is quite small, hence the size
    // limit.
    uint8_t expected_permissions = MappedMemoryRegion::Permission::READ |
                                   MappedMemoryRegion::Permission::WRITE |
                                   MappedMemoryRegion::Permission::PRIVATE;
    size_t region_size = region.end - region.start;
    constexpr size_t kMaxRegionSize = 10 * (1 << 20);
    if (region.permissions != expected_permissions ||
        region_size > kMaxRegionSize || region.path.empty()) {
      continue;
    }

    LOG(INFO) << "Found a candidate region between " << std::hex << region.start
              << " and " << region.end << std::dec
              << " (size = " << region.end - region.start
              << ") path = " << region.path;
    // Scan the region, looking for the needles.
    uintptr_t needle_array_candidate[kThreadCacheNeedleArraySize];
    for (uintptr_t address = region.start;
         address < region.end - sizeof(needle_array_candidate);
         address += sizeof(uintptr_t)) {
      bool ok =
          reader.ReadMemory(reinterpret_cast<unsigned long>(address),
                            sizeof(needle_array_candidate),
                            reinterpret_cast<char*>(needle_array_candidate));
      if (!ok) {
        LOG(WARNING) << "Failed to read";
        continue;
      }

      if (needle_array_candidate[0] == kNeedle1 &&
          needle_array_candidate[kThreadCacheNeedleArraySize - 1] == kNeedle2) {
        LOG(INFO) << "Got it! Address = 0x" << std::hex
                  << needle_array_candidate[index];
        return needle_array_candidate[index];
      }
    }
  }

  LOG(ERROR) << "Failed to find the address";
  return 0;
}

}  // namespace partition_alloc::tools
