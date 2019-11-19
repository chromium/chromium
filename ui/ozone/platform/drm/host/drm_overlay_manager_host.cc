// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager_host.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayManagerHost::DrmOverlayManagerHost(
    GpuThreadAdapter* proxy,
    DrmWindowHostManager* window_manager)
    : proxy_(proxy), window_manager_(window_manager) {
  proxy_->RegisterHandlerForDrmOverlayManager(this);
}

DrmOverlayManagerHost::~DrmOverlayManagerHost() {
  proxy_->UnRegisterHandlerForDrmOverlayManager();
}

void DrmOverlayManagerHost::GpuSentOverlayResult(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& candidates,
    const OverlayStatusList& returns) {
  TRACE_EVENT_ASYNC_END0("hwoverlays",
                         "DrmOverlayManagerHost::SendOverlayValidationRequest",
                         this);
  UpdateCacheForOverlayCandidates(candidates, widget, returns);
}

void DrmOverlayManagerHost::SendOverlayValidationRequest(
    const OverlaySurfaceCandidateList& candidates,
    gfx::AcceleratedWidget widget) {
  if (!proxy_->IsConnected())
    return;
  TRACE_EVENT_ASYNC_BEGIN0(
      "hwoverlays", "DrmOverlayManagerHost::SendOverlayValidationRequest",
      this);
  proxy_->GpuCheckOverlayCapabilities(widget, candidates);
}

bool DrmOverlayManagerHost::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (!DrmOverlayManager::CanHandleCandidate(candidate, widget))
    return false;

  if (candidate.plane_z_order != 0) {
    // It is possible that the cc rect we get actually falls off the edge of
    // the screen. Usually this is prevented via things like status bars
    // blocking overlaying or cc clipping it, but in case it wasn't properly
    // clipped (since GL will render this situation fine) just ignore it
    // here. This should be an extremely rare occurrence.
    DrmWindowHost* window = window_manager_->GetWindow(widget);
    if (!window->GetBounds().Contains(
            gfx::ToNearestRect(candidate.display_rect))) {
      return false;
    }
  }

  return true;
}

}  // namespace ui
