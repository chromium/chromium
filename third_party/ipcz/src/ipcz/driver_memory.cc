// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_memory.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

#include "ipcz/ipcz.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

DriverMemory::DriverMemory() = default;

DriverMemory::DriverMemory(DriverObject memory) {
  if (memory.is_valid()) {
    IpczSharedMemoryInfo info = {.size = sizeof(info)};
    const IpczResult result = memory.driver()->GetSharedMemoryInfo(
        memory.handle(), IPCZ_NO_FLAGS, nullptr, &info);
    if (result == IPCZ_RESULT_OK) {
      memory_ = std::move(memory);
      size_ = info.region_num_bytes;
    }
  }
}

DriverMemory::DriverMemory(const IpczDriver& driver, size_t num_bytes)
    : size_(num_bytes) {
  ABSL_ASSERT(num_bytes > 0);
  IpczDriverHandle handle;
  const IpczResult result =
      driver.AllocateSharedMemory(num_bytes, IPCZ_NO_FLAGS, nullptr, &handle);
  if (result == IPCZ_RESULT_OK) {
    memory_ = DriverObject(driver, handle);
  }
}

DriverMemory::DriverMemory(DriverMemory&& other) = default;

DriverMemory& DriverMemory::operator=(DriverMemory&& other) = default;

DriverMemory::~DriverMemory() = default;

DriverMemory DriverMemory::Clone() {
  ABSL_HARDENING_ASSERT(is_valid());

  IpczDriverHandle handle;
  const IpczResult result = memory_.driver()->DuplicateSharedMemory(
      memory_.handle(), 0, nullptr, &handle);
  if (result != IPCZ_RESULT_OK) {
    return DriverMemory();
  }

  return DriverMemory(DriverObject(*memory_.driver(), handle));
}

DriverMemoryMapping DriverMemory::Map() {
  if (!is_valid()) {
    return DriverMemoryMapping();
  }

  volatile void* address;
  IpczDriverHandle mapping_handle;
  IpczResult result = memory_.driver()->MapSharedMemory(
      memory_.handle(), 0, nullptr, &address, &mapping_handle);
  if (result != IPCZ_RESULT_OK) {
    return DriverMemoryMapping();
  }

  // TODO(https://crbug.com/1451717): Propagate the volatile qualifier on
  // `address`.
  return DriverMemoryMapping(*memory_.driver(), mapping_handle,
                             const_cast<void*>(address), size_);
}

DriverMemoryWithMapping::DriverMemoryWithMapping() = default;

DriverMemoryWithMapping::DriverMemoryWithMapping(DriverMemory memory,
                                                 DriverMemoryMapping mapping)
    : memory(std::move(memory)), mapping(std::move(mapping)) {}

DriverMemoryWithMapping::DriverMemoryWithMapping(DriverMemoryWithMapping&&) =
    default;

DriverMemoryWithMapping& DriverMemoryWithMapping::operator=(
    DriverMemoryWithMapping&&) = default;

DriverMemoryWithMapping::~DriverMemoryWithMapping() = default;

}  // namespace ipcz
