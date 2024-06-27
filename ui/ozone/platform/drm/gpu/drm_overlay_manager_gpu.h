// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"

namespace ui {

class DrmThreadProxy;

// DrmOverlayManager implementation that runs in the GPU process. PostTasks
// overlay validations requests to the DRM thread.
class DrmOverlayManagerGpu : public DrmOverlayManager {
 public:
  DrmOverlayManagerGpu(DrmThreadProxy* drm_thread_proxy,
                       bool handle_overlays_swap_failure,
                       bool allow_sync_and_real_buffer_page_flip_testing);

  DrmOverlayManagerGpu(const DrmOverlayManagerGpu&) = delete;
  DrmOverlayManagerGpu& operator=(const DrmOverlayManagerGpu&) = delete;

  ~DrmOverlayManagerGpu() override;

 private:
  // DrmOverlayManager:
  void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override;
  std::vector<OverlayStatus> SendOverlayValidationRequestSync(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override;

  void GetHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      HardwareCapabilitiesCallback& receive_callback) override;

  void SetDisplaysConfiguredCallbackIfNecessary();

  void ReceiveOverlayValidationResponse(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      const std::vector<OverlayStatus>& status);

  const raw_ptr<DrmThreadProxy> drm_thread_proxy_;

  bool has_set_displays_configured_callback_ = false;

  base::WeakPtrFactory<DrmOverlayManagerGpu> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_
