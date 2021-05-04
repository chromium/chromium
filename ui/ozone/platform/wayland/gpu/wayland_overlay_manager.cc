// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/wayland/gpu/wayland_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

WaylandOverlayManager::WaylandOverlayManager() = default;
WaylandOverlayManager::~WaylandOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
WaylandOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandOverlayCandidates>(this, widget);
}

void WaylandOverlayManager::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates,
    gfx::AcceleratedWidget widget) {
  for (auto& candidate : *candidates) {
    bool can_handle = CanHandleCandidate(candidate, widget);

    // CanHandleCandidate() should never return false if the candidate is
    // the primary plane.
    DCHECK(can_handle || candidate.plane_z_order != 0);

    candidate.overlay_handled = can_handle;
  }
}

bool WaylandOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  // Reject candidates that don't fall on a pixel boundary.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  if (candidate.clip_rect && !candidate.clip_rect->Contains(
                                 gfx::ToNearestRect(candidate.display_rect))) {
    return false;
  }

  return true;
}

}  // namespace ui
