// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_LEGACY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_LEGACY_H_

#include <stdint.h>

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace gfx {
struct GpuFenceHandle;
}  // namespace gfx

namespace ui {

class HardwareDisplayPlaneManagerLegacy : public HardwareDisplayPlaneManager {
 public:
  explicit HardwareDisplayPlaneManagerLegacy(DrmDevice* device);

  HardwareDisplayPlaneManagerLegacy(const HardwareDisplayPlaneManagerLegacy&) =
      delete;
  HardwareDisplayPlaneManagerLegacy& operator=(
      const HardwareDisplayPlaneManagerLegacy&) = delete;

  ~HardwareDisplayPlaneManagerLegacy() override;

  // HardwareDisplayPlaneManager:
  bool Commit(CommitRequest commit_request, uint32_t flags) override;

  bool Commit(HardwareDisplayPlaneList* plane_list,
              scoped_refptr<PageFlipRequest> page_flip_request,
              gfx::GpuFenceHandle* release_fence) override;
  bool TestSeamlessMode(int32_t crtc_id, const drmModeModeInfo& mode) override;

  bool DisableOverlayPlanes(HardwareDisplayPlaneList* plane_list) override;

  bool ValidatePrimarySize(const DrmOverlayPlane& primary,
                           const drmModeModeInfo& mode) override;

  void RequestPlanesReadyCallback(
      DrmOverlayPlaneList planes,
      base::OnceCallback<void(DrmOverlayPlaneList)> callback) override;

 protected:
  bool InitializePlanes() override;
  bool SetPlaneData(HardwareDisplayPlaneList* plane_list,
                    HardwareDisplayPlane* hw_plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_id,
                    std::optional<gfx::Point> crtc_offset,
                    const gfx::Rect& src_rect) override;
  bool IsCompatible(HardwareDisplayPlane* plane,
                    const DrmOverlayPlane& overlay,
                    uint32_t crtc_id) const override;
  bool CommitPendingCrtcState(CrtcState& state) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_MANAGER_LEGACY_H_
