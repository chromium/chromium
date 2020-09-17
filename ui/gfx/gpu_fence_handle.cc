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

GpuFenceHandle GpuFenceHandle::Clone() const {
  switch (type) {
    case GpuFenceHandleType::kEmpty:
      break;
    case GpuFenceHandleType::kAndroidNativeFenceSync: {
      gfx::GpuFenceHandle handle;
#if defined(OS_POSIX)
      handle.type = GpuFenceHandleType::kAndroidNativeFenceSync;
      const int duped_handle = HANDLE_EINTR(dup(owned_fd.get()));
      if (duped_handle < 0)
        return GpuFenceHandle();
      handle.owned_fd = base::ScopedFD(duped_handle);
#endif
      return handle;
    }
  }
  NOTREACHED();
  return gfx::GpuFenceHandle();
}

}  // namespace gfx
