// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/memory/partition_allocator/inspect_utils.h"

#include <sys/mman.h>

#include "base/check_op.h"
#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "partition_alloc/thread_cache.h"

namespace partition_alloc::tools {

char* CreateMappingAtAddress(uintptr_t address, size_t size) {
  CHECK_EQ(0u, address % internal::SystemPageSize());
  CHECK_EQ(0u, size % internal::SystemPageSize());
  // Not using MAP_FIXED since it would *overwrite* an existing
  // mapping. Instead, just provide a hint address, which will be used if
  // possible.
  void* local_memory =
      mmap(reinterpret_cast<void*>(address), size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (local_memory == MAP_FAILED) {
    LOG(WARNING) << "Cannot map memory at required address";
    return nullptr;
  }
  if (local_memory != reinterpret_cast<void*>(address)) {
    LOG(WARNING) << "Mapping successful, but not at the desired address. "
                 << "Retry to get better luck with ASLR? 0x" << std::hex
                 << address << " " << local_memory << std::dec;
    munmap(local_memory, size);
    return nullptr;
  }

  return reinterpret_cast<char*>(local_memory);
}

RemoteProcessMemoryReader::~RemoteProcessMemoryReader() = default;

bool RemoteProcessMemoryReader::IsValid() const {
  return is_valid_;
}

char* RemoteProcessMemoryReader::ReadAtSameAddressInLocalMemory(
    uintptr_t address,
    size_t size) {
  // Try to allocate data in the local address space.
  char* local_memory = CreateMappingAtAddress(address, size);
  if (!local_memory)
    return nullptr;

  bool ok = ReadMemory(address, size, reinterpret_cast<char*>(local_memory));

  if (!ok) {
    munmap(local_memory, size);
    return nullptr;
  }

  return reinterpret_cast<char*>(local_memory);
}

}  // namespace partition_alloc::tools
