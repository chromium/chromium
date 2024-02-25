// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

DrmWindow::DrmWindow(gfx::AcceleratedWidget widget,
                     DrmDeviceManager* device_manager,
                     ScreenManager* screen_manager)
    : widget_(widget),
      device_manager_(device_manager),
      screen_manager_(screen_manager) {}

DrmWindow::~DrmWindow() = default;

void DrmWindow::Initialize() {
  TRACE_EVENT1("drm", "DrmWindow::Initialize", "widget", widget_);

  device_manager_->UpdateDrmDevice(widget_, nullptr);
  overlay_validator_ = std::make_unique<DrmOverlayValidator>(this);
}

void DrmWindow::Shutdown() {
  TRACE_EVENT1("drm", "DrmWindow::Shutdown", "widget", widget_);
  device_manager_->RemoveDrmDevice(widget_);
}

gfx::AcceleratedWidget DrmWindow::GetAcceleratedWidget() const {
  return widget_;
}

HardwareDisplayController* DrmWindow::GetController() const {
  return controller_;
}

void DrmWindow::SetBounds(const gfx::Rect& bounds) {
  TRACE_EVENT2("drm", "DrmWindow::SetBounds", "widget", widget_, "bounds",
               bounds.ToString());
  if (bounds_.size() != bounds.size())
    last_submitted_planes_.clear();

  bounds_ = bounds;
  screen_manager_->UpdateControllerToWindowMapping();
}

void DrmWindow::SetCursor(const std::vector<SkBitmap>& bitmaps,
                          const std::optional<gfx::Point>& location,
                          base::TimeDelta frame_delay) {
  cursor_bitmaps_ = bitmaps;
  if (location.has_value()) {
    cursor_location_ = location.value();
  }
  cursor_frame_ = 0;
  cursor_timer_.Stop();

  if (!frame_delay.is_zero()) {
    cursor_timer_.Start(FROM_HERE, frame_delay, this,
                        &DrmWindow::OnCursorAnimationTimeout);
  }

  if (location.has_value()) {
    ResetCursor();
  } else {
    UpdateCursorImage();
  }
}

void DrmWindow::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;
  UpdateCursorLocation();
}

void DrmWindow::SchedulePageFlip(
    std::vector<DrmOverlayPlane> planes,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  if (controller_) {
    const DrmDevice* drm = controller_->GetDrmDevice().get();
    for (const auto& plane : planes) {
      if (plane.buffer && plane.buffer->drm_device() != drm) {
        // Although |force_buffer_reallocation_| is set to true during window
        // bounds update, this may still be needed because of in-flight buffers.
        force_buffer_reallocation_ = true;
        break;
      }
    }
  }

  if (force_buffer_reallocation_) {
    force_buffer_reallocation_ = false;
    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  last_submitted_planes_ = DrmOverlayPlane::Clone(planes);

  if (!controller_) {
    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_ACK,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  controller_->SchedulePageFlip(std::move(planes),
                                std::move(submission_callback),
                                std::move(presentation_callback));
}

OverlayStatusList DrmWindow::TestPageFlip(
    const OverlaySurfaceCandidateList& overlay_params) {
  return overlay_validator_->TestPageFlip(overlay_params,
                                          last_submitted_planes_);
}

void DrmWindow::UpdateCursorImage() {
  if (!controller_)
    return;
  if (cursor_bitmaps_.size()) {
    controller_->SetCursor(cursor_bitmaps_[cursor_frame_]);
  } else {
    // No cursor set.
    controller_->SetCursor(SkBitmap());
  }
}

void DrmWindow::UpdateCursorLocation() {
  if (!controller_)
    return;
  controller_->MoveCursor(cursor_location_);
}

void DrmWindow::ResetCursor() {
  UpdateCursorLocation();
  UpdateCursorImage();
}

void DrmWindow::OnCursorAnimationTimeout() {
  cursor_frame_++;
  cursor_frame_ %= cursor_bitmaps_.size();

  UpdateCursorImage();
}

void DrmWindow::SetController(HardwareDisplayController* controller) {
  if (controller_ == controller)
    return;

  // Force buffer reallocation since the window moved to a different controller.
  // This is required otherwise the GPU will eventually try to render into the
  // buffer currently showing on the old controller (there is no guarantee that
  // the old controller has been updated in the meantime).
  force_buffer_reallocation_ = true;

  controller_ = controller;
  device_manager_->UpdateDrmDevice(
      widget_, controller ? controller->GetDrmDevice() : nullptr);

  // We changed displays, so we want to update the cursor as well.
  ResetCursor();
}

}  // namespace ui
