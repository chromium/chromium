// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
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
  DrmOverlayManager(bool handle_overlays_swap_failure,
                    bool allow_sync_and_real_buffer_page_flip_testing);

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

  // Should be called by the overlay processor to indicate status of the last
  // swap.
  void OnSwapBuffersComplete(gfx::SwapResult swap_result);

  // Should be called by the overlay processor once it gets hardware
  // capabilities.
  void SetSupportedBufferFormats(
      gfx::AcceleratedWidget widget,
      base::flat_set<gfx::BufferFormat> supported_buffer_formats);

  // Should be called by the overlay processor to indicate what overlay types
  // are promoted. This is later used in |OnSwapBuffersComplete| to distinguish
  // overlay types. Can be empty.
  void OnPromotedOverlayTypes(
      std::vector<gfx::OverlayType> promoted_overlay_types);

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

  // Checks if gfx::BufferFormat that overlay candidate requires is supported
  // by hardware.
  bool IsBufferFormatSupported(gfx::BufferFormat required_overlay_buffer_format,
                               gfx::AcceleratedWidget widget) const;

  // Updates the MRU cache for overlay configuration |candidates| with |status|.
  void UpdateCacheForOverlayCandidates(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      const gfx::AcceleratedWidget widget,
      const std::vector<OverlayStatus>& status);

  base::TimeTicks disallow_fullscreen_overlays_end_time() const {
    return disallow_fullscreen_overlays_end_time_;
  }

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

  base::flat_map<gfx::AcceleratedWidget, base::flat_set<gfx::BufferFormat>>
      per_widget_overlay_supported_buffer_formats_;

  // A simple queue of bools that helps to identify buffer swaps.
  base::circular_deque<std::vector<gfx::OverlayType>> in_flight_overlay_types_;

  // Tell the manager to handle overlay swap failures.
  // TODO(b/331237773): Unfortunately, the kHandleOverlaysSwapFailure feature
  // cannot be checked by the this overlay manager in ozone directly as it
  // creates a circular dependency. That's why this control bool is here. Remove
  // this once kHandleOverlaysSwapFailure is removed and DrmOverlayManager is
  // always handling swap failures.
  const bool handle_overlays_swap_failure_;

  // Control variable, which allows to promote fullscreen overlay candidates
  // without drm testing if |handle_overlays_swap_failure_| is true.
  bool allow_skip_fullscreen_overlay_drm_test_ = true;
  // The end time when fullscreen overlay drm test is allowed again. This is
  // set when fullscreen overlay fails and the manager has to start to do
  // drm test of fullscreen overlays again.
  base::TimeTicks disallow_fullscreen_overlays_end_time_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_H_
