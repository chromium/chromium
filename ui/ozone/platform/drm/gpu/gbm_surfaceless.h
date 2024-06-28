// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_

#include <memory>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface_overlay.h"
#include "ui/gl/presenter.h"
#include "ui/gl/scoped_binders.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace ui {

class DrmWindowProxy;
class GbmSurfaceFactory;

// A GLSurface for GBM Ozone platform that uses surfaceless drawing. Drawing and
// displaying happens directly through NativePixmap buffers. CC would call into
// SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfaceless : public gl::Presenter {
 public:
  GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                 gl::GLDisplayEGL* display,
                 std::unique_ptr<DrmWindowProxy> window,
                 gfx::AcceleratedWidget widget);

  GbmSurfaceless(const GbmSurfaceless&) = delete;
  GbmSurfaceless& operator=(const GbmSurfaceless&) = delete;

  void QueueOverlayPlane(DrmOverlayPlane plane);

  // gl::Presenter:
  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool SupportsPlaneGpuFences() const override;
  void SetRelyOnImplicitSync() override;
  void SetNotifyNonSimpleOverlayFailure() override;
  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override;

 protected:
  ~GbmSurfaceless() override;

  gfx::AcceleratedWidget widget() { return widget_; }
  GbmSurfaceFactory* surface_factory() { return surface_factory_; }

 private:
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);

    bool ready = false;
    gfx::SwapResult swap_result = gfx::SwapResult::SWAP_FAILED;
    std::vector<gl::GLSurfaceOverlay> overlays;
    SwapCompletionCallback completion_callback;
    PresentationCallback presentation_callback;
  };

  void SubmitFrame();

  EGLSyncKHR InsertFence();
  void FenceRetired(PendingFrame* frame);

  void OnSubmission(bool should_handle_non_simple_overlay_failure,
                    gfx::SwapResult result,
                    gfx::GpuFenceHandle release_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);

  EGLDisplay GetEGLDisplay();

  const raw_ptr<GbmSurfaceFactory> surface_factory_;
  const std::unique_ptr<DrmWindowProxy> window_;
  std::vector<DrmOverlayPlane> planes_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  const gfx::AcceleratedWidget widget_;
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;
  std::unique_ptr<PendingFrame> submitted_frame_;
  std::unique_ptr<gfx::GpuFence> submitted_frame_gpu_fence_;
  bool last_swap_buffers_result_ = true;
  bool supports_plane_gpu_fences_ = false;
  bool use_egl_fence_sync_ = true;
  // Determines submission of non-simple overlays (see gfx::OverlayType) should
  // be handled with gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED.
  bool notify_non_simple_overlay_failure_ = false;

  // Conservatively assume we begin on a device that requires
  // explicit synchronization.
  bool is_on_external_drm_device_ = true;

  const raw_ptr<gl::GLDisplayEGL> display_;

  base::WeakPtrFactory<GbmSurfaceless> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
