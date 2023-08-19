// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/highlight_border.h"

#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
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
  cc::PaintFlags flags;
  flags.setStrokeWidth(kHighlightBorderThickness);
  flags.setColor(border_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  const float half_thickness = kHighlightBorderThickness / 2.0f;

  // Scale bounds and corner radius with device scale factor to make sure
  // border bounds match content bounds but keep border stroke width the same.
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  const gfx::RectF pixel_bounds = gfx::ConvertRectToPixels(bounds, dsf);

  const SkScalar radii[8] = {
      corner_radii.upper_left() * dsf,  corner_radii.upper_left() * dsf,
      corner_radii.upper_right() * dsf, corner_radii.upper_right() * dsf,
      corner_radii.lower_right() * dsf, corner_radii.lower_right() * dsf,
      corner_radii.lower_left() * dsf,  corner_radii.lower_left() * dsf};

  gfx::RectF outer_border_bounds(pixel_bounds);
  outer_border_bounds.Inset(half_thickness);
  SkPath outer_path;
  outer_path.addRoundRect(gfx::RectFToSkRect(outer_border_bounds), radii);
  canvas->DrawPath(outer_path, flags);

  gfx::RectF inner_border_bounds(pixel_bounds);
  inner_border_bounds.Inset(kHighlightBorderThickness);
  inner_border_bounds.Inset(half_thickness);
  flags.setColor(highlight_color);
  SkPath inner_path;
  inner_path.addRoundRect(gfx::RectFToSkRect(inner_border_bounds), radii);
  canvas->DrawPath(inner_path, flags);
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
    case HighlightBorder::Type::kHighlightBorder1:
      highlight_color_id = ui::kColorHighlightBorderHighlight1;
      break;
    case HighlightBorder::Type::kHighlightBorder2:
      highlight_color_id = ui::kColorHighlightBorderHighlight2;
      break;
    case HighlightBorder::Type::kHighlightBorder3:
      highlight_color_id = ui::kColorHighlightBorderHighlight3;
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
    case HighlightBorder::Type::kHighlightBorder1:
      border_color_id = ui::kColorHighlightBorderBorder1;
      break;
    case HighlightBorder::Type::kHighlightBorder2:
      border_color_id = ui::kColorHighlightBorderBorder2;
      break;
    case HighlightBorder::Type::kHighlightBorder3:
      border_color_id = ui::kColorHighlightBorderBorder3;
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
