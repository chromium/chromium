// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <utility>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

Memory::Mapping::Mapping() = default;

Memory::Mapping::Mapping(void* base_address, size_t size)
    : base_address_(base_address), size_(size) {}

Memory::Mapping::Mapping(Mapping&& other)
    : base_address_(std::exchange(other.base_address_, nullptr)),
      size_(std::exchange(other.size_, 0)) {}

Memory::Mapping& Memory::Mapping::operator=(Mapping&& other) {
  Reset();
  base_address_ = std::exchange(other.base_address_, nullptr);
  size_ = std::exchange(other.size_, 0);
  return *this;
}

Memory::Mapping::~Mapping() {
  Reset();
}

Memory::Memory() = default;

Memory::Memory(OSHandle handle, size_t size)
    : handle_(std::move(handle)), size_(size) {}

Memory::Memory(Memory&&) = default;

Memory& Memory::operator=(Memory&&) = default;

Memory::~Memory() = default;

Memory Memory::Clone() {
  ABSL_ASSERT(is_valid());
  return Memory(handle_.Clone(), size_);
}

}  // namespace ipcz::reference_drivers
