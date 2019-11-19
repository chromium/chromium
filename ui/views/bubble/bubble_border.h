// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

class SkRRect;

namespace gfx {
class Rect;
}

namespace views {

// Renders a border, with optional arrow, and a custom dropshadow.
// This can be used to produce floating "bubble" objects with rounded corners.
class VIEWS_EXPORT BubbleBorder : public Border {
 public:
  // Possible locations for the (optional) arrow.
  // 0 bit specifies left or right.
  // 1 bit specifies top or bottom.
  // 2 bit specifies horizontal or vertical.
  // 3 bit specifies whether the arrow at the center of its residing edge.
  enum ArrowMask {
    RIGHT    = 0x01,
    BOTTOM   = 0x02,
    VERTICAL = 0x04,
    CENTER   = 0x08,
  };

  enum Arrow {
    TOP_LEFT      = 0,
    TOP_RIGHT     = RIGHT,
    BOTTOM_LEFT   = BOTTOM,
    BOTTOM_RIGHT  = BOTTOM | RIGHT,
    LEFT_TOP      = VERTICAL,
    RIGHT_TOP     = VERTICAL | RIGHT,
    LEFT_BOTTOM   = VERTICAL | BOTTOM,
    RIGHT_BOTTOM  = VERTICAL | BOTTOM | RIGHT,
    TOP_CENTER    = CENTER,
    BOTTOM_CENTER = CENTER | BOTTOM,
    LEFT_CENTER   = CENTER | VERTICAL,
    RIGHT_CENTER  = CENTER | VERTICAL | RIGHT,
    NONE  = 16,  // No arrow. Positioned under the supplied rect.
    FLOAT = 17,  // No arrow. Centered over the supplied rect.
  };

  enum Shadow {
    NO_SHADOW = 0,
    NO_SHADOW_OPAQUE_BORDER,
    BIG_SHADOW,
    SMALL_SHADOW,
    // NO_ASSETS borders don't draw a stroke or a shadow. This is used for
    // platforms that provide their own shadows.
    NO_ASSETS,
    SHADOW_COUNT,

#if defined(OS_MACOSX)
    // On Mac, the native window server should provide its own shadow for
    // windows that could overlap the browser window.
    DIALOG_SHADOW = NO_ASSETS,
#else
    DIALOG_SHADOW = SMALL_SHADOW,
#endif
  };

  // The border is stroked at 1px, but for the purposes of reserving space we
  // have to deal in dip coordinates, so round up to 1dip.
  static constexpr int kBorderThicknessDip = 1;

  // Specific to MD bubbles: size of shadow blur (outside the bubble) and
  // vertical offset, both in DIP.
  static constexpr int kShadowBlur = 6;
  static constexpr int kShadowVerticalOffset = 2;

  BubbleBorder(Arrow arrow, Shadow shadow, SkColor color);
  ~BubbleBorder() override;

  static bool has_arrow(Arrow a) { return a < NONE; }

  static bool is_arrow_on_left(Arrow a) {
    return has_arrow(a) && (a == LEFT_CENTER || !(a & (RIGHT | CENTER)));
  }

  static bool is_arrow_on_top(Arrow a) {
    return has_arrow(a) && (a == TOP_CENTER || !(a & (BOTTOM | CENTER)));
  }

  static bool is_arrow_on_horizontal(Arrow a) {
    return a >= NONE ? false : !(a & VERTICAL);
  }

  static bool is_arrow_at_center(Arrow a) {
    return has_arrow(a) && !!(a & CENTER);
  }

  static Arrow horizontal_mirror(Arrow a) {
    return (a == TOP_CENTER || a == BOTTOM_CENTER || a >= NONE) ?
        a : static_cast<Arrow>(a ^ RIGHT);
  }

  static Arrow vertical_mirror(Arrow a) {
    return (a == LEFT_CENTER || a == RIGHT_CENTER || a >= NONE) ?
        a : static_cast<Arrow>(a ^ BOTTOM);
  }

  // Returns the insets required by a border and shadow based on
  // |shadow_elevation|. This is only used for MD bubbles. A null
  // |shadow_elevation| will yield the default BubbleBorder MD insets.
  static gfx::Insets GetBorderAndShadowInsets(
      base::Optional<int> shadow_elevation = base::nullopt);

  // Draws a border and shadow based on |shadow_elevation| outside the |rect| on
  // |canvas|, using |draw| as the draw function. Templated so as to accept
  // either SkRect or SkRRect.
  template <typename T>
  static void DrawBorderAndShadow(
      T rect,
      void (cc::PaintCanvas::*draw)(const T&, const cc::PaintFlags&),
      gfx::Canvas* canvas,
      base::Optional<int> shadow_elevation = base::nullopt,
      SkColor shadow_base_color = SK_ColorBLACK) {
    // Borders with custom shadow elevations do not draw the 1px border.
    if (!shadow_elevation.has_value()) {
      // Provide a 1 px border outside the bounds.
      constexpr int kBorderStrokeThicknessPx = 1;
      const SkScalar one_pixel =
          SkFloatToScalar(kBorderStrokeThicknessPx / canvas->image_scale());
      rect.outset(one_pixel, one_pixel);
    }

    (canvas->sk_canvas()->*draw)(
        rect, GetBorderAndShadowFlags(shadow_elevation, shadow_base_color));
  }

