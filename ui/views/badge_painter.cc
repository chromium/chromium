// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/badge_painter.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Returns the highlight rect for the badge given the font and text rect
// for the badge text.
gfx::Rect GetBadgeRectOutsetAroundText(const gfx::FontList& badge_font,
                                       const gfx::Rect& badge_text_rect) {
  gfx::Rect badge_rect = badge_text_rect;
  badge_rect.Inset(-gfx::AdjustVisualBorderForFont(
      badge_font, gfx::Insets(BadgePainter::kBadgeInternalPadding)));
  return badge_rect;
}

}  // namespace

// static
void BadgePainter::PaintBadge(gfx::Canvas* canvas,
                              const View* view,
                              int unmirrored_badge_left_x,
                              int text_top_y,
                              const std::u16string& text,
                              const gfx::FontList& primary_font) {
  gfx::FontList badge_font = GetBadgeFont(primary_font);

  // Calculate bounding box for badge text.
  unmirrored_badge_left_x += kBadgeInternalPadding;
  text_top_y += gfx::GetFontCapHeightCenterOffset(primary_font, badge_font);
  gfx::Rect badge_text_bounds(gfx::Point(unmirrored_badge_left_x, text_top_y),
                              gfx::GetStringSize(text, badge_font));
  if (base::i18n::IsRTL()) {
    badge_text_bounds.set_x(view->GetMirroredXForRect(badge_text_bounds));
  }

  // Render the badge itself.
  cc::PaintFlags flags;
  const ui::ColorProvider* color_provider = view->GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(ui::kColorBadgeBackground);
  flags.setColor(background_color);
  canvas->DrawRoundRect(
      GetBadgeRectOutsetAroundText(badge_font, badge_text_bounds),
      LayoutProvider::Get()->GetCornerRadiusMetric(
          ShapeContextTokens::kBadgeRadius),
      flags);

  // Render the badge text.
  const SkColor foreground_color =
      color_provider->GetColor(ui::kColorBadgeForeground);
  canvas->DrawStringRect(text, badge_font, foreground_color, badge_text_bounds);
}

// static
gfx::Size BadgePainter::GetBadgeSize(const std::u16string& text,
                                     const gfx::FontList& primary_font) {
  gfx::FontList badge_font = GetBadgeFont(primary_font);
  const gfx::Size text_size = gfx::GetStringSize(text, badge_font);
  return GetBadgeRectOutsetAroundText(badge_font, gfx::Rect(text_size)).size();
}

gfx::FontList BadgePainter::GetBadgeFont(const gfx::FontList& context_font) {
  return views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_BADGE, views::style::STYLE_SECONDARY);
}

}  // namespace views
