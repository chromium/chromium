// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/gfx/color_space.h"

namespace wl {

WaylandOverlayConfig::WaylandOverlayConfig() = default;

WaylandOverlayConfig::WaylandOverlayConfig(WaylandOverlayConfig&& other) =
    default;

WaylandOverlayConfig::WaylandOverlayConfig(const gfx::OverlayPlaneData& data,
                                           std::unique_ptr<gfx::GpuFence> fence,
                                           BufferId buffer_id,
                                           float scale_factor)
    : z_order(data.z_order),
      color_space(data.color_space),
      transform(data.plane_transform),
      buffer_id(buffer_id),
      surface_scale_factor(scale_factor),
      bounds_rect(data.display_bounds),
      crop_rect(data.crop_rect),
      damage_region(data.damage_rect),
      enable_blend(data.enable_blend),
      opacity(data.opacity),
      access_fence_handle(fence ? fence->GetGpuFenceHandle().Clone()
                                : gfx::GpuFenceHandle()),
      priority_hint(data.priority_hint),
      rounded_clip_bounds(data.rounded_corners),
      // Solid color quads are created as wl_buffers. Though, some overlays may
      // have background data passed.
      background_color(data.is_solid_color ? absl::nullopt : data.color),
      clip_rect(data.clip_rect) {}

WaylandOverlayConfig& WaylandOverlayConfig::operator=(
    WaylandOverlayConfig&& other) = default;

WaylandOverlayConfig::~WaylandOverlayConfig() = default;

}  // namespace wl
