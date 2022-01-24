// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_

#include "base/containers/lru_cache.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

class DrmWindow;

class DrmOverlayValidator {
 public:
  DrmOverlayValidator(DrmWindow* window);

  DrmOverlayValidator(const DrmOverlayValidator&) = delete;
  DrmOverlayValidator& operator=(const DrmOverlayValidator&) = delete;

  ~DrmOverlayValidator();

  // Tests if configurations |params| are compatible with |window_| and finds
  // which of these configurations can be promoted to Overlay composition
  // without failing the page flip. It expects |params| to be sorted by z_order.
  OverlayStatusList TestPageFlip(const OverlaySurfaceCandidateList& params,
                                 const DrmOverlayPlaneList& last_used_planes);

 private:
  DrmWindow* const window_;  // Not owned.
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
