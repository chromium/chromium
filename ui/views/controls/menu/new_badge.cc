// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/new_badge.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Returns the appropriate font to use for the "new" badge based on the font
// currently being used to render the title of the menu item.
gfx::FontList DeriveNewBadgeFont(const gfx::FontList& primary_font) {
  // Preferred font is slightly smaller and slightly more bold than the title
  // font. The size change is required to make it look correct in the badge; we
  // add a small degree of bold to prevent color smearing/blurring due to font
  // smoothing. This ensures readability on all platforms and in both light and
  // dark modes.
  return primary_font.Derive(NewBadge::kNewBadgeFontSizeAdjustment,
                             gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM);
}

// Returns the highlight rect for the "new" badge given the font and text rect
// for the badge text.
gfx::Rect GetNewBadgeRectOutsetAroundText(const gfx::FontList& badge_font,
                                          const gfx::Rect& badge_text_rect) {
  gfx::Rect badge_rect = badge_text_rect;
  badge_rect.Inset(-gfx::AdjustVisualBorderForFont(
      badge_font, gfx::Insets(NewBadge::kNewBadgeInternalPadding)));
  return badge_rect;
}

}  // namespace

// static
void NewBadge::DrawNewBadge(gfx::Canvas* canvas,
                            const View* view,
                            int unmirrored_badge_left_x,
                            int text_top_y,
                            const gfx::FontList& primary_font) {
  gfx::FontList badge_font = DeriveNewBadgeFont(primary_font);
  const std::u16string new_text = l10n_util::GetStringUTF16(IDS_NEW_BADGE);

  // Calculate bounding box for badge text.
  unmirrored_badge_left_x += kNewBadgeInternalPadding;
  text_top_y += gfx::GetFontCapHeightCenterOffset(primary_font, badge_font);
  gfx::Rect badge_text_bounds(gfx::Point(unmirrored_badge_left_x, text_top_y),
                              gfx::GetStringSize(new_text, badge_font));
  if (base::i18n::IsRTL())
    badge_text_bounds.set_x(view->GetMirroredXForRect(badge_text_bounds));

  // Render the badge itself.
  cc::PaintFlags new_flags;
  const ui::ColorProvider* color_provider = view->GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(ui::kColorButtonBackgroundProminent);
  new_flags.setColor(background_color);
  canvas->DrawRoundRect(
      GetNewBadgeRectOutsetAroundText(badge_font, badge_text_bounds),
      kNewBadgeCornerRadius, new_flags);

  // Render the badge text.
  const SkColor foreground_color =
      color_provider->GetColor(ui::kColorButtonForegroundProminent);
  canvas->DrawStringRect(new_text, badge_font, foreground_color,
                         badge_text_bounds);
}

// static
gfx::Size NewBadge::GetNewBadgeSize(const gfx::FontList& primary_font) {
  const std::u16string new_text = l10n_util::GetStringUTF16(IDS_NEW_BADGE);
  gfx::FontList badge_font = DeriveNewBadgeFont(primary_font);
  const gfx::Size text_size = gfx::GetStringSize(new_text, badge_font);
  return GetNewBadgeRectOutsetAroundText(badge_font, gfx::Rect(text_size))
      .size();
}

// static
std::u16string NewBadge::GetNewBadgeAccessibleDescription() {
  return l10n_util::GetStringUTF16(IDS_NEW_BADGE_SCREEN_READER_MESSAGE);
}

}  // namespace views
