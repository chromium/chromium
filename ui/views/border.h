// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BORDER_H_
#define UI_VIEWS_BORDER_H_

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Size;
}  // namespace gfx

namespace views {

class Painter;
class View;

////////////////////////////////////////////////////////////////////////////////
//
// Border class.
//
// The border class is used to display a border around a view.
// To set a border on a view, call SetBorder on the view, for example:
// view->SetBorder(
//     CreateSolidBorder(1, view->GetColorProvider()->GetColor(
//                              ui::kColorFocusableBorderUnfocused)));
// Make sure the border color is updated on theme changes.
// Once set on a view, the border is owned by the view.
//
// IMPORTANT NOTE: not all views support borders at this point. In order to
// support the border, views should make sure to use bounds excluding the
// border (by calling View::GetLocalBoundsExcludingBorder) when doing layout and
// painting.
//
////////////////////////////////////////////////////////////////////////////////

class VIEWS_EXPORT Border {
 public:
  Border();
  explicit Border(SkColor color);

  Border(const Border&) = delete;
  Border& operator=(const Border&) = delete;

  virtual ~Border();

  // Renders the border for the specified view.
  virtual void Paint(const View& view, gfx::Canvas* canvas) = 0;

  // Returns the border insets.
  virtual gfx::Insets GetInsets() const = 0;

  // Returns the minimum size this border requires.  Note that this may not be
  // the same as the insets.  For example, a Border may paint images to draw
  // some graphical border around a view, and this would return the minimum size
  // such that these images would not be clipped or overlapping -- but the
  // insets may be larger or smaller, depending on how the view wanted its
  // content laid out relative to these images.
  virtual gfx::Size GetMinimumSize() const = 0;

  // This is called by the View on which it is attached. This is overridden for
  // subclasses that depend on theme colors.
  virtual void OnViewThemeChanged(View* view);

  SkColor color() const { return color_; }

  // Sets the border color.
  void set_color(SkColor color) { color_ = color; }

 private:
  SkColor color_ = gfx::kPlaceholderColor;
};

// Convenience for creating a scoped_ptr with no Border.
VIEWS_EXPORT std::unique_ptr<Border> NullBorder();

// Creates a border that is a simple line of the specified thickness and color.
VIEWS_EXPORT std::unique_ptr<Border> CreateSolidBorder(int thickness,
                                                       SkColor color);

// Creates a border that is a simple line of the specified thickness and color,
// which updates on theme changes.
VIEWS_EXPORT std::unique_ptr<Border> CreateThemedSolidBorder(int thickness,
                                                             ui::ColorId color);

// Creates a border that is a rounded rectangle of the specified thickness and
// color.
// NOTE: `corner_radius` is an OUTER EDGE RADIUS, not a stroke radius!
VIEWS_EXPORT std::unique_ptr<Border>
CreateRoundedRectBorder(int thickness, float corner_radius, SkColor color);
VIEWS_EXPORT std::unique_ptr<Border> CreateRoundedRectBorder(
    int thickness,
    float corner_radius,
    const gfx::Insets& paint_insets,
    SkColor color);

// Same as above except the color updates with theme changes.
VIEWS_EXPORT std::unique_ptr<Border> CreateThemedRoundedRectBorder(
    int thickness,
    float corner_radius,
    ui::ColorId color_id);
VIEWS_EXPORT std::unique_ptr<Border> CreateThemedRoundedRectBorder(
    int thickness,
    float corner_radius,
    const gfx::Insets& paint_insets,
    ui::ColorId color_id);

// Creates a border for reserving space. The returned border does not paint
// anything.
VIEWS_EXPORT std::unique_ptr<Border> CreateEmptyBorder(
    const gfx::Insets& insets);

// A simpler version of the above for a border with uniform thickness.
VIEWS_EXPORT std::unique_ptr<Border> CreateEmptyBorder(int thickness);

// Creates a border of the specified color, and thickness on each side specified
// in |insets|.
VIEWS_EXPORT std::unique_ptr<Border> CreateSolidSidedBorder(
    const gfx::Insets& insets,
    SkColor color);

// Creates a border of the specified color with thickness on each side specified
// in |insets|. The border updates on theme changes.
VIEWS_EXPORT std::unique_ptr<Border> CreateThemedSolidSidedBorder(
    const gfx::Insets& insets,
    ui::ColorId color_id);

// Creates a new border that draws |border| and adds additional padding. This is
// equivalent to changing the insets of |border| without changing how or what it
// paints. Example:
//
// view->SetBorder(CreatePaddedBorder(
//     CreateSolidBorder(1, view->GetColorProvider()->GetColor(
//                              ui::kColorFocusableBorderUnfocused)),
//     gfx::Insets::TLBR(2, 0, 0, 0)));
//
// yields a single dip red border and an additional 2dip of unpainted padding
// above the view content (below the border).
VIEWS_EXPORT std::unique_ptr<Border> CreatePaddedBorder(
    std::unique_ptr<Border> border,
    const gfx::Insets& insets);

// Creates a Border from the specified Painter. |insets| define size of an area
// allocated for a Border.
VIEWS_EXPORT std::unique_ptr<Border> CreateBorderPainter(
    std::unique_ptr<Painter> painter,
    const gfx::Insets& insets);

}  // namespace views

#endif  // UI_VIEWS_BORDER_H_
