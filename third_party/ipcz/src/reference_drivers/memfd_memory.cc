// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memfd_memory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <utility>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

MemfdMemory::Mapping::Mapping() = default;

MemfdMemory::Mapping::Mapping(void* base_address, size_t size)
    : base_address_(base_address), size_(size) {}

MemfdMemory::Mapping::Mapping(Mapping&& other)
    : base_address_(std::exchange(other.base_address_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

MemfdMemory::Mapping& MemfdMemory::Mapping::operator=(Mapping&& other) {
  Reset();
  base_address_ = std::exchange(other.base_address_, nullptr);
  size_ = std::exchange(other.size_, 0);
  return *this;
}

MemfdMemory::Mapping::~Mapping() {
  Reset();
}

void MemfdMemory::Mapping::Reset() {
  if (base_address_) {
    munmap(base_address_, size_);
    base_address_ = nullptr;
    size_ = 0;
  }
}

MemfdMemory::MemfdMemory() = default;

MemfdMemory::MemfdMemory(FileDescriptor fd, size_t size)
    : fd_(std::move(fd)), size_(size) {}

MemfdMemory::MemfdMemory(size_t size) {
  int fd = memfd_create("/ipcz/mem", MFD_ALLOW_SEALING);
  ABSL_ASSERT(fd >= 0);

  int result = ftruncate(fd, size);
  ABSL_ASSERT(result == 0);

  result = fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);
  ABSL_ASSERT(result == 0);

  fd_ = FileDescriptor(fd);
  size_ = size;
}

MemfdMemory::MemfdMemory(MemfdMemory&&) = default;

MemfdMemory& MemfdMemory::operator=(MemfdMemory&&) = default;

MemfdMemory::~MemfdMemory() = default;

MemfdMemory MemfdMemory::Clone() {
  ABSL_ASSERT(is_valid());
  return MemfdMemory(fd_.Clone(), size_);
}

MemfdMemory::Mapping MemfdMemory::Map() {
  ABSL_ASSERT(is_valid());
  void* addr =
      mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_.get(), 0);
  ABSL_ASSERT(addr && addr != MAP_FAILED);
  return Mapping(addr, size_);
}

}  // namespace ipcz::reference_drivers
