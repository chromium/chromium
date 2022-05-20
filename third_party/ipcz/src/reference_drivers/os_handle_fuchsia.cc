// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/os_handle.h"

#include <lib/zx/handle.h>
#include <zircon/status.h>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz::reference_drivers {

OSHandle::OSHandle() = default;

OSHandle::OSHandle(zx::handle handle) : handle_(std::move(handle)) {}

OSHandle::OSHandle(OSHandle&& other) = default;

OSHandle& OSHandle::operator=(OSHandle&& other) = default;

OSHandle::~OSHandle() = default;

void OSHandle::reset() {
  handle_.reset();
}

OSHandle OSHandle::Clone() const {
  ABSL_ASSERT(is_valid());

  zx::handle dupe;
  zx_status_t status = handle_.duplicate(ZX_RIGHT_SAME_RIGHTS, &dupe);
  ABSL_ASSERT(status == ZX_OK);
  return OSHandle(std::move(dupe));
}

}  // namespace ipcz::reference_drivers
