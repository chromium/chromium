// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_

#include <memory>

#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/overlay_plane.h"
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

  void QueueOverlayPlane(OverlayPlane plane, BufferId buffer_id);

  // gl::GLSurface:
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
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
  gfx::SurfaceOrigin GetOrigin() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandCheckOrderOfCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandCommitOverlaysCallbacksTest);
  FRIEND_TEST_ALL_PREFIXES(WaylandSurfaceFactoryTest,
                           GbmSurfacelessWaylandGroupOnSubmissionCallbacksTest);

  ~GbmSurfacelessWayland() override;

  // WaylandSurfaceGpu overrides:
  void OnSubmission(BufferId buffer_id,
                    const gfx::SwapResult& swap_result) override;
  void OnPresentation(BufferId buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    // Queues overlay configs to |planes|.
    void ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);
    void Flush();

    bool ready = false;

    // The id of the buffer, which represents this frame.
    BufferId buffer_id = 0;

    // A region of the updated content in a corresponding frame. It's used to
    // advice Wayland which part of a buffer is going to be updated. Passing {0,
    // 0, 0, 0} results in a whole buffer update on the Wayland compositor side.
    gfx::Rect damage_region_ = gfx::Rect();
    // TODO(fangzhoug): This should be changed to support Vulkan.
    std::vector<gl::GLSurfaceOverlay> overlays;
    SwapCompletionCallback completion_callback;
    PresentationCallback presentation_callback;

    bool schedule_planes_succeeded = false;

    // Maps |buffer_id| to an OverlayPlane, used for committing overlays and
    // wait for OnSubmission's.
    base::small_map<std::map<BufferId, OverlayPlane>> planes;
    base::flat_set<BufferId> pending_presentation_buffers;
    gfx::SwapResult swap_result = gfx::SwapResult::SWAP_ACK;
    gfx::PresentationFeedback feedback;
  };

  void MaybeSubmitFrames();

  EGLSyncKHR InsertFence(bool implicit);
  void FenceRetired(PendingFrame* frame);

  // Sets a flag that skips glFlush step in unittests.
  void SetNoGLFlushForTests();

  WaylandBufferManagerGpu* const buffer_manager_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  gfx::AcceleratedWidget widget_;
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;
  std::vector<std::unique_ptr<PendingFrame>> submitted_frames_;
  std::vector<std::unique_ptr<PendingFrame>> pending_presentation_frames_;
  bool has_implicit_external_sync_;
  bool last_swap_buffers_result_ = true;
  bool use_egl_fence_sync_ = true;

  bool no_gl_flush_for_tests_ = false;

  base::WeakPtrFactory<GbmSurfacelessWayland> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfacelessWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
