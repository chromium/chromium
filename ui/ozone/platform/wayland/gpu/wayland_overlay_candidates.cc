// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_candidates.h"

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

WaylandOverlayCandidates::WaylandOverlayCandidates(
    WaylandOverlayManager* manager,
    gfx::AcceleratedWidget widget)
    : overlay_manager_(manager), widget_(widget) {}

WaylandOverlayCandidates::~WaylandOverlayCandidates() = default;

void WaylandOverlayCandidates::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates) {
  overlay_manager_->CheckOverlaySupport(candidates, widget_);
}

}  // namespace ui
