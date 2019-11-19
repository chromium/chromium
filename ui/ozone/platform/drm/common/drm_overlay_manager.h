// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {
class OverlaySurfaceCandidate;

// Ozone DRM extension of the OverlayManagerOzone interface. It queries the
// DrmDevice to see if an overlay configuration will work and keeps an MRU cache
// of recent configurations.
class DrmOverlayManager : public OverlayManagerOzone {
 public:
  DrmOverlayManager();
  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // Resets the cache of OverlaySurfaceCandidates and if they can be displayed
  // as an overlay. For use when display configuration changes.
  void ResetCache();

  // Checks if overlay candidates can be displayed as overlays. Modifies
  // |candidates| to indicate if they can.
  void CheckOverlaySupport(std::vector<OverlaySurfaceCandidate>* candidates,
                           gfx::AcceleratedWidget widget);

 protected:
  // Sends a request to see if overlay configuration will work. Implementations
  // should call UpdateCacheForOverlayCandidates() with the response.
  virtual void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) = 0;

  // Perform basic validation to see if |candidate| is a valid request.
  virtual bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate,
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
      base::MRUCache<std::vector<OverlaySurfaceCandidate>,
                     OverlayValidationCacheValue>;

  // Map of each widget to the cache of list of all OverlaySurfaceCandidate
  // instances which have been requested for validation and/or validated.
  std::map<gfx::AcceleratedWidget, OverlayCandidatesListCache>
      widget_cache_map_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_
