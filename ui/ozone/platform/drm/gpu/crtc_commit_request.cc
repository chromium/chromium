// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/crtc_commit_request.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

CrtcCommitRequest::CrtcCommitRequest(uint32_t crtc_id,
                                     uint32_t connector_id,
                                     drmModeModeInfo mode,
                                     gfx::Point origin,
                                     HardwareDisplayPlaneList* plane_list,
                                     DrmOverlayPlaneList overlays,
                                     bool should_enable)
    : should_enable_(should_enable),
      crtc_id_(crtc_id),
      connector_id_(connector_id),
      mode_(mode),
      origin_(origin),
      plane_list_(plane_list),
      overlays_(std::move(overlays)) {
  // Verify that at least one overlay plane is a primary plane if we're enabling
  // a CRTC.
  DCHECK(!should_enable || DrmOverlayPlane::GetPrimaryPlane(overlays_));
}

CrtcCommitRequest::~CrtcCommitRequest() = default;

CrtcCommitRequest::CrtcCommitRequest(const CrtcCommitRequest& other)
    : should_enable_(other.should_enable_),
      crtc_id_(other.crtc_id_),
      connector_id_(other.connector_id_),
      mode_(other.mode_),
      origin_(other.origin_),
      plane_list_(other.plane_list_),
      overlays_(DrmOverlayPlane::Clone(other.overlays_)) {}

void CrtcCommitRequest::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetBoolean("should_enable", should_enable_);
  value->SetInteger("crtc_id", crtc_id_);
  value->SetInteger("connector_id", connector_id_);
  value->SetString("origin", origin_.ToString());
  {
    auto scoped_dict = value->BeginDictionaryScoped("mode");
    DrmAsValueIntoHelper(mode_, value);
  }
  {
    auto scoped_dict =
        value->BeginDictionaryScoped("hardware_display_plane_list");
    if (plane_list_)
      plane_list_->AsValueInto(value);
  }
  {
    auto scoped_array = value->BeginArrayScoped("overlays");
    for (auto& overlay : overlays_) {
      auto scoped_dict = value->AppendDictionaryScoped();
      overlay.AsValueInto(value);
    }
  }
}

// static
CrtcCommitRequest CrtcCommitRequest::EnableCrtcRequest(
    uint32_t crtc_id,
    uint32_t connector_id,
    drmModeModeInfo mode,
    gfx::Point origin,
    HardwareDisplayPlaneList* plane_list,
    DrmOverlayPlaneList overlays) {
  DCHECK(plane_list && !overlays.empty());

  return CrtcCommitRequest(crtc_id, connector_id, mode, origin, plane_list,
                           std::move(overlays), /*should_enable=*/true);
}

// static
CrtcCommitRequest CrtcCommitRequest::DisableCrtcRequest(
    uint32_t crtc_id,
    uint32_t connector_id,
    HardwareDisplayPlaneList* plane_list) {
  return CrtcCommitRequest(crtc_id, connector_id, {}, gfx::Point(), plane_list,
                           DrmOverlayPlaneList(), /*should_enable=*/false);
}

}  // namespace ui
