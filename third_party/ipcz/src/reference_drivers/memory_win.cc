// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <windows.h>

#include "reference_drivers/os_handle.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/safe_math.h"

namespace ipcz::reference_drivers {

void Memory::Mapping::Reset() {
  if (base_address_) {
    ::UnmapViewOfFile(base_address_);
  }
}

Memory::Memory(size_t size) {
  HANDLE h = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                 0, checked_cast<DWORD>(size), nullptr);
  const HANDLE process = ::GetCurrentProcess();
  HANDLE h2;

  // NOTE: DuplicateHandle is called here to remove some permissions from the
  // handle (at least WRITE_DAC, among others). This allows the handle to be
  // duplicated to other processes under strict sandboxing conditions, which may
  // be useful in some test scenarios.
  BOOL ok = ::DuplicateHandle(process, h, process, &h2,
                              FILE_MAP_READ | FILE_MAP_WRITE, FALSE, 0);
  ::CloseHandle(h);
  ABSL_ASSERT(ok);

  handle_ = OSHandle(h2);
  size_ = size;
}

Memory::Mapping Memory::Map() {
  ABSL_ASSERT(is_valid());
  void* addr = ::MapViewOfFile(handle_.handle(), FILE_MAP_READ | FILE_MAP_WRITE,
                               0, 0, size_);
  ABSL_ASSERT(addr);
  return Mapping(addr, size_);
}

}  // namespace ipcz::reference_drivers
