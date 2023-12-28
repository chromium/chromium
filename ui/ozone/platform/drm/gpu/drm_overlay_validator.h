// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_

#include <vector>

#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

class DrmWindow;

class DrmOverlayValidator {
 public:
  explicit DrmOverlayValidator(DrmWindow* window);

  DrmOverlayValidator(const DrmOverlayValidator&) = delete;
  DrmOverlayValidator& operator=(const DrmOverlayValidator&) = delete;

  ~DrmOverlayValidator();

  // Tests if configurations of |params| are compatible with |window_| and finds
  // which of these configurations can be promoted to Overlay composition
  // without failing the page flip.
  // If the complete list of planes fails we will remove planes from the end of
  // the test list one at a time. This means that |params| should always have
  // the primary plane at the beginning of the list, and the rest should be
  // sorted based on expected power gain, so less impactful planes are dropped
  // first.
  OverlayStatusList TestPageFlip(const OverlaySurfaceCandidateList& params,
                                 const DrmOverlayPlaneList& last_used_planes);

 private:
  DrmOverlayPlane MakeOverlayPlane(
      const OverlaySurfaceCandidate& param,
      std::vector<scoped_refptr<DrmFramebuffer>>& reusable_buffers);

  const raw_ptr<DrmWindow, DanglingUntriaged> window_;  // Not owned.
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_VALIDATOR_H_
