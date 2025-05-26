// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BACKGROUND_H_
#define UI_VIEWS_BACKGROUND_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui {
class ThemedVectorIcon;
}  // namespace ui

namespace views {

class Painter;
class View;

/////////////////////////////////////////////////////////////////////////////
//
// Background class
//
// A background implements a way for views to paint a background. The
// background can be either solid or based on a gradient. Of course,
// Background can be subclassed to implement various effects.
//
// Any View can have a background. See View::SetBackground() and
// View::OnPaintBackground()
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Background {
 public:
  Background();
  Background(const Background&) = delete;
  Background& operator=(const Background&) = delete;
  virtual ~Background();

  // Render the background for the provided view
  virtual void Paint(gfx::Canvas* canvas, View* view) const = 0;

  // This is called by the View on which it is attached. This is overridden for
  // subclasses that depend on theme colors.
  virtual void OnViewThemeChanged(View* view);

  // Returns the rounded corner radii of the background. Returns `std::nullopt`
  // by default.
  virtual std::optional<gfx::RoundedCornersF> GetRoundedCornerRadii() const;

  // Returns the "background color".  After resolution, this color is the color
  // for solid backgrounds; for gradient backgrounds, it's the midpoint of the
  // gradient; for painter backgrounds, this is not useful (returns a default
  // color).
  ui::ColorVariant color() const { return color_; }

  // Set a solid color to be used when drawing backgrounds.
  virtual void SetColor(ui::ColorVariant color);

 private:
  ui::ColorVariant color_;
};

// Creates a background that fills the canvas in the specified color.
VIEWS_EXPORT std::unique_ptr<Background> CreateSolidBackground(
    ui::ColorVariant color);

// Creates a background that fills the canvas with rounded corners.
// If using a rounded rect border as well, pass its radius as `radius` and its
// thickness as `for_border_thickness`.  This will inset the background properly
// so it doesn't bleed through the border.
VIEWS_EXPORT std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    float radius,
    int for_border_thickness = 0);

// Same as above except each corner radius can be different and customized.
VIEWS_EXPORT std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    float top_radius,
    float bottom_radius,
    int for_border_thickness = 0);

// Same as above except each corner radius can be different and customized.
VIEWS_EXPORT std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    const gfx::RoundedCornersF& radii,
    int for_border_thickness = 0);

// Same as above except the background is inset by the specified insets.
VIEWS_EXPORT std::unique_ptr<Background> CreateRoundedRectBackground(
    ui::ColorVariant color,
    const gfx::RoundedCornersF& radii,
    const gfx::Insets& insets);

// Creates a background from the specified Painter.
VIEWS_EXPORT std::unique_ptr<Background> CreateBackgroundFromPainter(
    std::unique_ptr<Painter> painter);

// Creates a background from the specified ThemedVectorIcon.
VIEWS_EXPORT std::unique_ptr<Background> CreateThemedVectorIconBackground(
    const ui::ThemedVectorIcon& icon);

}  // namespace views

#endif  // UI_VIEWS_BACKGROUND_H_
