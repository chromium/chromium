// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"

#include <variant>

#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

namespace {

void NotifyOverlayDelegationLimitedCapabilityOnce() {
  static bool logged_once = false;
  if (!logged_once) {
    DLOG(ERROR)
        << "Subpixel accurate position is not available. Only some quads "
           "can be forwarded as overlays.";
    logged_once = true;
  }
}

}  // namespace

WaylandOverlayManager::WaylandOverlayManager(
    WaylandBufferManagerGpu* manager_gpu)
    : manager_gpu_(manager_gpu) {}
WaylandOverlayManager::~WaylandOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
WaylandOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<WaylandOverlayCandidates>(this, widget);
}

void WaylandOverlayManager::SetContextDelegated() {
  is_delegated_context_ = true;
}

void WaylandOverlayManager::CheckOverlaySupport(
    std::vector<OverlaySurfaceCandidate>* candidates,
    gfx::AcceleratedWidget widget) {
  for (auto& candidate : *candidates) {
    bool can_handle = CanHandleCandidate(candidate, widget);

    // CanHandleCandidate() should never return false if the candidate is
    // the primary plane.
    DCHECK(can_handle || candidate.plane_z_order != 0);

    candidate.overlay_handled = can_handle;
  }
}

bool WaylandOverlayManager::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate,
    gfx::AcceleratedWidget widget) const {
  if (!manager_gpu_->SupportsFormat(
          viz::SharedImageFormatToBufferFormat(candidate.format))) {
    return false;
  }

  // TODO( https://crbug.com/331241180 ): Quads can come into overlay processor
  // with 'rect's having position and size as pseudo nonsense values. Here we
  // avoid we fail handling the candidate and avoid passing them through
  // wayland.
  // Wayland 'wl_fixed_t' allows for 23 bits of integer precision. Here we are
  // very conservative and limit to 20 bits.
  constexpr auto kMaxWaylandFixed = 1 << 20;
  constexpr auto kMaxWaylandRect =
      gfx::RectF(-kMaxWaylandFixed, -kMaxWaylandFixed, kMaxWaylandFixed * 2,
                 kMaxWaylandFixed * 2);
  if (!kMaxWaylandRect.Contains(candidate.display_rect)) {
    return false;
  }
  // Passing an empty surface size through wayland will actually clear the size
  // restriction and display the buffer at full size. The function
  // 'set_destination_size' in augmenter will accept empty sizes without
  // protocol error but interprets this as a clear.
  // TODO(crbug.com/40218274) : Move and generalize this fix in wayland
  // host.
  constexpr int kAssumedMaxDeviceScaleFactor = 8;
  if (wl_fixed_from_double(candidate.display_rect.width() /
                           kAssumedMaxDeviceScaleFactor) == 0 ||
      wl_fixed_from_double(candidate.display_rect.height() /
                           kAssumedMaxDeviceScaleFactor) == 0)
    return false;

  if (std::holds_alternative<gfx::OverlayTransform>(candidate.transform)) {
    if (std::get<gfx::OverlayTransform>(candidate.transform) ==
        gfx::OVERLAY_TRANSFORM_INVALID) {
      return false;
    }
  } else if (std::get<gfx::Transform>(candidate.transform).HasPerspective()) {
    // Wayland supports only 2d matrix transforms.
    return false;
  }

  // Wayland doesn't support clip_rect, background_color.
  if (candidate.clip_rect || candidate.background_color.has_value()) {
    return false;
  }

  if (is_delegated_context_) {
    // Subpixel accurate position is not available.
    NotifyOverlayDelegationLimitedCapabilityOnce();
  }

  // Reject candidates that don't fall on a pixel boundary.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  return true;
}

}  // namespace ui
