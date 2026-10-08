// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/decoration_util.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/export_template.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace ui::decoration {

// static
gfx::Insets ShadowGenerator::GetMargins(const gfx::ShadowValues& shadows) {
  return gfx::ShadowValue::GetMargin(shadows);
}

// static
void ShadowGenerator::Draw(gfx::Canvas* canvas,
                           const gfx::ShadowValues& shadows,
                           const gfx::RoundedCornersF& rounded_corners,
                           const gfx::Rect& content_rect) {
  CHECK(!shadows.empty());

  gfx::ScopedCanvas scoped_canvas(canvas);

  cc::PaintFlags flags;
  flags.setLooper(gfx::CreateShadowDrawLooper(shadows));

  SkRRect r_rect = gfx::RoundedRectToSkRRect(content_rect, rounded_corners);

  // Clip out the center so it's not painted with the shadow.
  canvas->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference, true);
  // Clipping alone is not enough --- due to anti aliasing there will still be
  // some of the fill color in the rounded corners. We must make the fill
  // color transparent.
  flags.setColor(SK_ColorTRANSPARENT);
  canvas->sk_canvas()->drawRRect(r_rect, flags);
}

// static
gfx::Insets ShadowGenerator::GetNineboxApertureInsets(
    const gfx::ShadowValues& shadows,
    const gfx::RoundedCornersF& rounded_corners) {
  DCHECK(!shadows.empty());

  // We need enough space to render the full range of blur and the corner
  // rounding.
  const gfx::Insets blur_region = gfx::ShadowValue::GetBlurRegion(shadows);
  const bool is_pill_shaped = shadows.front().is_pill_shaped();
#if DCHECK_IS_ON()
  // `is_pill_shaped` describes the shape of the content around which the
  // shadows are drawn, so their values must match.
  for (const auto& shadow : shadows) {
    DCHECK_EQ(is_pill_shaped, shadow.is_pill_shaped());
  }
#endif  // DCHECK_IS_ON()
  const gfx::Insets corner_insets = GetInsetsForRoundedCorners(rounded_corners);
  if (!is_pill_shaped) {
    return blur_region + corner_insets;
  }

  // For pill shaped content, instead of allocating space separately for blur
  // and rounded corners, we take advantage of the fact that blur propagates
  // perpendicular to the edge. The inner blur can thus be drawn within the
  // space already occupied by the corner's curvature.
  //
  // This produces a slightly lighter shadow, but is necessary to produce an
  // image for PillShaped shadow that can be represented as non-overlapping
  // patches in NinePatchLayer.
  //
  // TODO(crbug.com/516866009) Ideally, we should use the same image for
  // pilled vs non-pilled content. Investigate why different shadows are
  // generated.
  const gfx::Insets margins = gfx::ShadowValue::GetMargin(shadows);
  const gfx::Insets outer_blur = -margins;
  const gfx::Insets inner_blur = blur_region - outer_blur;
  return gfx::Insets::TLBR(
      outer_blur.top() + std::max(inner_blur.top(), corner_insets.top()),
      outer_blur.left() + std::max(inner_blur.left(), corner_insets.left()),
      outer_blur.bottom() +
          std::max(inner_blur.bottom(), corner_insets.bottom()),
      outer_blur.right() + std::max(inner_blur.right(), corner_insets.right()));
}

gfx::Insets GetInsetsForRoundedCorners(
    const gfx::RoundedCornersF& rounded_corners) {
  return gfx::Insets::TLBR(
      base::ClampRound(std::max(rounded_corners.upper_left(),
                                rounded_corners.upper_right())),
      base::ClampRound(
          std::max(rounded_corners.upper_left(), rounded_corners.lower_left())),
      base::ClampRound(std::max(rounded_corners.lower_left(),
                                rounded_corners.lower_right())),
      base::ClampRound(std::max(rounded_corners.upper_right(),
                                rounded_corners.lower_right())));
}

void DrawHighlightBorder(gfx::Canvas* canvas,
                         const HighlightBorderValue& value,
                         const gfx::RoundedCornersF& rounded_corners,
                         const gfx::Rect& bounds) {
  const int thickness = value.thickness();
  DCHECK_GT(thickness, 0);

  cc::PaintFlags flags;
  flags.setStrokeWidth(thickness);
  flags.setColor(value.border_color());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float half_thickness = thickness / 2.0f;

  // Scale bounds and corner radii with the device scale factor so the border
  // matches the content, but keep the stroke width in physical pixels.
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  const gfx::RectF pixel_bounds = gfx::ConvertRectToPixels(bounds, dsf);
  const gfx::RoundedCornersF scaled_radii =
      gfx::ScaleRoundedCorners(rounded_corners, dsf);

  gfx::RectF outer_border_bounds(pixel_bounds);
  outer_border_bounds.Inset(half_thickness);
  canvas->DrawPath(SkPath::RRect(gfx::RoundedRectFToSkRRect(outer_border_bounds,
                                                            scaled_radii)),
                   flags);

  gfx::RectF inner_border_bounds(pixel_bounds);
  inner_border_bounds.Inset(thickness + half_thickness);
  flags.setColor(value.highlight_color());
  canvas->DrawPath(SkPath::RRect(gfx::RoundedRectFToSkRRect(inner_border_bounds,
                                                            scaled_radii)),
                   flags);
}

// static
gfx::Insets HighlightBorderGenerator::GetMargins(
    const HighlightBorderValue& value) {
  DCHECK_GT(value.thickness(), 0);
  return gfx::Insets(-value.thickness());
}

// static
gfx::Insets HighlightBorderGenerator::GetNineboxApertureInsets(
    const HighlightBorderValue& value,
    const gfx::RoundedCornersF& rounded_corners) {
  DCHECK_GT(value.thickness(), 0);

  // Both rings' corner arcs end within two thicknesses plus the radius.
  return gfx::Insets(2 * value.thickness()) +
         GetInsetsForRoundedCorners(rounded_corners);
}

// static
void HighlightBorderGenerator::Draw(gfx::Canvas* canvas,
                                    const HighlightBorderValue& value,
                                    const gfx::RoundedCornersF& rounded_corners,
                                    const gfx::Rect& content_rect) {
  // The outer ring sits outside the content.
  gfx::Rect bounds = content_rect;
  bounds.Outset(value.thickness());
  DrawHighlightBorder(canvas, value, rounded_corners, bounds);
}

template class EXPORT_TEMPLATE_DEFINE(COMPONENT_EXPORT(UI_DECORATION))
    internal::DecorationCache<gfx::ShadowValues, ShadowGenerator>;

template class EXPORT_TEMPLATE_DEFINE(COMPONENT_EXPORT(UI_DECORATION))
    internal::DecorationCache<HighlightBorderValue, HighlightBorderGenerator>;

}  // namespace ui::decoration
