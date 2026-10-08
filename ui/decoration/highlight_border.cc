// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/highlight_border.h"

#include <algorithm>
#include <optional>

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

std::optional<DecorationSource::Details> HighlightBorder::GetDetails(
    const gfx::Rect& content_bounds,
    const gfx::RoundedCornersF& rounded_corners) {
  const gfx::Insets corner_insets = GetInsetsForRoundedCorners(rounded_corners);

  // The ninebox assumption breaks down when the content is too small for the
  // desired thickness: each border patch spans two thicknesses plus the corner
  // insets, while the decoration layer is only one thickness larger than the
  // content (see HighlightBorderGenerator). Thin the rings to the most the
  // content can hold, and draw nothing once not even a hairline fits.
  const int max_safe_thickness =
      std::min(content_bounds.width() - corner_insets.width(),
               content_bounds.height() - corner_insets.height()) /
      2;
  const int size_adjusted_thickness = std::min(thickness_, max_safe_thickness);
  if (size_adjusted_thickness <= 0) {
    return std::nullopt;
  }

  const HighlightBorderDetails& details = HighlightBorderDetails::Get(
      rounded_corners, HighlightBorderValue(highlight_color_, border_color_,
                                            size_adjusted_thickness));

  // The inner ring overlaps the edge of the content, so only the content inside
  // it (and inside the rounded corners) is guaranteed to be covered.
  gfx::Rect occlusion_rect(content_bounds.size());
  occlusion_rect.Inset(gfx::Insets(size_adjusted_thickness) + corner_insets);

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
