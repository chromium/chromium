// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gpu_fence_handle.h"

#include "base/notreached.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

namespace gfx {

GpuFenceHandle::GpuFenceHandle() = default;

GpuFenceHandle::GpuFenceHandle(GpuFenceHandle&& other) = default;

GpuFenceHandle& GpuFenceHandle::operator=(GpuFenceHandle&& other) = default;

GpuFenceHandle::~GpuFenceHandle() = default;

bool GpuFenceHandle::is_null() const {
#if defined(OS_POSIX)
  return !owned_fd.is_valid();
#else
  return true;
#endif
}

GpuFenceHandle GpuFenceHandle::Clone() const {
  gfx::GpuFenceHandle handle;
#if defined(OS_POSIX)
  const int duped_handle = HANDLE_EINTR(dup(owned_fd.get()));
  if (duped_handle < 0)
    return GpuFenceHandle();
  handle.owned_fd = base::ScopedFD(duped_handle);
#else
  NOTREACHED();
#endif
  return handle;
}

}  // namespace gfx
