// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_MAC_H_
#define IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_MAC_H_

#include <mach/mach.h>

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "util/strong_alias.h"

namespace ipcz::reference_drivers {

// The macOS implementation of OSHandle supports wrapping a single Mach send
// right, or a POSIX file descriptor.
class OSHandle {
 public:
  using FileDescriptor = StrongAlias<class FileDescriptorTag, int>;
  using MachSendRight = StrongAlias<class MachSendRightTag, mach_port_t>;
  using MachReceiveRight = StrongAlias<class MachReceiveRightTag, mach_port_t>;
  using Value = absl::
      variant<absl::monostate, FileDescriptor, MachSendRight, MachReceiveRight>;

  OSHandle();
  explicit OSHandle(Value value);

  OSHandle(const OSHandle&) = delete;
  OSHandle& operator=(const OSHandle&) = delete;

  OSHandle(OSHandle&& other);
  OSHandle& operator=(OSHandle&& other);

  ~OSHandle();

  void reset();

  // Duplicates the underlying handle, returning a new OSHandle to wrap it.
  // The handle must be a valid file descriptor or Mach send right. Cloning of
  // of Mach receive rights is not supported.
  OSHandle Clone() const;

  bool is_valid() const {
    return is_valid_fd() || is_valid_mach_send_right() ||
           is_valid_mach_receive_right();
  }

  bool is_valid_fd() const {
    return absl::holds_alternative<FileDescriptor>(value_) && fd() != -1;
  }

  bool is_valid_mach_send_right() const {
    return absl::holds_alternative<MachSendRight>(value_) &&
           mach_send_right() != MACH_PORT_NULL;
  }

  bool is_valid_mach_receive_right() const {
    return absl::holds_alternative<MachReceiveRight>(value_) &&
           mach_receive_right() != MACH_PORT_NULL;
  }

  int fd() const {
    ABSL_ASSERT(is_valid_fd());
    return absl::get<FileDescriptor>(value_).value();
  }

  mach_port_t mach_send_right() const {
    ABSL_ASSERT(is_valid_mach_send_right());
    return absl::get<MachSendRight>(value_).value();
  }

  mach_port_t mach_receive_right() const {
    ABSL_ASSERT(is_valid_mach_receive_right());
    return absl::get<MachReceiveRight>(value_).value();
  }

 private:
  Value value_;
};

}  // namespace ipcz::reference_drivers

#endif  // IPCZ_SRC_REFERENCE_DRIVERS_OS_HANDLE_MAC_H_
