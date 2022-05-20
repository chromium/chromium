// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/os_handle.h"

#include <windows.h>

#include <utility>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

OSHandle::OSHandle() = default;

OSHandle::OSHandle(HANDLE handle) : handle_(handle) {}

OSHandle::OSHandle(OSHandle&& other)
    : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)) {}

OSHandle& OSHandle::operator=(OSHandle&& other) {
  reset();
  handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
  return *this;
}

OSHandle::~OSHandle() {
  reset();
}

void OSHandle::reset() {
  HANDLE handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
  if (handle != INVALID_HANDLE_VALUE) {
    ::CloseHandle(handle);
  }
}

OSHandle OSHandle::Clone() const {
  ABSL_ASSERT(is_valid());

  HANDLE dupe;
  BOOL result =
      ::DuplicateHandle(::GetCurrentProcess(), handle_, ::GetCurrentProcess(),
                        &dupe, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (!result) {
    return OSHandle();
  }

  ABSL_ASSERT(dupe != INVALID_HANDLE_VALUE);
  return OSHandle(dupe);
}

}  // namespace ipcz::reference_drivers
