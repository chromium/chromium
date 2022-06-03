// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_GPU_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_GPU_H_

#include "ui/gfx/gpu_fence_handle.h"

namespace gfx {
enum class SwapResult;
struct PresentationFeedback;
}  // namespace gfx

namespace ui {

// This is a common interface for surfaces created in the GPU process. The
// purpose of this is receiving submission and presentation callbacks from the
// WaylandBufferManagerGpu whenever the browser process has completed presenting
// the buffer.
class WaylandSurfaceGpu {
 public:
  virtual ~WaylandSurfaceGpu() {}

  // Tells the surface the result of the last swap of buffer with the
  // |buffer_id|. After this callback, the previously (before |buffer_id|)
  // submitted buffer may be reused. This is guaranteed to be called
  // in the same order that buffers were submitted.
  virtual void OnSubmission(uint32_t buffer_id,
                            const gfx::SwapResult& swap_result,
                            gfx::GpuFenceHandle release_fence) = 0;

  // Tells the surface the result of the last presentation of buffer with the
  // |buffer_id|. This is guaranteed to be called in the same order that
  // buffers were submitted, and is guaranteed to be called after the
  // corresponding call to |OnSubmission| for this buffer.
  virtual void OnPresentation(uint32_t buffer_id,
                              const gfx::PresentationFeedback& feedback) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_GPU_H_
