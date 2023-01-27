// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_overlay_surface.h"

#include <unistd.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/public/overlay_plane.h"

namespace ui {

GbmOverlaySurface::GbmOverlaySurface(std::unique_ptr<DrmWindowProxy> window)
    : window_(std::move(window)) {}

GbmOverlaySurface::~GbmOverlaySurface() = default;

GbmOverlaySurface::Frame::Frame() = default;

GbmOverlaySurface::Frame::Frame(GbmOverlaySurface::Frame&& frame) = default;

GbmOverlaySurface::Frame& GbmOverlaySurface::Frame::operator=(
    GbmOverlaySurface::Frame&& frame) = default;

GbmOverlaySurface::Frame::~Frame() = default;

void GbmOverlaySurface::SubmitFrame(std::vector<OverlayPlane> overlay_planes,
                                    SubmissionCallback submission_callback,
                                    PresentationCallback presentation_callback,
                                    ReleaseCallback release_callback) {
  DCHECK(!have_unsubmitted_frame_);
  Frame unsubmitted_frame;
  unsubmitted_frame.overlay_planes.reserve(overlay_planes.size());
  for (auto& plane : overlay_planes) {
    unsubmitted_frame.overlay_planes.emplace_back(
        static_cast<GbmPixmap*>(plane.pixmap.get())->framebuffer(),
        plane.overlay_plane_data, std::move(plane.gpu_fence));
  }
  unsubmitted_frame.submission_callback = std::move(submission_callback);
  unsubmitted_frame.presentation_callback = std::move(presentation_callback);
  unsubmitted_frame.release_callback = std::move(release_callback);

  unsubmitted_frame_ = std::move(unsubmitted_frame);
  have_unsubmitted_frame_ = true;

  SubmitFrame();
}

void GbmOverlaySurface::SubmitFrame() {
  if (!have_unsubmitted_frame_ || page_flip_pending_)
    return;

  window_->SchedulePageFlip(std::move(unsubmitted_frame_.overlay_planes),
                            base::BindOnce(&GbmOverlaySurface::OnSubmission,
                                           weak_ptr_factory_.GetWeakPtr()),
                            base::BindOnce(&GbmOverlaySurface::OnPresentation,
                                           weak_ptr_factory_.GetWeakPtr()));
  page_flip_pending_ = true;
}

void GbmOverlaySurface::OnSubmission(gfx::SwapResult swap_result,
                                     gfx::GpuFenceHandle release_fence) {
  DCHECK(page_flip_pending_);
  DCHECK(have_unsubmitted_frame_);

  have_unsubmitted_frame_ = false;
  submitted_frame_ = std::move(unsubmitted_frame_);

  std::move(submitted_frame_.submission_callback).Run(swap_result);
}

void GbmOverlaySurface::OnPresentation(
    const gfx::PresentationFeedback& presentation_feedback) {
  DCHECK(page_flip_pending_);

  page_flip_pending_ = false;
  Frame previous_frame = std::move(presented_frame_);
  presented_frame_ = std::move(submitted_frame_);

  // Release buffers from previously presented frame back to client.
  if (!previous_frame.release_callback.is_null())
    std::move(previous_frame.release_callback).Run();

  std::move(presented_frame_.presentation_callback).Run(presentation_feedback);

  SubmitFrame();
}

}  // namespace ui
