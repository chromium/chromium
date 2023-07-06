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
    const absl::optional<HDRMetadata>& hdr_metadata,
    absl::optional<SkColor4f> color,
    bool is_solid_color,
    absl::optional<Rect> clip_rect)
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
      clip_rect(clip_rect) {}

OverlayPlaneData::~OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(const OverlayPlaneData& other) = default;

}  // namespace gfx
