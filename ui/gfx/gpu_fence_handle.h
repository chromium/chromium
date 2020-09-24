// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_FENCE_HANDLE_H_
#define UI_GFX_GPU_FENCE_HANDLE_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/files/scoped_file.h"
#endif

namespace gfx {

struct GFX_EXPORT GpuFenceHandle {
  GpuFenceHandle(const GpuFenceHandle&) = delete;
  GpuFenceHandle& operator=(const GpuFenceHandle&) = delete;

  GpuFenceHandle();
  GpuFenceHandle(GpuFenceHandle&& other);
  GpuFenceHandle& operator=(GpuFenceHandle&& other);
  ~GpuFenceHandle();

  bool is_null() const;

  // Returns an instance of |handle| which can be sent over IPC. This duplicates
  // the handle so that IPC code can take ownership of it without invalidating
  // |handle| itself.
  GpuFenceHandle Clone() const;

  // owned_fd is defined here for both OS_FUCHSIA and OS_POSIX but all
  // of the handling for owned_fd is only for POSIX. Consider adjusting the
  // defines in the future.
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  base::ScopedFD owned_fd;
#endif
};

}  // namespace gfx

#endif  // UI_GFX_GPU_FENCE_HANDLE_H_
