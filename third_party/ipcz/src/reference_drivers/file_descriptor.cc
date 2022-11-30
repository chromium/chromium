// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/file_descriptor.h"

#include <errno.h>
#include <unistd.h>

#include <utility>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

FileDescriptor::FileDescriptor() = default;

FileDescriptor::FileDescriptor(int fd) : fd_(fd) {}

FileDescriptor::FileDescriptor(FileDescriptor&& other)
    : fd_(std::exchange(other.fd_, -1)) {}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) {
  reset();
  fd_ = std::exchange(other.fd_, -1);
  return *this;
}

FileDescriptor::~FileDescriptor() {
  reset();
}

void FileDescriptor::reset() {
  int fd = std::exchange(fd_, -1);
  if (fd >= 0) {
    int rv = close(fd);
    ABSL_ASSERT(rv == 0 || errno == EINTR);
  }
}

FileDescriptor FileDescriptor::Clone() const {
  ABSL_ASSERT(is_valid());
  int dupe = dup(fd_);
  return FileDescriptor(dupe);
}

}  // namespace ipcz::reference_drivers
