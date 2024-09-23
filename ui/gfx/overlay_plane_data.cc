// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_plane_data.h"

namespace gfx {

OverlayPlaneData::OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(
    int z_order,
    absl::variant<gfx::OverlayTransform, gfx::Transform> plane_transform,
    const RectF& display_bounds,
    const RectF& crop_rect,
    bool enable_blend,
    const Rect& damage_rect,
    float opacity,
    OverlayPriorityHint priority_hint,
    const gfx::RRectF& rounded_corners,
    const gfx::ColorSpace& color_space,
    const std::optional<HDRMetadata>& hdr_metadata,
    std::optional<SkColor4f> color,
    bool is_solid_color,
    bool is_root_overlay,
    std::optional<Rect> clip_rect,
    gfx::OverlayType overlay_type)
    : z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend),
      damage_rect(damage_rect),
      opacity(opacity),
      priority_hint(priority_hint),
      rounded_corners(rounded_corners),
      color_space(color_space),
      hdr_metadata(hdr_metadata),
      color(color),
      is_solid_color(is_solid_color),
      is_root_overlay(is_root_overlay),
      clip_rect(clip_rect),
      overlay_type(overlay_type) {}

OverlayPlaneData::~OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(const OverlayPlaneData& other) = default;

}  // namespace gfx
