// Copyright 2019 The Chromium Authors
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
  virtual ~WaylandSurfaceGpu() = default;

  // Tells the surface the result of the last swap of frame with the |frame_id|.
  // After this callback, the previously (before |frame_id|) submitted buffers
  // may be reused. This is guaranteed to be called in the same order that
  // frames were submitted. If not, there's been a GPU process crash and the
  // stale |frame_id| can be ignored.
  virtual void OnSubmission(uint32_t frame_id,
                            const gfx::SwapResult& swap_result,
                            gfx::GpuFenceHandle release_fence) = 0;

  // Tells the surface the result of the last presentation of buffer with the
  // |frame_id|. This is guaranteed to be called in the same order that frames
  // were submitted, and is guaranteed to be called after the corresponding call
  // to |OnSubmission| for this frame.
  virtual void OnPresentation(uint32_t frame_id,
                              const gfx::PresentationFeedback& feedback) = 0;

  uint32_t next_frame_id() { return ++frame_id_; }

 private:
  uint32_t frame_id_ = 0u;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_GPU_H_
