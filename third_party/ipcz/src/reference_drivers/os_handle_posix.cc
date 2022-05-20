// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/os_handle.h"

#include <errno.h>
#include <unistd.h>

#include <utility>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

OSHandle::OSHandle() = default;

OSHandle::OSHandle(int fd) : fd_(fd) {}

OSHandle::OSHandle(OSHandle&& other) : fd_(std::exchange(other.fd_, -1)) {}

OSHandle& OSHandle::operator=(OSHandle&& other) {
  reset();
  fd_ = std::exchange(other.fd_, -1);
  return *this;
}

OSHandle::~OSHandle() {
  reset();
}

void OSHandle::reset() {
  int fd = std::exchange(fd_, -1);
  if (fd >= 0) {
    int rv = close(fd);
    ABSL_ASSERT(rv == 0 || errno == EINTR);
  }
}

OSHandle OSHandle::Clone() const {
  ABSL_ASSERT(is_valid());
  int dupe = dup(fd_);
  return OSHandle(dupe);
}

}  // namespace ipcz::reference_drivers
