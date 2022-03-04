// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_ring_utils.h"

#include <utility>

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace views {

void DrawProgressRing(gfx::Canvas* canvas,
                      const SkRect& bounds,
                      SkColor background_color,
                      SkColor progress_color,
                      float stroke_width,
                      SkScalar start_angle,
                      SkScalar sweep_angle) {
  // Draw the background ring that gets progressively filled.
  SkPath background_path;
  background_path.addArc(bounds, /*startAngle=*/-90,
                         /*SweepAngle=*/360);
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(background_color);
  background_flags.setStrokeWidth(stroke_width);
  canvas->DrawPath(std::move(background_path), std::move(background_flags));

  // Draw the filled portion of the ring.
  SkPath path;
  path.addArc(bounds, start_angle, sweep_angle);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setColor(progress_color);
  flags.setStrokeWidth(stroke_width);
  canvas->DrawPath(std::move(path), std::move(flags));
}

}  // namespace views
