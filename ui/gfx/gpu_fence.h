// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GPU_FENCE_H_
#define UI_GFX_GPU_FENCE_H_

#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/gpu_fence_handle.h"

extern "C" typedef struct _ClientGpuFence* ClientGpuFence;

namespace base {
class TimeTicks;
}  // namespace base

namespace gfx {

// GpuFence objects own a GpuFenceHandle and release the resources in it when
// going out of scope as appropriate.
class GFX_EXPORT GpuFence {
 public:
  // Constructor takes ownership of the source handle's resources.
  explicit GpuFence(GpuFenceHandle handle);
  GpuFence() = delete;
  GpuFence(GpuFence&& other);
  GpuFence& operator=(GpuFence&& other);

  GpuFence(const GpuFence&) = delete;
  GpuFence& operator=(const GpuFence&) = delete;

  ~GpuFence();

  // Returns a const reference to the underlying GpuFenceHandle
  // owned by GpuFence. If you'd like a duplicated handle for use
  // with IPC, call the Clone method on the returned handle.
  const GpuFenceHandle& GetGpuFenceHandle() const;

  // Casts for use with the GLES interface.
  ClientGpuFence AsClientGpuFence();
  static GpuFence* FromClientGpuFence(ClientGpuFence gpu_fence);

  // Wait for the GpuFence to become ready.
  void Wait();

  enum FenceStatus { kSignaled, kNotSignaled, kInvalid };
  static FenceStatus GetStatusChangeTime(int fd, base::TimeTicks* time);

  base::TimeTicks GetMaxTimestamp() const;

 private:
  gfx::GpuFenceHandle fence_handle_;
};

}  // namespace gfx

#endif  // UI_GFX_GPU_FENCE_H_
