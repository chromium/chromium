// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_ring_utils.h"

#include <utility>
#include <vector>

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace views {

namespace {

// Specifies a single section of the ring. Multiple sections should be specified
// in clockwise order and should sum to 360 degrees.
struct ArcSpec {
  enum class Color { kBackground, kForeground };
  Color color;
  SkScalar sweep_angle;
};

void DrawRing(gfx::Canvas* canvas,
              const SkRect& bounds,
              SkColor background_color,
              SkColor foreground_color,
              float stroke_width,
              SkScalar start_angle,
              const std::vector<ArcSpec>& arcs) {
  // Flags for the background portion of the ring.
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(background_color);
  background_flags.setStrokeWidth(stroke_width);

  // Flags for the filled portion of the ring.
  cc::PaintFlags foreground_flags;
  foreground_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  foreground_flags.setAntiAlias(true);
  foreground_flags.setColor(foreground_color);
  foreground_flags.setStrokeWidth(stroke_width);

  SkPath background_path, foreground_path;
  SkScalar cur_angle = start_angle;
  for (const ArcSpec& arc : arcs) {
    SkPath& path = arc.color == ArcSpec::Color::kBackground ? background_path
                                                            : foreground_path;
    path.addArc(bounds, cur_angle, arc.sweep_angle);
    cur_angle += arc.sweep_angle;
  }
  CHECK_EQ(cur_angle, 360 + start_angle);

  canvas->DrawPath(std::move(foreground_path), std::move(foreground_flags));
  canvas->DrawPath(std::move(background_path), std::move(background_flags));
}

}  // namespace

void DrawProgressRing(gfx::Canvas* canvas,
                      const SkRect& bounds,
                      SkColor background_color,
                      SkColor progress_color,
                      float stroke_width,
                      SkScalar start_angle,
                      SkScalar sweep_angle) {
  std::vector<ArcSpec> arcs;
  arcs.push_back({ArcSpec::Color::kForeground, sweep_angle});
  arcs.push_back({ArcSpec::Color::kBackground, 360 - sweep_angle});
  DrawRing(canvas, bounds, background_color, progress_color, stroke_width,
           start_angle, arcs);
}

void DrawSpinningRing(gfx::Canvas* canvas,
                      const SkRect& bounds,
                      SkColor background_color,
                      SkColor progress_color,
                      float stroke_width,
                      SkScalar start_angle) {
  std::vector<ArcSpec> arcs;
  // Draw alternating foreground and background arcs of equal sizes (6 total).
  for (int i = 0; i < 3; ++i) {
    arcs.push_back({ArcSpec::Color::kForeground, 60});
    arcs.push_back({ArcSpec::Color::kBackground, 60});
  }
  DrawRing(canvas, bounds, background_color, progress_color, stroke_width,
           start_angle, arcs);
}

}  // namespace views
