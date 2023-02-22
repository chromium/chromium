// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
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
  bool SupportsOverridePlatformSize() const override;
  bool SupportsAsyncBufferSwap() const override;
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                     gfx::FrameData data) override;

 private:
  // Internal helper class, which creates a shared memory region, asks the
  // WaylandBufferManager to import a wl_buffer, and creates an SkSurface, which
  // is backed by that shared region.
  class SharedMemoryBuffer;

  // Internal implementation of gfx::VSyncProvider.
  class VSyncProvider;

  struct PendingFrame {
    PendingFrame(uint32_t frame_id,
                 const gfx::Size& surface_size,
                 SwapBuffersCallback callback,
                 gfx::FrameData frame_data,
                 SharedMemoryBuffer* frame_buffer);
    ~PendingFrame();

    // Unique identifier of the frame within this AcceleratedWidget.
    const uint32_t frame_id;

    // Current surface size. This is required for the |swap_ack_callback|.
    const gfx::Size surface_size;

    SwapBuffersCallback swap_ack_callback;
    gfx::FrameData data;

    // Buffer associated with this frame. If null, the frame is invalid and
    // requires execution of the |swap_ack_callback| as viz may still request
    // to swap buffers without calling GetCanvas first. In that case, it skips
    // drawing a root render pass and there is nothing to present.
    const raw_ptr<SharedMemoryBuffer, DanglingUntriaged> frame_buffer;
  };

  void MaybeProcessUnsubmittedFrames();

  // WaylandSurfaceGpu overrides:
  void OnSubmission(uint32_t frame_id,
                    const gfx::SwapResult& swap_result,
                    gfx::GpuFenceHandle release_fence) override;
  void OnPresentation(uint32_t frame_id,
                      const gfx::PresentationFeedback& feedback) override;

  sk_sp<SkSurface> GetNextSurface();
  std::unique_ptr<SharedMemoryBuffer> CreateSharedMemoryBuffer();

  const raw_ptr<WaylandBufferManagerGpu> buffer_manager_;
  const gfx::AcceleratedWidget widget_;

  gfx::Size size_;
  float viewport_scale_ = 1.f;
  std::vector<std::unique_ptr<SharedMemoryBuffer>> buffers_;

  // Contains pending to be submitted frames. The vector is processed as FIFO.
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;

  // Currently submitted frame that waits OnSubmission. Set on OnSwapBuffers and
  // release on OnSubmission() call.
  std::unique_ptr<PendingFrame> submitted_frame_;

  // Pending buffer that is to be placed into the |unsubmitted_buffers_| to be
  // processed.
  raw_ptr<SharedMemoryBuffer, DanglingUntriaged> pending_buffer_ = nullptr;

  // Previously used buffer. Set on OnSubmission().
  raw_ptr<SharedMemoryBuffer, DanglingUntriaged> previous_buffer_ = nullptr;

  // Used by the internal VSyncProvider implementation. Set on OnPresentation().
  base::TimeTicks last_timestamp_;
  base::TimeDelta last_interval_;
  bool is_hw_clock_;

  base::WeakPtrFactory<WaylandCanvasSurface> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
