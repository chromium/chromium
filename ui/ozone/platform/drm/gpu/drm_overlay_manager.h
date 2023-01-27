// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/hardware_capabilities.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OverlaySurfaceCandidate;

// Ozone DRM extension of the OverlayManagerOzone interface. It queries the
// DrmDevice to see if an overlay configuration will work and keeps an MRU cache
// of recent configurations.
class DrmOverlayManager : public OverlayManagerOzone {
 public:
  explicit DrmOverlayManager(
      bool allow_sync_and_real_buffer_page_flip_testing = true);

  DrmOverlayManager(const DrmOverlayManager&) = delete;
  DrmOverlayManager& operator=(const DrmOverlayManager&) = delete;

  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // Called when notified by the DRM thread of a display configuration change.
  // Resets the cache of OverlaySurfaceCandidates and if they can be displayed
  // as an overlay. Requests an updated HardwareCapabilities for any observing
  // OverlayProcessors.
  void DisplaysConfigured();

  // Checks if overlay candidates can be displayed as overlays. Modifies
  // |candidates| to indicate if they can.
  void CheckOverlaySupport(std::vector<OverlaySurfaceCandidate>* candidates,
                           gfx::AcceleratedWidget widget);

  void StartObservingHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      HardwareCapabilitiesCallback receive_callback);
  void StopObservingHardwareCapabilities(gfx::AcceleratedWidget widget);

  virtual void GetHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      HardwareCapabilitiesCallback& receive_callback) = 0;

  // Should be called by the overlay processor to indicate if a widget has a
  // candidate that requires an overlay. This is to prioritize which display
  // gets the overlay in a multiple display environment.
  void RegisterOverlayRequirement(gfx::AcceleratedWidget widget,
                                  bool requires_overlay);

 protected:
  // Sends a request to see if overlay configuration will work. Implementations
  // should call UpdateCacheForOverlayCandidates() with the response.
  virtual void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) = 0;

  // Similar to SendOverlayValidationRequest() but instead of calling
  // UpdateCacheForOverlayCandidates(), returns the result synchronously.
  virtual std::vector<OverlayStatus> SendOverlayValidationRequestSync(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) = 0;

  // Perform basic validation to see if |candidate| is a valid request.
  bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate,
                          gfx::AcceleratedWidget widget) const;

  // Updates the MRU cache for overlay configuration |candidates| with |status|.
  void UpdateCacheForOverlayCandidates(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      const gfx::AcceleratedWidget widget,
      const std::vector<OverlayStatus>& status);

 private:
  // Value for the request cache, that keeps track of how many times a
  // specific validation has been requested, if there is a GPU validation
  // in flight, and at last the result of the validation.
  struct OverlayValidationCacheValue {
    OverlayValidationCacheValue();
    OverlayValidationCacheValue(OverlayValidationCacheValue&& other);
    ~OverlayValidationCacheValue();
    OverlayValidationCacheValue& operator=(OverlayValidationCacheValue&& other);

    int request_num = 0;
    std::vector<OverlayStatus> status;
  };

  using OverlayCandidatesListCache =
      base::LRUCache<std::vector<OverlaySurfaceCandidate>,
                     OverlayValidationCacheValue>;

  // Map of each widget to the cache of list of all OverlaySurfaceCandidate
  // instances which have been requested for validation and/or validated.
  std::map<gfx::AcceleratedWidget, OverlayCandidatesListCache>
      widget_cache_map_;

  base::flat_set<gfx::AcceleratedWidget> widgets_with_required_overlays_;

  std::map<gfx::AcceleratedWidget, HardwareCapabilitiesCallback>
      hardware_capabilities_callbacks_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_
