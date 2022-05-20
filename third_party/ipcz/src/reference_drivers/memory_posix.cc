// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

void Memory::Mapping::Reset() {
  if (base_address_) {
    munmap(base_address_, size_);
    base_address_ = nullptr;
    size_ = 0;
  }
}

Memory::Memory(size_t size) {
  int fd = memfd_create("/ipcz/mem", MFD_ALLOW_SEALING);
  ABSL_ASSERT(fd >= 0);

  int result = ftruncate(fd, size);
  ABSL_ASSERT(result == 0);

  result = fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
  ABSL_ASSERT(result == 0);

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
