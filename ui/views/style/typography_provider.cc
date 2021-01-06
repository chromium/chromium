// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography_provider.h"

#include <string>

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

#if defined(OS_APPLE)
#include "base/mac/mac_util.h"
#endif

namespace views {
namespace {

gfx::Font::Weight GetValueBolderThan(gfx::Font::Weight weight) {
  switch (weight) {
    case gfx::Font::Weight::BOLD:
      return gfx::Font::Weight::EXTRA_BOLD;
    case gfx::Font::Weight::EXTRA_BOLD:
    case gfx::Font::Weight::BLACK:
      return gfx::Font::Weight::BLACK;
    default:
      return gfx::Font::Weight::BOLD;
  }
}

ui::NativeTheme::ColorId GetDisabledColorId(int context) {
  switch (context) {
    case style::CONTEXT_BUTTON_MD:
      return ui::NativeTheme::kColorId_ButtonDisabledColor;
    case style::CONTEXT_TEXTFIELD:
      return ui::NativeTheme::kColorId_TextfieldReadOnlyColor;
    case style::CONTEXT_MENU:
    case style::CONTEXT_TOUCH_MENU:
      return ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor;
    default:
      return ui::NativeTheme::kColorId_LabelDisabledColor;
  }
}

ui::NativeTheme::ColorId GetMenuColorId(int style) {
  switch (style) {
    case style::STYLE_SECONDARY:
      return ui::NativeTheme::kColorId_MenuItemMinorTextColor;
    case style::STYLE_SELECTED:
      return ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor;
    case style::STYLE_HIGHLIGHTED:
      return ui::NativeTheme::kColorId_HighlightedMenuItemForegroundColor;
    default:
      return ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor;
  }
}

ui::NativeTheme::ColorId GetHintColorId(int context) {
  return (context == style::CONTEXT_TEXTFIELD)
             ? ui::NativeTheme::kColorId_TextfieldPlaceholderColor
             : ui::NativeTheme::kColorId_LabelSecondaryColor;
}

ui::NativeTheme::ColorId GetColorId(int context, int style) {
  if (style == style::STYLE_DIALOG_BUTTON_DEFAULT)
    return ui::NativeTheme::kColorId_TextOnProminentButtonColor;
  if (style == style::STYLE_DISABLED)
    return GetDisabledColorId(context);
  if (style == style::STYLE_LINK)
    return ui::NativeTheme::kColorId_LinkEnabled;
  if (style == style::STYLE_HINT)
    return GetHintColorId(context);
  if (context == style::CONTEXT_BUTTON_MD)
    return ui::NativeTheme::kColorId_ButtonEnabledColor;
  if (context == style::CONTEXT_LABEL && style == style::STYLE_SECONDARY)
    return ui::NativeTheme::kColorId_LabelSecondaryColor;
  if (context == style::CONTEXT_DIALOG_BODY_TEXT &&
      (style == style::STYLE_PRIMARY || style == style::STYLE_SECONDARY))
    return ui::NativeTheme::kColorId_DialogForeground;
  if (context == style::CONTEXT_TEXTFIELD)
    return ui::NativeTheme::kColorId_TextfieldDefaultColor;
  if (context == style::CONTEXT_MENU || context == style::CONTEXT_TOUCH_MENU)
    return GetMenuColorId(style);
  return ui::NativeTheme::kColorId_LabelEnabledColor;
}

}  // namespace

ui::ResourceBundle::FontDetails TypographyProvider::GetFontDetails(
    int context,
    int style) const {
  ui::ResourceBundle::FontDetails details;

  switch (context) {
    case style::CONTEXT_BUTTON_MD:
      details.size_delta = ui::kLabelFontSizeDelta;
      details.weight = TypographyProvider::MediumWeightForUI();
      break;
    case style::CONTEXT_DIALOG_TITLE:
      details.size_delta = ui::kTitleFontSizeDelta;
      break;
    case style::CONTEXT_TOUCH_MENU:
      details.size_delta = 2;
      break;
    default:
      details.size_delta = ui::kLabelFontSizeDelta;
      break;
  }

  switch (style) {
    case style::STYLE_TAB_ACTIVE:
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_DIALOG_BUTTON_DEFAULT:
      // Only non-MD default buttons should "increase" in boldness.
      if (context == style::CONTEXT_BUTTON) {
        details.weight =
            GetValueBolderThan(ui::ResourceBundle::GetSharedInstance()
                                   .GetFontListForDetails(details)
                                   .GetFontWeight());
      }
      break;
  }

  return details;
}

const gfx::FontList& TypographyProvider::GetFont(int context, int style) const {
  return ui::ResourceBundle::GetSharedInstance().GetFontListForDetails(
      GetFontDetails(context, style));
}

SkColor TypographyProvider::GetColor(const View& view,
                                     int context,
                                     int style) const {
  return view.GetNativeTheme()->GetSystemColor(GetColorId(context, style));
}

int TypographyProvider::GetLineHeight(int context, int style) const {
  return GetFont(context, style).GetHeight();
}

// static
gfx::Font::Weight TypographyProvider::MediumWeightForUI() {
#if defined(OS_APPLE)
  // System fonts are not user-configurable on Mac, so there's a simpler check.
  // However, 10.11 do not ship with a MEDIUM weight system font. In that
  // case, trying to use MEDIUM there will give a bold font, which will look
  // worse with the surrounding NORMAL text than just using NORMAL.
  return base::mac::IsOS10_11() ? gfx::Font::Weight::NORMAL
                                : gfx::Font::Weight::MEDIUM;
#else
  // NORMAL may already have at least MEDIUM weight. Return NORMAL in that case
  // since trying to return MEDIUM would actually make the font lighter-weight
  // than the surrounding text. For example, Windows can be configured to use a
  // BOLD font for dialog text; deriving MEDIUM from that would replace the BOLD
  // attribute with something lighter.
  if (ui::ResourceBundle::GetSharedInstance()
          .GetFontListForDetails(ui::ResourceBundle::FontDetails())
          .GetFontWeight() < gfx::Font::Weight::MEDIUM)
    return gfx::Font::Weight::MEDIUM;
  return gfx::Font::Weight::NORMAL;
#endif
}

}  // namespace views
