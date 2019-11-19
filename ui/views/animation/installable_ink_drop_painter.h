// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_

#include "base/optional.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/painter.h"

namespace views {

struct InstallableInkDropConfig;

// Holds the current visual state of the installable ink drop and handles
// painting it. The |Painter::Paint()| implementation draws a rectangular ink
// drop of the given size; the user should set a clip path via
// |gfx::Canvas::ClipPath()| to control the shape.
class VIEWS_EXPORT InstallableInkDropPainter : public Painter {
 public:
  struct VIEWS_EXPORT State {
    State();
    ~State();

    gfx::PointF flood_fill_center;
    float flood_fill_progress = 0.0f;
    float highlighted_ratio = 0.0f;
  };

  // Pointer arguments must outlive |this|.
  InstallableInkDropPainter(const InstallableInkDropConfig* config,
                            const State* state)
      : config_(config), state_(state) {}
  ~InstallableInkDropPainter() override = default;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  // Contains the colors and opacities we use to paint, given the current state.
  // This isn't modified inside this class, but it can be modified by our user.
  const InstallableInkDropConfig* const config_;

  // The current visual state. This isn't modified inside this class, but it can
  // be modified by our user.
  const State* const state_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_PAINTER_H_
