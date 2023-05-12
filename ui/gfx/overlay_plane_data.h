// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_PLANE_DATA_H_
#define UI_GFX_OVERLAY_PLANE_DATA_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {

struct GFX_EXPORT OverlayPlaneData {
  OverlayPlaneData();
  OverlayPlaneData(int z_order,
                   OverlayTransform plane_transform,
                   const RectF& display_bounds,
                   const RectF& crop_rect,
                   bool enable_blend,
                   const Rect& damage_rect,
                   float opacity,
                   OverlayPriorityHint priority_hint,
                   const gfx::RRectF& rounded_corners,
                   const gfx::ColorSpace& color_space,
                   const absl::optional<HDRMetadata>& hdr_metadata,
                   absl::optional<SkColor4f> color = absl::nullopt,
                   bool is_solid_color = false,
                   absl::optional<Rect> clip_rect = absl::nullopt);
  ~OverlayPlaneData();

  OverlayPlaneData(const OverlayPlaneData& other);

  // Specifies the stacking order of the plane relative to the main framebuffer
  // located at index 0.
  int z_order = 0;

  // Specifies how the buffer is to be transformed during composition.
  OverlayTransform plane_transform = OverlayTransform::OVERLAY_TRANSFORM_NONE;

  // Bounds within the display to position the image in pixel coordinates. They
  // are sent as floating point rect as some backends such as Wayland are able
  // to do delegated compositing with sub-pixel accurate positioning.
  RectF display_bounds;

  // Normalized bounds of the image to be displayed in |display_bounds|.
  RectF crop_rect = RectF(1.f, 1.f);

  // Whether alpha blending should be enabled.
  bool enable_blend = false;

  // Damage in viz::Display space, the same space as |display_bounds|;
  Rect damage_rect;

  // Opacity of overlay plane. For a blending buffer (|enable_blend|) the total
  // transparency will by = channel alpha * |opacity|.
  float opacity = 1.0f;

  // Hints for overlay prioritization when delegated composition is used.
  OverlayPriorityHint priority_hint = OverlayPriorityHint::kNone;

  // Specifies the rounded corners of overlay plane.
  RRectF rounded_corners;

  // ColorSpace for this overlay.
  gfx::ColorSpace color_space;

  // Optional HDR meta data required to display this overlay.
  absl::optional<HDRMetadata> hdr_metadata;

  // Represents either a background of this overlay or a color of a solid color
  // quad, which can be checked via the |is_solid_color|.
  absl::optional<SkColor4f> color;

  // Set if this is a solid color quad.
  bool is_solid_color;

  // Optional clip rect for this overlay.
  absl::optional<gfx::Rect> clip_rect;
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_PLANE_DATA_H_
