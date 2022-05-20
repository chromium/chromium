// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/ashmem/ashmem.h"

namespace ipcz::reference_drivers {

void Memory::Mapping::Reset() {
  if (base_address_) {
    munmap(base_address_, size_);
    base_address_ = nullptr;
    size_ = 0;
  }
}

Memory::Memory(size_t size) {
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  const size_t rounded_size = (size + page_size - 1) & (page_size - 1);
  int fd = ashmem_create_region("ipcz-memory", rounded_size);
  ABSL_ASSERT(fd >= 0);
  int err = ashmem_set_prot_region(fd, PROT_READ | PROT_WRITE);
  ABSL_ASSERT(err == 0);
  handle_ = OSHandle(fd);
  size_ = size;
}

Memory::Mapping Memory::Map() {
  ABSL_ASSERT(is_valid());
  void* addr =
      mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, handle_.fd(), 0);
  ABSL_ASSERT(addr && addr != MAP_FAILED);
  return Mapping(addr, size_);
}

}  // namespace ipcz::reference_drivers
