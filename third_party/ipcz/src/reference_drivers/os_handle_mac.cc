// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/os_handle.h"

#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <unistd.h>

#include <utility>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

OSHandle::OSHandle() = default;

OSHandle::OSHandle(Value value) : value_(value) {}

OSHandle::OSHandle(OSHandle&& other)
    : value_(std::exchange(other.value_, {})) {}

OSHandle& OSHandle::operator=(OSHandle&& other) {
  reset();
  value_ = std::exchange(other.value_, {});
  return *this;
}

OSHandle::~OSHandle() {
  reset();
}

void OSHandle::reset() {
  if (is_valid_fd()) {
    int rv = close(fd());
    ABSL_ASSERT(rv == 0 || errno == EINTR);
  } else if (is_valid_mach_send_right()) {
    kern_return_t kr =
        mach_port_deallocate(mach_task_self(), mach_send_right());
    ABSL_ASSERT(kr == KERN_SUCCESS);
  } else if (is_valid_mach_receive_right()) {
    kern_return_t kr = mach_port_mod_refs(
        mach_task_self(), mach_receive_right(), MACH_PORT_RIGHT_RECEIVE, -1);
    ABSL_ASSERT(kr == KERN_SUCCESS);
  } else if (is_valid_mach_port_set()) {
    kern_return_t kr = mach_port_mod_refs(
        mach_task_self(), mach_receive_right(), MACH_PORT_RIGHT_PORT_SET, -1);
    ABSL_ASSERT(kr == KERN_SUCCESS);
  }

  value_ = {};
}

OSHandle OSHandle::Clone() const {
  ABSL_ASSERT(is_valid());

  // Cloning of receive rights or port sets is not supported.
  ABSL_ASSERT(!is_mach_receive_right() && !is_mach_port_set());

  if (is_valid_fd()) {
    int duped_fd = dup(fd());
    ABSL_ASSERT(duped_fd >= 0);
    return OSHandle(FileDescriptor(duped_fd));
  }

  if (is_valid_mach_send_right()) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_send_right(),
                                          MACH_PORT_RIGHT_SEND, 1);
    if (kr != KERN_SUCCESS) {
      return OSHandle();
    }
    return OSHandle(MachSendRight(mach_send_right()));
  }

  return {};
}

}  // namespace ipcz::reference_drivers