  // Set the corner radius, enables Material Design.
  void SetCornerRadius(int radius);

  // Get or set the arrow type.
  void set_arrow(Arrow arrow) { arrow_ = arrow; }
  Arrow arrow() const { return arrow_; }

  // Get the shadow type.
  Shadow shadow() const { return shadow_; }

  // Get or set the background color for the bubble and arrow body.
  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  // If true, the background color should be determined by the host's
  // NativeTheme.
  void set_use_theme_background_color(bool use_theme_background_color) {
    use_theme_background_color_ = use_theme_background_color;
  }
  bool use_theme_background_color() { return use_theme_background_color_; }

  // Sets a desired pixel distance between the arrow tip and the outside edge of
  // the neighboring border image. For example:        |----offset----|
  // '(' represents shadow around the '{' edge:        ((({           ^   })))
  // The arrow will still anchor to the same location but the bubble will shift
  // location to place the arrow |offset| pixels from the perpendicular edge.
  void set_arrow_offset(int offset) { arrow_offset_ = offset; }
  int arrow_offset() const { return arrow_offset_; }

  // Sets the shadow elevation for MD shadows. A null |shadow_elevation| will
  // yield the default BubbleBorder MD shadow.
  void set_md_shadow_elevation(int shadow_elevation) {
    md_shadow_elevation_ = shadow_elevation;
  }

  // Sets the shadow color for MD shadows. Defaults to SK_ColorBLACK.
  void set_md_shadow_color(SkColor shadow_color) {
    md_shadow_color_ = shadow_color;
  }

  // Set a flag to avoid the bubble's shadow overlapping the anchor.
  void set_avoid_shadow_overlap(bool value) { avoid_shadow_overlap_ = value; }

  // Sets an explicit insets value to be used.
  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

  // Get the desired widget bounds (in screen coordinates) given the anchor rect
  // and bubble content size; calculated from shadow and arrow image dimensions.
  virtual gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                              const gfx::Size& contents_size) const;

  // Returns the corner radius of the current image set.
  int corner_radius() const { return corner_radius_; }

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetSizeForContentsSizeTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetBoundsOriginTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, ShadowTypes);

  // Returns the shadows based on |shadow_elevation| to use for painting the
  // border and shadow, and for getting insets. This is only used for MD
  // bubbles. A null |shadow_elevation| will yield the default BubbleBorder MD
  // ShadowValues.
  static const gfx::ShadowValues& GetShadowValues(
      base::Optional<int> shadow_elevation = base::nullopt,
      SkColor shadow_base_color = SK_ColorBLACK);

  // Returns the paint flags to use for painting the border and shadow based on
  // |shadow_elevation|. This is only used for MD bubbles. A null
  // |shadow_elevation| will yield the default BubbleBorder MD PaintFlags.
  static const cc::PaintFlags& GetBorderAndShadowFlags(
      base::Optional<int> shadow_elevation = base::nullopt,
      SkColor shadow_base_color = SK_ColorBLACK);

  // The border and arrow stroke size used in image assets, in pixels.
  static constexpr int kStroke = 1;

  gfx::Size GetSizeForContentsSize(const gfx::Size& contents_size) const;

  // Returns the region within |view| representing the client area. This can be
  // set as a canvas clip to ensure any fill or shadow from the border does not
  // draw over the contents of the bubble.
  SkRRect GetClientRect(const View& view) const;

  // Paint for the NO_ASSETS shadow type. This just paints transparent pixels
  // to make the window shape based on insets and GetBorderCornerRadius().
  void PaintNoAssets(const View& view, gfx::Canvas* canvas);

  // Paint for the NO_SHADOW shadow type. This paints a simple line border.
  void PaintNoShadow(const View& view, gfx::Canvas* canvas);

  Arrow arrow_;
  int arrow_offset_;
  // Corner radius for the bubble border. If supplied the border will use
  // material design.
  int corner_radius_ = 0;

  Shadow shadow_;
  // Elevation for the MD shadow.
  base::Optional<int> md_shadow_elevation_;
  // Color for the MD shadow.
  SkColor md_shadow_color_ = SK_ColorBLACK;
  SkColor background_color_;
  bool use_theme_background_color_;
  bool avoid_shadow_overlap_ = false;
  base::Optional<gfx::Insets> insets_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder);
};

// A Background that clips itself to the specified BubbleBorder and uses
// the background color of the BubbleBorder.
class VIEWS_EXPORT BubbleBackground : public Background {
 public:
  explicit BubbleBackground(BubbleBorder* border) : border_(border) {}

  // Overridden from Background:
  void Paint(gfx::Canvas* canvas, View* view) const override;

 private:
  BubbleBorder* border_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBackground);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
