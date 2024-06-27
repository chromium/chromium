// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_CANDIDATES_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_CANDIDATES_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

class DrmOverlayManager;
class OverlaySurfaceCandidate;

// OverlayCandidatesOzone implementation that delegates decisions to
// DrmOverlayManager.
class DrmOverlayCandidates : public OverlayCandidatesOzone {
 public:
  DrmOverlayCandidates(DrmOverlayManager* manager,
                       gfx::AcceleratedWidget widget);

  DrmOverlayCandidates(const DrmOverlayCandidates&) = delete;
  DrmOverlayCandidates& operator=(const DrmOverlayCandidates&) = delete;

  ~DrmOverlayCandidates() override;

  // OverlayCandidatesOzone:
  void CheckOverlaySupport(
      std::vector<OverlaySurfaceCandidate>* candidates) override;
  void ObserveHardwareCapabilities(
      HardwareCapabilitiesCallback receive_callback) override;
  void RegisterOverlayRequirement(bool requires_overlay) override;
  void OnSwapBuffersComplete(gfx::SwapResult swap_result) override;
  void SetSupportedBufferFormats(
      base::flat_set<gfx::BufferFormat> supported_buffer_formats) override;
  void NotifyOverlayPromotion(
      std::vector<gfx::OverlayType> promoted_overlay_types) override;

 private:
  const raw_ptr<DrmOverlayManager> overlay_manager_;  // Not owned.
  const gfx::AcceleratedWidget widget_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_CANDIDATES_H_
