// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_HIGHLIGHT_BORDER_H_
#define UI_VIEWS_HIGHLIGHT_BORDER_H_

#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/border.h"
#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {

constexpr int kHighlightBorderThickness = 1;

// A rounded rectangle border that has inner (highlight) and outer color.
// Useful when go/cros-launcher-spec mentions "BorderHighlight".
class VIEWS_EXPORT HighlightBorder : public views::Border {
 public:
  // TODO(crbug/1319944): Change these type names to something more descriptive.
  enum class Type {
    // The highlight border for the UI without a shadow.
    kHighlightBorderNoShadow,
    // The highlight border for the UI with a shadow.
    kHighlightBorderOnShadow,
    // TODO(b/271009119): Clean up these old types after MaterialNext is fully
    // launched.
    // A higher contrast highlight border than the `kHighlightBorder2` used
    // for floating components that do not have a shield below.
    kHighlightBorder1,
    // A less contrast highlight border for components that float above a
    // shield.
    kHighlightBorder2,
    // Has same inner border color as `kHighlightBorder1`. The outer color is
    // same in dark and light mode with low opacity. This type is mainly used
    // with `HighlightBorderLayerOverlay` whose outer border is outside the
    // window contents. For more information, refer to the comment of
    // `HighlightBorderOverlay`.
    kHighlightBorder3,
  };

  // The type of insets created by this highlight border. The insets shrink the
  // content view.
  enum class InsetsType {
    // The border does not add insets that shrink the content, so the content
    // around the edge of the view can be overlapped with the border.
    kNoInsets,
    // Insetting by half of the border size pushes the content inside the outer
    // color of the border so the content bounds is surrounded by outer color
    // of the border.
    kHalfInsets,
    // Insetting by full border size makes sure that contents in the view
    // start inside the inner color of the highlight border, so there is no
    // overlap between content.
    kFullInsets,
  };

  // Paints the highlight border onto `canvas` with given highlight and border
  // border color. Note that directly using this function won't set the insets
  // on any view so it acts like setting kNoInsets when using HighlightBorder
  // class.
  static void PaintBorderToCanvas(gfx::Canvas* canvas,
                                  SkColor highlight_color,
                                  SkColor border_color,
                                  const gfx::Rect& bounds,
                                  const gfx::RoundedCornersF& corner_radii,
                                  Type type);

  // Paints the highlight border onto `canvas` for the specified `view`. The
  // color of the border will be determined using `view`'s color provider. Note
  // that directly using this function won't set the insets on any view so it
  // acts like setting kNoInsets when using HighlightBorder class.
  static void PaintBorderToCanvas(gfx::Canvas* canvas,
                                  const views::View& view,
                                  const gfx::Rect& bounds,
                                  const gfx::RoundedCornersF& corner_radii,
                                  Type type);

  // Returns the inner highlight color used to paint highlight border.
  static SkColor GetHighlightColor(const views::View& view,
                                   HighlightBorder::Type type);

  // Returns the outer border color used to paint highlight border.
  static SkColor GetBorderColor(const views::View& view,
                                HighlightBorder::Type type);

  HighlightBorder(int corner_radius,
                  Type type,
                  InsetsType insets_type = InsetsType::kNoInsets);

  HighlightBorder(const gfx::RoundedCornersF& rounded_corners,
                  Type type,
                  InsetsType insets_type = InsetsType::kNoInsets);

  HighlightBorder(const HighlightBorder&) = delete;
  HighlightBorder& operator=(const HighlightBorder&) = delete;

  ~HighlightBorder() override = default;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  void OnViewThemeChanged(views::View* view) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  // The rounded corners of this border.
  const gfx::RoundedCornersF rounded_corners_;

  const Type type_;

  // Used by `GetInsets()` to calculate the insets.
  const InsetsType insets_type_;
};

}  // namespace views

#endif  // UI_VIEWS_HIGHLIGHT_BORDER_H_
