// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_PLANE_DATA_H_
#define UI_GFX_OVERLAY_PLANE_DATA_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {

struct GFX_EXPORT OverlayPlaneData {
  OverlayPlaneData();
  OverlayPlaneData(int z_order,
                   OverlayTransform plane_transform,
                   const Rect& display_bounds,
                   const RectF& crop_rect,
                   bool enable_blend,
                   const Rect& damage_rect,
                   float opacity,
                   OverlayPriorityHint priority_hint);
  ~OverlayPlaneData();

  // Specifies the stacking order of the plane relative to the main framebuffer
  // located at index 0.
  int z_order = 0;

  // Specifies how the buffer is to be transformed during composition.
  OverlayTransform plane_transform = OverlayTransform::OVERLAY_TRANSFORM_NONE;

  // Pixel bounds within the display to position the image.
  Rect display_bounds;

  // Normalized bounds of the image to be displayed in |display_bounds|.
  RectF crop_rect = RectF(1.f, 1.f);

  // Whether alpha blending should be enabled.
  bool enable_blend = false;

  // Damage on the buffer.
  Rect damage_rect;

  // Opacity of overlay plane. For a blending buffer (|enable_blend|) the total
  // transparency will by = channel alpha * |opacity|.
  float opacity = 1.0f;

  // Hints for overlay prioritization when delegated composition is used.
  OverlayPriorityHint priority_hint = OverlayPriorityHint::kNone;
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_PLANE_DATA_H_
