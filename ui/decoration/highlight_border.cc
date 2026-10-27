// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/highlight_border.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ui::decoration {

DEFINE_SAFE_CAST_TARGET(HighlightBorder)

HighlightBorder::HighlightBorder(SkColor highlight_color,
                                 SkColor border_color,
                                 int thickness)
    : highlight_color_(highlight_color),
      border_color_(border_color),
      thickness_(thickness) {
  DCHECK_GT(thickness_, 0);
}

HighlightBorder::~HighlightBorder() = default;

void HighlightBorder::SetColors(SkColor highlight_color, SkColor border_color) {
  if (highlight_color_ == highlight_color && border_color_ == border_color) {
    return;
  }

  highlight_color_ = highlight_color;
  border_color_ = border_color;
  NotifyDecorationChanged();
}

void HighlightBorder::SetThickness(int thickness) {
  DCHECK_GT(thickness, 0);
  if (thickness_ == thickness) {
    return;
  }

  thickness_ = thickness;
  NotifyDecorationChanged();
}

std::optional<HighlightBorderValue> HighlightBorder::GetSpec(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) const {
  CHECK(!content_bounds.IsEmpty());

  const int smaller_dimension =
      std::min(content_bounds.width(), content_bounds.height());
  const float max_radius = std::floor(smaller_dimension / 2.0f);

  // The ninebox assumption breaks down when the content is too small for the
  // desired thickness: normally each border patch spans two thicknesses plus
  // the corner insets on each side (needing four thicknesses plus the corner
  // insets across opposite patches), while for pill-shaped content the rings
  // sit within the corner insets so opposite patches only need four thicknesses
  // (see HighlightBorderGenerator). Thin the rings to the most the content can
  // hold, and draw nothing once not even a hairline fits.
  const bool is_pill_shaped = (max_radius == rounded_corners.upper_left() ||
                               max_radius == rounded_corners.upper_right() ||
                               max_radius == rounded_corners.lower_right() ||
                               max_radius == rounded_corners.lower_left());
  const gfx::Insets corner_insets = GetInsetsForRoundedCorners(rounded_corners);
  const int max_safe_thickness =
      is_pill_shaped
          ? smaller_dimension / 4
          : std::min(content_bounds.width() - corner_insets.width(),
                     content_bounds.height() - corner_insets.height()) /
                4;
  const int size_adjusted_thickness = std::min(max_safe_thickness, thickness_);
  CHECK_GE(size_adjusted_thickness, 0);

  if (size_adjusted_thickness == 0) {
    return std::nullopt;
  }

  return HighlightBorderValue(highlight_color_, border_color_,
                              size_adjusted_thickness, is_pill_shaped);
}

std::optional<DecorationSource::Details> HighlightBorder::GetDetails(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) {
  const std::optional<HighlightBorderValue> value =
      GetSpec(content_bounds, rounded_corners);
  if (!value.has_value()) {
    return std::nullopt;
  }

  const HighlightBorderDetails& details =
      HighlightBorderDetails::Get(rounded_corners, *value);

  // Both rings sit inside the content, so only the content inside the border
  // patches is guaranteed to be covered.
  gfx::Rect occlusion_rect(content_bounds.size());
  occlusion_rect.Inset(details.aperture_insets);

  return Details{
      .appearance =
          {
              .nine_patch_image = details.nine_patch_image,
              .aperture_insets = details.aperture_insets,
              .margins = details.margins,
          },
      .occlusion_rect = occlusion_rect,
  };
}

}  // namespace ui::decoration
