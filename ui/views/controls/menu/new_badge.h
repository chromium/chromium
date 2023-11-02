// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_NEW_BADGE_H_
#define UI_VIEWS_CONTROLS_MENU_NEW_BADGE_H_

#include <string>

#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class FontList;
class Size;
}  // namespace gfx

namespace views {

class View;

// Represents a "New" badge that can be inserted into any other view as part of
// a new feature promotion. Provides static methods only;
class VIEWS_EXPORT NewBadge {
 public:
  // This is a utility class and should not be instantiated.
  NewBadge() = delete;

  // Draws the "new" badge on |canvas|. |unmirrored_badge_left_x| is the
  // leading edge of the badge, not mirrored for RTL. |text_top_y| is the
  // top of the text the badge should align with, and |primary_font| is the font
  // of that text.
  //
  // You can call this method from any View to draw the badge directly onto the
  // view as part of OnPaint() or a similar method, so you don't necessarily
  // have to instantiate a NewBadge view to get this functionality.
  static void DrawNewBadge(gfx::Canvas* canvas,
                           const View* view,
                           int unmirrored_badge_left_x,
                           int text_top_y,
                           const gfx::FontList& primary_font);

  // Returns the space required for the "new" badge itself, not counting leading
  // or trailing margin. It is recommended to leave a margin of
  // NewBadge::kNewBadgeHorizontalMargin between the badge and any other text
  // or image elements.
  static gfx::Size GetNewBadgeSize(const gfx::FontList& primary_font);

  // Gets the accessible description of the new badge, which can be added to
  // tooltip/screen reader text.
  static std::u16string GetNewBadgeAccessibleDescription();

  // Layout Constants
  //
  // Note that there are a few differences between Views and Mac constants here
  // that are due to the fact that the rendering is different and therefore
  // tweaks to the spacing need to be made to achieve the same visual result.

  // Difference in the font size (in pixels) between menu label font and "new"
  // badge font size.
  static constexpr int kNewBadgeFontSizeAdjustment = -1;

  // Space between primary text and "new" badge.
  static constexpr int kNewBadgeHorizontalMargin = 8;

  // Highlight padding around "new" text.
  static constexpr int kNewBadgeInternalPadding = 4;
  static constexpr int kNewBadgeInternalPaddingTopMac = 1;

  // The baseline offset of the "new" badge image to the menu text baseline.
  static constexpr int kNewBadgeBaselineOffsetMac = -4;

  // The corner radius of the rounded rect for the "new" badge.
  static constexpr int kNewBadgeCornerRadius = 3;
  static_assert(kNewBadgeCornerRadius <= kNewBadgeInternalPadding,
                "New badge corner radius should not exceed padding.");
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_NEW_BADGE_H_
