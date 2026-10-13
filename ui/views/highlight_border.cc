// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/highlight_border.h"

#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/decoration/decoration_util.h"
#include "ui/decoration/highlight_border_value.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {

// static
void HighlightBorder::PaintBorderToCanvas(
    gfx::Canvas* canvas,
    SkColor highlight_color,
    SkColor border_color,
    const gfx::Rect& bounds,
    const gfx::RoundedCornersF& corner_radii,
    Type type) {
  ui::decoration::DrawHighlightBorder(
      canvas,
      ui::decoration::HighlightBorderValue(highlight_color, border_color,
                                           kHighlightBorderThickness),
      corner_radii, bounds);
}

// static
void HighlightBorder::PaintBorderToCanvas(
    gfx::Canvas* canvas,
    const views::View& view,
    const gfx::Rect& bounds,
    const gfx::RoundedCornersF& corner_radii,
    Type type) {
  PaintBorderToCanvas(canvas, GetHighlightColor(view, type),
                      GetBorderColor(view, type), bounds, corner_radii, type);
}

// static
SkColor HighlightBorder::GetHighlightColor(const views::View& view,
                                           HighlightBorder::Type type) {
  ui::ColorId highlight_color_id;
  switch (type) {
    case HighlightBorder::Type::kHighlightBorderNoShadow:
    case HighlightBorder::Type::kHighlightBorderOnShadow:
      highlight_color_id = ui::kColorCrosSystemHighlight;
      break;
  }

  // `view` should be embedded in a Widget to use color provider.
  DCHECK(view.GetWidget());
  return view.GetColorProvider()->GetColor(highlight_color_id);
}

// static
SkColor HighlightBorder::GetBorderColor(const views::View& view,
                                        HighlightBorder::Type type) {
  ui::ColorId border_color_id;
  switch (type) {
    case HighlightBorder::Type::kHighlightBorderNoShadow:
      border_color_id = ui::kColorCrosSystemHighlightBorder;
      break;
    case HighlightBorder::Type::kHighlightBorderOnShadow:
      border_color_id = ui::kColorCrosSystemHighlightBorder1;
      break;
  }

  // `view` should be embedded in a Widget to use color provider.
  DCHECK(view.GetWidget());
  return view.GetColorProvider()->GetColor(border_color_id);
}

HighlightBorder::HighlightBorder(int corner_radius,
                                 Type type,
                                 InsetsType insets_type)
    : HighlightBorder(gfx::RoundedCornersF(corner_radius), type, insets_type) {}

HighlightBorder::HighlightBorder(const gfx::RoundedCornersF& rounded_corners,
                                 Type type,
                                 InsetsType insets_type)
    : rounded_corners_(rounded_corners),
      type_(type),
      insets_type_(insets_type) {}

void HighlightBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  PaintBorderToCanvas(canvas, view, view.GetLocalBounds(), rounded_corners_,
                      type_);
}

void HighlightBorder::OnViewThemeChanged(views::View* view) {
  view->SchedulePaint();
}

gfx::Insets HighlightBorder::GetInsets() const {
  switch (insets_type_) {
    case InsetsType::kNoInsets:
      return gfx::Insets();
    case InsetsType::kHalfInsets:
      return gfx::Insets(kHighlightBorderThickness);
    case InsetsType::kFullInsets:
      return gfx::Insets(2 * kHighlightBorderThickness);
  }
}

gfx::Size HighlightBorder::GetMinimumSize() const {
  return gfx::Size(kHighlightBorderThickness * 4,
                   kHighlightBorderThickness * 4);
}

}  // namespace views
