// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/crtc_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

CrtcController::CrtcController(const scoped_refptr<DrmDevice>& drm,
                               uint32_t crtc,
                               uint32_t connector)
    : drm_(drm),
      crtc_(crtc),
      connector_(connector),
      internal_diplay_only_modifiers_(
          {I915_FORMAT_MOD_Y_TILED_CCS, I915_FORMAT_MOD_Yf_TILED_CCS}) {}

CrtcController::~CrtcController() {
  if (!is_disabled_) {
    const std::vector<std::unique_ptr<HardwareDisplayPlane>>& all_planes =
        drm_->plane_manager()->planes();
    for (const auto& plane : all_planes) {
      if (plane->owning_crtc() == crtc_) {
        plane->set_owning_crtc(0);
        plane->set_in_use(false);
      }
    }

    DisableCursor();
    drm_->DisableCrtc(crtc_);
  }
}

bool CrtcController::Modeset(const DrmOverlayPlane& plane,
                             drmModeModeInfo mode) {
  if (!drm_->SetCrtc(crtc_, plane.buffer->opaque_framebuffer_id(),
                     std::vector<uint32_t>(1, connector_), &mode)) {
    PLOG(ERROR) << "Failed to modeset: crtc=" << crtc_
                << " connector=" << connector_
                << " framebuffer_id=" << plane.buffer->opaque_framebuffer_id()
                << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
                << mode.vrefresh;
    return false;
  }

  mode_ = mode;
  is_disabled_ = false;

  // Hold modeset buffer until page flip. This fixes a crash on entering
  // hardware mirror mode in some circumstances (bug 888553).
  // TODO(spang): Fix this better by changing how mirrors are set up (bug
  // 899352).
  modeset_framebuffer_ = plane.buffer;

  return true;
}

bool CrtcController::Disable() {
  if (is_disabled_)
    return true;

  is_disabled_ = true;
  DisableCursor();
  return drm_->DisableCrtc(crtc_);
}

bool CrtcController::AssignOverlayPlanes(HardwareDisplayPlaneList* plane_list,
                                         const DrmOverlayPlaneList& overlays) {
  DCHECK(!is_disabled_);

  const DrmOverlayPlane* primary = DrmOverlayPlane::GetPrimaryPlane(overlays);
  if (primary && !drm_->plane_manager()->ValidatePrimarySize(*primary, mode_)) {
    VLOG(2) << "Trying to pageflip a buffer with the wrong size. Expected "
            << mode_.hdisplay << "x" << mode_.vdisplay << " got "
            << primary->buffer->size().ToString() << " for"
            << " crtc=" << crtc_ << " connector=" << connector_;
    return true;
  }

  if (!drm_->plane_manager()->AssignOverlayPlanes(plane_list, overlays, crtc_,
                                                  this)) {
    PLOG(ERROR) << "Failed to assign overlay planes for crtc " << crtc_;
    return false;
  }

  return true;
}

std::vector<uint64_t> CrtcController::GetFormatModifiers(uint32_t format) {
  std::vector<uint64_t> modifiers =
      drm_->plane_manager()->GetFormatModifiers(crtc_, format);

  display::DisplayConnectionType display_type =
      ui::GetDisplayType(drm_->GetConnector(connector_).get());
  // If this is an external display, remove the modifiers applicable to internal
  // displays only.
  if (display_type != display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
    for (auto modifier : internal_diplay_only_modifiers_) {
      modifiers.erase(std::remove(modifiers.begin(), modifiers.end(), modifier),
                      modifiers.end());
    }
  }

  return modifiers;
}

void CrtcController::SetCursor(uint32_t handle, const gfx::Size& size) {
  if (is_disabled_)
    return;
  if (!drm_->SetCursor(crtc_, handle, size)) {
    PLOG(ERROR) << "drmModeSetCursor: device " << drm_->device_path().value()
                << " crtc " << crtc_ << " handle " << handle << " size "
                << size.ToString();
  }
}

void CrtcController::MoveCursor(const gfx::Point& location) {
  if (is_disabled_)
    return;
  drm_->MoveCursor(crtc_, location);
}

void CrtcController::OnPageFlipComplete() {
  modeset_framebuffer_ = nullptr;
}

void CrtcController::DisableCursor() {
  if (!drm_->SetCursor(crtc_, 0, gfx::Size())) {
    PLOG(ERROR) << "drmModeSetCursor: device " << drm_->device_path().value()
                << " crtc " << crtc_ << " disable";
  }
}

}  // namespace ui
