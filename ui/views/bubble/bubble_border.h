// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_

#include <optional>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/views_export.h"

class SkRRect;
struct SkRect;

namespace gfx {
class Canvas;
}

namespace ui {
class ColorProvider;
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
    RIGHT = 0x01,
    BOTTOM = 0x02,
    VERTICAL = 0x04,
    CENTER = 0x08,
  };

  enum Arrow {
    TOP_LEFT = 0,
    TOP_RIGHT = RIGHT,
    BOTTOM_LEFT = BOTTOM,
    BOTTOM_RIGHT = BOTTOM | RIGHT,
    LEFT_TOP = VERTICAL,
    RIGHT_TOP = VERTICAL | RIGHT,
    LEFT_BOTTOM = VERTICAL | BOTTOM,
    RIGHT_BOTTOM = VERTICAL | BOTTOM | RIGHT,
    TOP_CENTER = CENTER,
    BOTTOM_CENTER = CENTER | BOTTOM,
    LEFT_CENTER = CENTER | VERTICAL,
    RIGHT_CENTER = CENTER | VERTICAL | RIGHT,
    NONE = 16,   // No arrow. Positioned under the supplied rect.
    FLOAT = 17,  // No arrow. Centered over the supplied rect.
  };

  enum Shadow {
    STANDARD_SHADOW = 0,
#if BUILDFLAG(IS_CHROMEOS)
    // CHROMEOS_SYSTEM_UI_SHADOW uses ChromeOS system UI shadow style.
    CHROMEOS_SYSTEM_UI_SHADOW,
#endif
    // NO_SHADOW don't draw a stroke or a shadow. This is used for platforms
    // that provide their own shadows or UIs that doesn't need shadows.
    NO_SHADOW,
    SHADOW_COUNT,

#if BUILDFLAG(IS_MAC)
    // On Mac, the native window server should provide its own shadow for
    // windows that could overlap the browser window.
    DIALOG_SHADOW = NO_SHADOW,
#elif BUILDFLAG(IS_CHROMEOS)
    DIALOG_SHADOW = CHROMEOS_SYSTEM_UI_SHADOW,
#else
    DIALOG_SHADOW = STANDARD_SHADOW,
#endif
  };

  // The border is stroked at 1px, but for the purposes of reserving space we
  // have to deal in dip coordinates, so round up to 1dip.
  static constexpr int kBorderThicknessDip = 1;

  // Specific to MD bubbles: size of shadow blur (outside the bubble) in DIP.
  static constexpr int kShadowBlur = 6;

  // Space between the anchor view and a visible arrow if one is present.
  static constexpr int kVisibleArrowGap = 4;

  // Length of the visible arrow (distance from the bubble to the tip of the
  // arrow) if one is present.
  static constexpr int kVisibleArrowLength = 8;

  // Radius (half-width) of the visible arrow, when one is present.
  static constexpr int kVisibleArrowRadius = 9;

  // Distance between the edge of the bubble widget and the edge of the visible
  // arrow if one is present.
  static constexpr int kVisibleArrowBuffer = 12;

  BubbleBorder(Arrow arrow,
               Shadow shadow,
               ui::ColorId color_id = ui::kColorDialogBackground);

  BubbleBorder(const BubbleBorder&) = delete;
  BubbleBorder& operator=(const BubbleBorder&) = delete;

  ~BubbleBorder() override;

  static bool has_arrow(Arrow a) { return a < NONE; }

  static bool is_arrow_on_left(Arrow a) {
    return has_arrow(a) && (a == LEFT_CENTER || !(a & (RIGHT | CENTER)));
  }

  static bool is_arrow_on_top(Arrow a) {
    return has_arrow(a) && (a == TOP_CENTER || !(a & (BOTTOM | CENTER)));
  }

  static bool is_arrow_on_horizontal(Arrow a) {
    return a >= NONE ? false : !(int{a} & VERTICAL);
  }

  static bool is_arrow_at_center(Arrow a) {
    return has_arrow(a) && !!(int{a} & CENTER);
  }

  static Arrow horizontal_mirror(Arrow a) {
    return (a == TOP_CENTER || a == BOTTOM_CENTER || a >= NONE)
               ? a
               : static_cast<Arrow>(int{a} ^ RIGHT);
  }

  static Arrow vertical_mirror(Arrow a) {
    return (a == LEFT_CENTER || a == RIGHT_CENTER || a >= NONE)
               ? a
               : static_cast<Arrow>(int{a} ^ BOTTOM);
  }

  // Returns the insets required by a border and shadow based on
  // |shadow_elevation|. This is only used for MD bubbles. A null
  // |shadow_elevation| will yield the default BubbleBorder MD insets.
  static gfx::Insets GetBorderAndShadowInsets(
      const std::optional<int>& shadow_elevation = std::nullopt,
      const std::optional<bool>& draw_border_stroke = std::nullopt,
      Shadow shadow_type = Shadow::STANDARD_SHADOW);

  // Draws a border and shadow outside the |rect| on |canvas|. |color_provider|
  // is passed into GetBorderAndShadowFlags to obtain the shadow color.
  static void DrawBorderAndShadow(SkRect rect,
                                  gfx::Canvas* canvas,
                                  const ui::ColorProvider* color_provider);

  // Set the corner radius, enables Material Design.
  void SetCornerRadius(int radius);
  int corner_radius() const { return corner_radius_; }

  // Set the customized rounded corners. Takes precedence over `corner_radius_`
  // when non-empty.
  void set_rounded_corners(const gfx::RoundedCornersF& rounded_corners) {
    rounded_corners_ = rounded_corners;
  }
  const gfx::RoundedCornersF& rounded_corners() { return rounded_corners_; }

  // Get or set the arrow type.
  void set_arrow(Arrow arrow) { arrow_ = arrow; }
  Arrow arrow() const { return arrow_; }

  void set_visible_arrow(bool visible_arrow) { visible_arrow_ = visible_arrow; }
  bool visible_arrow() const { return visible_arrow_; }

  // Sets whether to draw the border stroke. Passing `nullopt` uses the default
  // behavior (see comments on `ShouldDrawStroke()` below).
  void set_draw_border_stroke(std::optional<bool> draw_border_stroke) {
    draw_border_stroke_ = std::move(draw_border_stroke);
  }

  // Get the shadow type.
  Shadow shadow() const { return shadow_; }

  // Get or set the color for the bubble and arrow body.
  void SetColor(SkColor color);
  SkColor color() const { return color_; }

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

  // Set a flag to avoid the bubble's shadow overlapping the anchor.
  void set_avoid_shadow_overlap(bool value) { avoid_shadow_overlap_ = value; }

  // Sets an explicit insets value to be used.
  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

  // Get the desired widget bounds (in screen coordinates) given the anchor rect
  // and bubble content size; calculated from shadow and arrow image dimensions.
  virtual gfx::Rect GetBounds(const gfx::Rect& anchor_rect,
                              const gfx::Size& contents_size) const;

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;
  void OnViewThemeChanged(View* view) override;

  // Sets and activates the visible `arrow`. The position of the visible arrow
  // on the edge of the `popup_bounds` is determined using the
  // `anchor_rect`. While the side of the arrow is already determined by
  // `arrow`, the placement along the side is chosen to point towards the
  // `anchor_rect`. For a horizontal bubble with an arrow on either the left
  // or right side, the arrow is placed to point towards the vertical center of
  // `anchor_rect`. For a vertical arrow that is either on top of below the
  // bubble, the placement depends on the specifics of `arrow`:
  //
  //  * A right-aligned arrow (TOP_RIGHT, BOTTOM_RIGHT) optimizes the arrow
  //  position to point at the right edge of the `element_bounds`.
  //  * A center-aligned arrow (TOP_CENTER, BOTTOM_CENTER) points towards the
  //  horizontal center of `element_bounds`.
  //  * Otherwise, the arrow points towards the left edge of `element_bounds`.
  //
  // If it is not possible for the arrow to point towards the targeted point
  // because there is no overlap between the bubble and the element in the
  // significant direction, the arrow is placed at the most extreme allowed
  // position that is closest to the targeted point.
  //
  // Note that `popup_bounds` can be slightly shifted to accommodate appended
  // arrow and make the whole popup visually pointing to the anchor element.
  // `popup_min_y` limits this shift, which can be used to prevent overlapping
  // the browser top elements (e.g., the address bar). The `popup_bounds`
  // initial value is expected to not violate the `popup_min_y` restriction.
  //
  // Returns false if the arrow cannot be added due to missing space on the
  // bubble border.
  bool AddArrowToBubbleCornerAndPointTowardsAnchor(const gfx::Rect& anchor_rect,
                                                   gfx::Rect& popup_bounds,
                                                   int popup_min_y);

  // Returns a constant reference to the |visible_arrow_rect_| for teseting
  // purposes.
  const gfx::Rect& GetVisibibleArrowRectForTesting() {
    return visible_arrow_rect_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetSizeForContentsSizeTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, GetBoundsOriginTest);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, ShadowTypes);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest, VisibleArrowSizesAreConsistent);
  FRIEND_TEST_ALL_PREFIXES(BubbleBorderTest,
                           MoveContentsBoundsToPlaceVisibleArrow);

  // Returns the translation vector for a bubble to make space for
  // inserting the visible arrow at the right position for |arrow_|.
  // |include_gap| controls if the displacement accounts for the
  // kVisibleArrowGap.
  static gfx::Vector2d GetContentsBoundsOffsetToPlaceVisibleArrow(
      BubbleBorder::Arrow arrow,
      bool include_gap = true);

  // The border and arrow stroke size used in image assets, in pixels.
  static constexpr int kStroke = 1;

  gfx::Size GetSizeForContentsSize(const gfx::Size& contents_size) const;

  // Calculates and assigns the |visible_arrow_rect_| for the given
  // |contents_bounds| and the |anchor_point| in which the arrow is rendered to.
  void CalculateVisibleArrowRect(const gfx::Rect& contents_bounds,
                                 const gfx::Point& anchor_point) const;

  // Returns the region within |view| representing the client area. This can be
  // set as a canvas clip to ensure any fill or shadow from the border does not
  // draw over the contents of the bubble.
  SkRRect GetClientRect(const View& view) const;

  // Returns whether to draw the border stroke. By default the stroke is drawn
  // iff there is a visible shadow and it does not have a custom elevation.
  bool ShouldDrawStroke() const;

  // Sets `color_` appropriately, using `view` to obtain a ColorProvider.
  // `view` may be null if `requested_color_` is set.
  void UpdateColor(View* view);

  // Paint for the NO_SHADOW shadow type. This just paints transparent pixels
  // to make the window shape based on insets and GetBorderCornerRadius().
  void PaintNoShadow(const View& view, gfx::Canvas* canvas);

  // Paint a visible arrow pointing to the anchor region.
  void PaintVisibleArrow(const View& view, gfx::Canvas* canvas);

  Arrow arrow_;
  int arrow_offset_ = 0;

  // Corner radius for the bubble border. If supplied the border will use
  // material design.
  int corner_radius_ = 0;

  // Customized rounded corners for the bubble border. Takes precedence over
  // `corner_radius_` when non-empty.
  gfx::RoundedCornersF rounded_corners_;

  // Whether a visible arrow should be present.
  bool visible_arrow_ = false;
  // Cached arrow bounding box, calculated when bounds are calculated.
  mutable gfx::Rect visible_arrow_rect_;

  std::optional<bool> draw_border_stroke_;
  Shadow shadow_;
  std::optional<int> md_shadow_elevation_;
  ui::ColorId color_id_;
  std::optional<SkColor> requested_color_;
  SkColor color_ = gfx::kPlaceholderColor;
  bool avoid_shadow_overlap_ = false;
  std::optional<gfx::Insets> insets_;
};

// A Background that clips itself to the specified BubbleBorder and uses the
// color of the BubbleBorder.
class VIEWS_EXPORT BubbleBackground : public Background {
 public:
  explicit BubbleBackground(BubbleBorder* border) : border_(border) {}

  BubbleBackground(const BubbleBackground&) = delete;
  BubbleBackground& operator=(const BubbleBackground&) = delete;

  // Overridden from Background:
  void Paint(gfx::Canvas* canvas, View* view) const override;

 private:
  const raw_ptr<BubbleBorder> border_;
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
