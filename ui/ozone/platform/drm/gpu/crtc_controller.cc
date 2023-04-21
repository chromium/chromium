// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/crtc_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

CrtcController::CrtcController(const scoped_refptr<DrmDevice>& drm,
                               uint32_t crtc,
                               uint32_t connector)
    : drm_(drm),
      crtc_(crtc),
      connector_(connector),
      state_(drm->plane_manager()->GetCrtcStateForCrtcId(crtc)) {}

CrtcController::~CrtcController() {
  if (is_enabled()) {
    const std::vector<std::unique_ptr<HardwareDisplayPlane>>& all_planes =
        drm_->plane_manager()->planes();
    for (const auto& plane : all_planes) {
      if (plane->owning_crtc() == crtc_) {
        plane->set_owning_crtc(0);
        plane->set_in_use(false);
      }
    }
  }
}

bool CrtcController::AssignOverlayPlanes(HardwareDisplayPlaneList* plane_list,
                                         const DrmOverlayPlaneList& overlays,
                                         bool is_modesetting) {
  // If we're in the process of modesetting, the CRTC is still disabled.
  // Once the modeset is done, we expect it to be enabled.
  DCHECK(is_modesetting || is_enabled());

  const DrmOverlayPlane* primary = DrmOverlayPlane::GetPrimaryPlane(overlays);
  if (primary &&
      !drm_->plane_manager()->ValidatePrimarySize(*primary, state_->mode)) {
    VLOG(2) << "Trying to pageflip a buffer with the wrong size. Expected "
            << ModeSize(state_->mode).ToString() << " got "
            << primary->buffer->size().ToString() << " for"
            << " crtc=" << crtc_ << " connector=" << connector_;
    return true;
  }

  if (!drm_->plane_manager()->AssignOverlayPlanes(plane_list, overlays,
                                                  crtc_)) {
    return false;
  }

  return true;
}

std::vector<uint64_t> CrtcController::GetFormatModifiers(uint32_t format) {
  return drm_->plane_manager()->GetFormatModifiers(crtc_, format);
}

void CrtcController::SetCursor(uint32_t handle, const gfx::Size& size) {
  if (!is_enabled())
    return;
  if (!drm_->SetCursor(crtc_, handle, size)) {
    PLOG(ERROR) << "drmModeSetCursor: device " << drm_->device_path().value()
                << " crtc " << crtc_ << " handle " << handle << " size "
                << size.ToString();
  }
}

void CrtcController::MoveCursor(const gfx::Point& location) {
  if (!is_enabled())
    return;
  drm_->MoveCursor(crtc_, location);
}

void CrtcController::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("crtc_id", crtc_);
  dict.Add("connector", connector_);

  DrmWriteIntoTraceHelper(state_->mode, dict.AddItem("mode"));
}

}  // namespace ui
