// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

class SkCanvas;

namespace ui {

class WaylandBufferManagerGpu;

// WaylandCanvasSurface creates an SkCanvas whose contents is backed by a shared
// memory region. The shared memory region is registered with the Wayland server
// as a wl_buffer.
//
// Basic control flow:
//   1. WaylandCanvasSurface creates an anonymous shared memory region.
//   2. WaylandCanvasSurface creates an SkCanvas that rasters directly into
//   this shared memory region.
//   3. WaylandCanvasSurface registers the shared memory region with the
//   WaylandServer via IPC through WaylandBufferManagerGpu and
//   WaylandBufferManagerHost. See
//   WaylandBufferManagerHost::CreateShmBasedBuffer. This creates a wl_buffer
//   object in the browser process.
//   4. WaylandCanvasSurface::CommitBuffer simply routes via IPC through the
//   browser process to the Wayland server. It is not safe to modify the shared
//   memory region in (1) until OnSubmission/OnPresentation callbacks are
//   received.
class WaylandCanvasSurface : public SurfaceOzoneCanvas,
                             public WaylandSurfaceGpu {
 public:
  WaylandCanvasSurface(WaylandBufferManagerGpu* buffer_manager,
                       gfx::AcceleratedWidget widget);

  WaylandCanvasSurface(const WaylandCanvasSurface&) = delete;
  WaylandCanvasSurface& operator=(const WaylandCanvasSurface&) = delete;

  ~WaylandCanvasSurface() override;

  // SurfaceOzoneCanvas

  // GetCanvas() returns an SkCanvas whose shared memory region is not being
  // used by Wayland. If no such SkCanvas is available, a new one is created.
  SkCanvas* GetCanvas() override;
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  void SetGpuMainRunner(
      scoped_refptr<base::SequencedTaskRunner> gpu_main_runner) override;

 private:
  // Internal helper class, which creates a shared memory region, asks the
  // WaylandBufferManager to import a wl_buffer, and creates an SkSurface, which
  // is backed by that shared region.
  class SharedMemoryBuffer;

  void ProcessUnsubmittedBuffers();

  // WaylandSurfaceGpu overrides:
  void OnSubmission(uint32_t buffer_id,
                    const gfx::SwapResult& swap_result,
                    gfx::GpuFenceHandle release_fence) override;
  void OnPresentation(uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  void OnSubmissionOnVizThread(uint32_t buffer_id,
                               const gfx::SwapResult& swap_result,
                               gfx::GpuFenceHandle release_fence);
  void OnPresentationOnVizThread(uint32_t buffer_id,
                                 const gfx::PresentationFeedback& feedback);

  sk_sp<SkSurface> GetNextSurface();
  std::unique_ptr<SharedMemoryBuffer> CreateSharedMemoryBuffer();

  void RegisterSurfaceOnGpuMainThread();

  WaylandBufferManagerGpu* const buffer_manager_;
  const gfx::AcceleratedWidget widget_;
  // The thread where this object is created on.
  scoped_refptr<base::SequencedTaskRunner> viz_thread_;

  // All the WaylandBufferManagerGpu requests must be sent to the main gpu
  // thread as the mojo pipe it sets uses the main thread for incoming and
  // outgoing messages.
  scoped_refptr<base::SequencedTaskRunner> gpu_main_runner_;

  gfx::Size size_;
  float viewport_scale_ = 1.f;
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

  base::WeakPtrFactory<WaylandCanvasSurface> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
