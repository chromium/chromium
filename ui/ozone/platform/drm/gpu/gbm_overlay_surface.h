// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_GBM_OVERLAY_SURFACE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_GBM_OVERLAY_SURFACE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/overlay_surface.h"

namespace ui {

class DrmWindowProxy;
class GbmSurfaceFactory;
struct DrmOverlayPlane;

class GbmOverlaySurface : public OverlaySurface {
 public:
  GbmOverlaySurface(std::unique_ptr<DrmWindowProxy> window);
  ~GbmOverlaySurface() override;

  // OverlaySurface:
  void SubmitFrame(std::vector<OverlayPlane> overlay_planes,
                   SubmissionCallback submission_callback,
                   PresentationCallback presentation_callback,
                   ReleaseCallback release_callback) override;

 private:
  struct Frame {
    Frame();
    Frame(Frame&& frame);
    Frame& operator=(Frame&& frame);
    ~Frame();

    std::vector<DrmOverlayPlane> overlay_planes;
    SubmissionCallback submission_callback;
    PresentationCallback presentation_callback;
    ReleaseCallback release_callback;
  };

  void SubmitFrame();

  void OnSubmission(gfx::SwapResult result,
                    std::unique_ptr<gfx::GpuFence> out_fence);
  void OnPresentation(const gfx::PresentationFeedback& presentation_feedback);

  const std::unique_ptr<DrmWindowProxy> window_;
  Frame unsubmitted_frame_;
  Frame submitted_frame_;
  Frame presented_frame_;
  bool have_unsubmitted_frame_ = false;
  bool page_flip_pending_ = false;

  base::WeakPtrFactory<GbmOverlaySurface> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GbmOverlaySurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_GBM_OVERLAY_SURFACE_H_
