// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/round_rect_painter.h"

#include "cc/paint/paint_canvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace views {

RoundRectPainter::RoundRectPainter(SkColor border_color, int corner_radius)
    : border_color_(border_color), corner_radius_(corner_radius) {}

RoundRectPainter::~RoundRectPainter() = default;

gfx::Size RoundRectPainter::GetMinimumSize() const {
  return gfx::Size(1, 1);
}

void RoundRectPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  cc::PaintFlags flags;
  flags.setColor(border_color_);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kBorderWidth);
  flags.setAntiAlias(true);
  gfx::Rect rect(size);
  rect.Inset(gfx::Insets::TLBR(0, 0, kBorderWidth, kBorderWidth));
  SkRect skia_rect = gfx::RectToSkRect(rect);
  skia_rect.offset(kBorderWidth / 2.f, kBorderWidth / 2.f);
  canvas->sk_canvas()->drawRoundRect(skia_rect, SkIntToScalar(corner_radius_),
                                     SkIntToScalar(corner_radius_), flags);
}

}  // namespace views
