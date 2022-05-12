// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class WaylandBufferManagerGpu;

using BufferId = uint32_t;

// A GLSurface for Wayland Ozone platform that uses surfaceless drawing. Drawing
// and displaying happens directly through NativePixmap buffers. CC would call
// into SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfacelessWayland : public gl::SurfacelessEGL,
                              public WaylandSurfaceGpu {
 public:
  GbmSurfacelessWayland(WaylandBufferManagerGpu* buffer_manager,
                        gfx::AcceleratedWidget widget);

  GbmSurfacelessWayland(const GbmSurfacelessWayland&) = delete;
  GbmSurfacelessWayland& operator=(const GbmSurfacelessWayland&) = delete;

  float surface_scale_factor() const { return surface_scale_factor_; }

  void QueueWaylandOverlayConfig(wl::WaylandOverlayConfig config);

  // gl::GLSurface:
  bool ScheduleOverlayPlane(
      gl::GLImage* image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  bool IsOffscreen() override;
  bool SupportsAsyncSwap() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          SwapCompletionCallback completion_callback,
                          PresentationCallback presentation_callback) override;
  EGLConfig GetConfig() override;
  void SetRelyOnImplicitSync() override;
  bool SupportsPlaneGpuFences() const override;
  bool SupportsOverridePlatformSize() const override;
  bool SupportsViewporter() const override;
  gfx::SurfaceOrigin GetOrigin() const override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  void SetForceGlFlushOnSwapBuffers() override;

  BufferId GetOrCreateSolidColorBuffer(SkColor color, const gfx::Size& size);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandCheckOrderOfCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandCommitOverlaysCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandGroupOnSubmissionCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryCompositorV3,
                           SurfaceDamageTest);

  // Holds solid color buffers.
  class SolidColorBufferHolder {
   public:
    SolidColorBufferHolder();
    ~SolidColorBufferHolder();

    BufferId GetOrCreateSolidColorBuffer(
        SkColor color,
        WaylandBufferManagerGpu* buffer_manager);

    void OnSubmission(BufferId buffer_id,
                      WaylandBufferManagerGpu* buffer_manager);
    void EraseBuffers(WaylandBufferManagerGpu* buffer_manager);

   private:
    // Gpu-size holder for the solid color buffers. These are not backed by
    // anything and stored on the gpu side for convenience so that WBHM doesn't
    // become more complex.
    struct SolidColorBuffer {
      SolidColorBuffer(SkColor color, BufferId buffer_id)
          : color(color), buffer_id(buffer_id) {}
      SolidColorBuffer(SolidColorBuffer&& buffer) = default;
      SolidColorBuffer& operator=(SolidColorBuffer&& buffer) = default;
      ~SolidColorBuffer() = default;

      // Color of the buffer.
      SkColor color = SK_ColorWHITE;
      // The buffer id that is mapped with the buffer id created on the browser
      // side.
      BufferId buffer_id = 0;
    };

    std::vector<SolidColorBuffer> inflight_solid_color_buffers_;
    std::vector<SolidColorBuffer> available_solid_color_buffers_;
  };

  ~GbmSurfacelessWayland() override;

  // WaylandSurfaceGpu overrides:
  void OnSubmission(uint32_t frame_id,
                    const gfx::SwapResult& swap_result,
                    gfx::GpuFenceHandle release_fence) override;
  void OnPresentation(uint32_t frame_id,
                      const gfx::PresentationFeedback& feedback) override;

  // PendingFrame here is a post-SkiaRenderer struct that contains overlays +
  // primary plane informations. It is a "compositor frame" on AcceleratedWidget
  // level. This information gets into browser process and overlays are
  // translated to be attached to WaylandSurfaces of the AcceleratedWidget.
  struct PendingFrame {
    explicit PendingFrame(uint32_t frame_id);
    ~PendingFrame();

    // Queues overlay configs to |configs|.
    void ScheduleOverlayPlanes(GbmSurfacelessWayland* surfaceless);
    void Flush();

    // Unique identifier of the frame within this AcceleratedWidget.
    uint32_t frame_id;

    bool ready = false;

    // TODO(fangzhoug): This should be changed to support Vulkan.
    std::vector<gl::GLSurfaceOverlay> overlays;
    std::vector<gfx::OverlayPlaneData> non_backed_overlays;
    SwapCompletionCallback completion_callback;
    PresentationCallback presentation_callback;

    // Merged release fence fd. This is taken as the union of all release
    // fences for a particular OnSubmission.
    bool schedule_planes_succeeded = false;

    std::vector<BufferId> in_flight_color_buffers;
    // Contains |buffer_id|s to gl::GLSurfaceOverlay, used for committing
    // overlays and wait for OnSubmission's.
    std::vector<wl::WaylandOverlayConfig> configs;
  };

  void MaybeSubmitFrames();

  EGLSyncKHR InsertFence(bool implicit);
  void FenceRetired(PendingFrame* frame);

  // Sets a flag that skips glFlush step in unittests.
  void SetNoGLFlushForTests();

  WaylandBufferManagerGpu* const buffer_manager_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  gfx::AcceleratedWidget widget_;

  // PendingFrames that are waiting to be submitted. They can be either ready,
  // waiting for gpu fences, or still scheduling overlays.
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;

  // PendingFrames that are submitted, pending OnSubmission() calls.
  std::vector<std::unique_ptr<PendingFrame>> submitted_frames_;

  // PendingFrames that have received OnSubmission(), pending OnPresentation()
  // calls.
  std::vector<std::unique_ptr<PendingFrame>> pending_presentation_frames_;
  const bool has_implicit_external_sync_;
  const bool has_image_flush_external_;
  bool last_swap_buffers_result_ = true;
  bool use_egl_fence_sync_ = true;

  bool no_gl_flush_for_tests_ = false;
  bool requires_gl_flush_on_swap_buffers_ = false;

  // Scale factor of the current surface.
  float surface_scale_factor_ = 1.f;

  // Holds gpu side reference (buffer_ids) for solid color wl_buffers.
  std::unique_ptr<SolidColorBufferHolder> solid_color_buffers_holder_;

  base::WeakPtrFactory<GbmSurfacelessWayland> weak_factory_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
