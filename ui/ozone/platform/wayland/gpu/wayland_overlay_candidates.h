// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_CANDIDATES_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_CANDIDATES_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
class WaylandOverlayManager;
class OverlaySurfaceCandidate;

// OverlayCandidatesOzone implementation that delegates overlay decision to
// WaylandOverlayManager.
class WaylandOverlayCandidates : public OverlayCandidatesOzone {
 public:
  WaylandOverlayCandidates(WaylandOverlayManager* manager,
                           gfx::AcceleratedWidget widget);
  WaylandOverlayCandidates(const WaylandOverlayCandidates&) = delete;
  WaylandOverlayCandidates& operator=(const WaylandOverlayCandidates&) = delete;
  ~WaylandOverlayCandidates() override;

  // OverlayCandidatesOzone:
  void CheckOverlaySupport(
      std::vector<OverlaySurfaceCandidate>* candidates) override;

 private:
  const raw_ptr<WaylandOverlayManager> overlay_manager_;  // Not owned.
  const gfx::AcceleratedWidget widget_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_CANDIDATES_H_
