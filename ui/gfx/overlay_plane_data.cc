// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_plane_data.h"

namespace gfx {

OverlayPlaneData::OverlayPlaneData() = default;

OverlayPlaneData::OverlayPlaneData(int z_order,
                                   OverlayTransform plane_transform,
                                   const Rect& display_bounds,
                                   const RectF& crop_rect,
                                   bool enable_blend,
                                   const Rect& damage_rect,
                                   float opacity,
                                   OverlayPriorityHint priority_hint)
    : z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      enable_blend(enable_blend),
      damage_rect(damage_rect),
      opacity(opacity),
      priority_hint(priority_hint) {}

OverlayPlaneData::~OverlayPlaneData() = default;

}  // namespace gfx
