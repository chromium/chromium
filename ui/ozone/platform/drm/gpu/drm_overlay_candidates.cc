// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_candidates.h"

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayCandidates::DrmOverlayCandidates(DrmOverlayManager* manager,
                                           gfx::AcceleratedWidget widget)
    : overlay_manager_(manager), widget_(widget) {}

DrmOverlayCandidates::~DrmOverlayCandidates() = default;

void DrmOverlayCandidates::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates) {
  overlay_manager_->CheckOverlaySupport(candidates, widget_);
}

}  // namespace ui
