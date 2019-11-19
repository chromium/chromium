// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

class WaylandBufferManagerGpu;

// Returns SkSurface, which must be used to write to a shared memory region. It
// is guaranteed that the returned SkSurface is not backed by the shared memory
// region used by a Wayland compositor. Instead, a new SkSurface will be
// returned and the client can write to it without resulting in tearing.
class WaylandCanvasSurface : public SurfaceOzoneCanvas,
                             public WaylandSurfaceGpu {
 public:
  WaylandCanvasSurface(WaylandBufferManagerGpu* buffer_manager,
                       gfx::AcceleratedWidget widget);
  ~WaylandCanvasSurface() override;

  // SurfaceOzoneCanvas
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  // Internal helper class, which creates a shared memory region, asks the
  // WaylandBufferManager to import a wl_buffer, and creates an SkSurface, which
  // is backed by that shared region.
  class SharedMemoryBuffer;

  void ProcessUnsubmittedBuffers();

  // WaylandSurfaceGpu overrides:
  void OnSubmission(uint32_t buffer_id,
                    const gfx::SwapResult& swap_result) override;
  void OnPresentation(uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  sk_sp<SkSurface> GetNextSurface();
  std::unique_ptr<SharedMemoryBuffer> CreateSharedMemoryBuffer();

  WaylandBufferManagerGpu* const buffer_manager_;
  const gfx::AcceleratedWidget widget_;

  gfx::Size size_;
  std::vector<std::unique_ptr<SharedMemoryBuffer>> buffers_;

  // Contains pending to be submitted buffers. The vector is processed as FIFO.
  std::vector<SharedMemoryBuffer*> unsubmitted_buffers_;

  // Pending buffer that is to be placed into the |unsubmitted_buffers_| to be
  // processed.
  SharedMemoryBuffer* pending_buffer_ = nullptr;

  // Currently used buffer. Set on PresentCanvas() and released on
  // OnSubmission() call.
  SharedMemoryBuffer* current_buffer_ = nullptr;

  // Previously used buffer. Set on OnSubmission().
  SharedMemoryBuffer* previous_buffer_ = nullptr;

  // The id of the current existing buffer. Even though, there can only be one
  // buffer (SkSurface) at a time, the buffer manager on the browser process
  // side requires buffer id to be passed.
  uint32_t buffer_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WaylandCanvasSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
