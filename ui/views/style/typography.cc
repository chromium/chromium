// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography.h"

#include "base/check_op.h"
#include "ui/color/color_id.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

namespace views::style {
namespace {

void ValidateContextAndStyle(int context, int style) {
  DCHECK_GE(context, VIEWS_TEXT_CONTEXT_START);
  DCHECK_LT(context, TEXT_CONTEXT_MAX);
  DCHECK_GE(style, VIEWS_TEXT_STYLE_START);
}

}  // namespace

ui::ResourceBundle::FontDetails GetFontDetails(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetFontDetails(context,
                                                                       style);
}

const gfx::FontList& GetFont(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetFont(context, style);
}

ui::ColorId GetColorId(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetColorId(context,
                                                                   style);
}

int GetFontSizeDeltaIgnoringUserOrLocaleSettings(int desired_font_size) {
  int size_delta = desired_font_size - gfx::PlatformFont::kDefaultBaseFontSize;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = bundle.GetFontListWithDelta(size_delta);

  // The ResourceBundle's default font may not actually be kDefaultBaseFontSize
  // if, for example, the user has changed their system font sizes or the
  // current locale has been overridden to use a different default font size.
  // Adjust for the difference in default font sizes.
  int user_or_locale_delta = 0;
  if (base_font.GetFontSize() != desired_font_size) {
    user_or_locale_delta = desired_font_size - base_font.GetFontSize();
    base_font = bundle.GetFontListWithDelta(size_delta + user_or_locale_delta);
  }
  DCHECK_EQ(desired_font_size, base_font.GetFontSize());

  // To ensure a subsequent request from the ResourceBundle ignores the delta
  // due to user or locale settings, include it here.
  return base_font.GetFontSize() - gfx::PlatformFont::kDefaultBaseFontSize +
         user_or_locale_delta;
}

int GetLineHeight(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetLineHeight(context,
                                                                      style);
}

}  // namespace views::style
