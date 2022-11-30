// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ROUND_RECT_PAINTER_H_
#define UI_VIEWS_ROUND_RECT_PAINTER_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/views/painter.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Size;
}  // namespace gfx

namespace views {

// Painter to draw a border with rounded corners.
class VIEWS_EXPORT RoundRectPainter : public Painter {
 public:
  static constexpr int kBorderWidth = 1;

  RoundRectPainter(SkColor border_color, int corner_radius);

  RoundRectPainter(const RoundRectPainter&) = delete;
  RoundRectPainter& operator=(const RoundRectPainter&) = delete;

  ~RoundRectPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor border_color_;
  const int corner_radius_;
};

}  // namespace views

#endif  // UI_VIEWS_ROUND_RECT_PAINTER_H_
