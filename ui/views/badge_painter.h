// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BADGE_PAINTER_H_
#define UI_VIEWS_BADGE_PAINTER_H_

#include <string>

#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class FontList;
class Size;
}  // namespace gfx

namespace views {

class View;

// Painter that paints a badge on the canvas of any other view.
// Provides static methods only;
class VIEWS_EXPORT BadgePainter {
 public:
  // This is a utility class and should not be instantiated.
  BadgePainter() = delete;

  // Draws the badge on `canvas`. `unmirrored_badge_left_x` is the
  // leading edge of the badge, not mirrored for RTL. `text_top_y` is the
  // top of the text the badge should align with, and `primary_font` is the font
  // of that text.
  //
  // You can call this method from any View to draw the badge directly onto the
  // view as part of OnPaint() or a similar method.
  static void PaintBadge(gfx::Canvas* canvas,
                         const View* view,
                         int unmirrored_badge_left_x,
                         int text_top_y,
                         const std::u16string& text,
                         const gfx::FontList& primary_font);

  // Returns the space required for the badge itself, not counting leading
  // or trailing margin. It is recommended to leave a margin of
  // BadgePainter::kBadgeHorizontalMargin between the badge and any other text
  // or image elements.
  static gfx::Size GetBadgeSize(const std::u16string& text,
                                const gfx::FontList& primary_font);

  // Returns the appropriate font to use for the badge based on the font
  // currently being used to render the surrounding text.
  static gfx::FontList GetBadgeFont(const gfx::FontList& context_font);

  // Layout Constants
  //
  // Note that there are a few differences between Views and Mac constants here
  // that are due to the fact that the rendering is different and therefore
  // tweaks to the spacing need to be made to achieve the same visual result.

  // Difference in the font size (in pixels) between menu label font and
  // badge font size.
  static constexpr int kBadgeFontSizeAdjustment = -1;

  // Space between primary text and badge.
  static constexpr int kBadgeHorizontalMargin = 8;

  // Highlight padding around text.
  static constexpr int kBadgeInternalPadding = 4;

  // The minimal badge height on a cocoa menu.
  static constexpr int kBadgeMinHeightCocoa = 16;
};

}  // namespace views

#endif  // UI_VIEWS_BADGE_PAINTER_H_
