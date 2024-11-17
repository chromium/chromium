// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography_provider.h"

#include <map>
#include <string>

#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace views {
namespace {

const gfx::FontList& GetFontForDetails(
    const ui::ResourceBundle::FontDetails& details) {
  return ui::ResourceBundle::GetSharedInstance().GetFontListForDetails(details);
}

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

ui::ColorId GetDisabledColorId(int context) {
  switch (context) {
    case style::CONTEXT_BUTTON_MD:
      return ui::kColorButtonForegroundDisabled;
    case style::CONTEXT_TEXTFIELD:
      return ui::kColorTextfieldForegroundDisabled;
    case style::CONTEXT_MENU:
    case style::CONTEXT_TOUCH_MENU:
      return ui::kColorMenuItemForegroundDisabled;
    default:
      return ui::kColorLabelForegroundDisabled;
  }
}

ui::ColorId GetMenuColorId(int style) {
  switch (style) {
    case style::STYLE_SECONDARY:
      return ui::kColorMenuItemForegroundSecondary;
    case style::STYLE_SELECTED:
      return ui::kColorMenuItemForegroundSelected;
    case style::STYLE_HIGHLIGHTED:
      return ui::kColorMenuItemForegroundHighlighted;
    default:
      return ui::kColorMenuItemForeground;
  }
}

ui::ColorId GetHintColorId(int context) {
  return (context == style::CONTEXT_TEXTFIELD)
             ? ui::kColorTextfieldForegroundPlaceholder
             : ui::kColorLabelForegroundSecondary;
}

}  // namespace

// static
const TypographyProvider& TypographyProvider::Get() {
  // The actual instance is owned by the layout provider.
  return LayoutProvider::Get()->GetTypographyProvider();
}

const gfx::FontList& TypographyProvider::GetFont(int context, int style) const {
  TRACE_EVENT0("ui", "TypographyProvider::GetFont");
  return GetFontForDetails(GetFontDetails(context, style));
}

ui::ResourceBundle::FontDetails TypographyProvider::GetFontDetails(
    int context,
    int style) const {
  AssertContextAndStyleAreValid(context, style);
  return GetFontDetailsImpl(context, style);
}

ui::ColorId TypographyProvider::GetColorId(int context, int style) const {
  AssertContextAndStyleAreValid(context, style);
  return GetColorIdImpl(context, style);
}

int TypographyProvider::GetLineHeight(int context, int style) const {
  AssertContextAndStyleAreValid(context, style);
  return GetLineHeightImpl(context, style);
}

// static
gfx::Font::Weight TypographyProvider::MediumWeightForUI() {
#if BUILDFLAG(IS_MAC)
  // System fonts are not user-configurable on Mac, so it's simpler.
  return gfx::Font::Weight::MEDIUM;
#else
  // NORMAL may already have at least MEDIUM weight. Return NORMAL in that case
  // since trying to return MEDIUM would actually make the font lighter-weight
  // than the surrounding text. For example, Windows can be configured to use a
  // BOLD font for dialog text; deriving MEDIUM from that would replace the BOLD
  // attribute with something lighter.
  if (ui::ResourceBundle::GetSharedInstance()
          .GetFontListForDetails(ui::ResourceBundle::FontDetails())
          .GetFontWeight() < gfx::Font::Weight::MEDIUM) {
    return gfx::Font::Weight::MEDIUM;
  }
  return gfx::Font::Weight::NORMAL;
#endif
}

bool TypographyProvider::StyleAllowedForContext(int context, int style) const {
  // TODO(crbug.com/40234831): Limit emphasizing text to contexts where
  // it's obviously correct. chrome_typography_provider.cc implements this
  // correctly, but that does not cover uses outside of //chrome or //ash.
  return true;
}

ui::ResourceBundle::FontDetails TypographyProvider::GetFontDetailsImpl(
    int context,
    int style) const {
  ui::ResourceBundle::FontDetails details;

  switch (context) {
    case style::CONTEXT_BADGE:
      details.size_delta = ui::kBadgeFontSizeDelta;
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::CONTEXT_BUTTON_MD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(13);
      details.weight = MediumWeightForUI();
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
    case style::STYLE_EMPHASIZED:
    case style::STYLE_EMPHASIZED_SECONDARY:
      details.weight = gfx::Font::Weight::SEMIBOLD;
      break;
    case style::STYLE_HEADLINE_1:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(24);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_HEADLINE_2:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(20);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_HEADLINE_3:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(18);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_HEADLINE_4:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(16);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_HEADLINE_4_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(16);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_HEADLINE_5:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(14);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_1:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(16);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_1_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(16);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_1_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(16);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_BODY_2:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(14);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_2_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(14);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_2_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(14);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_BODY_3:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(13);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_3_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(13);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_3_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(13);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_LINK_3:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(13);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_4:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(12);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_4_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(12);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_4_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(12);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_BODY_5:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(11);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_BODY_5_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(11);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_BODY_5_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(11);
      details.weight = gfx::Font::Weight::BOLD;
      break;
    case style::STYLE_LINK_5:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(11);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_CAPTION:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(9);
      details.weight = gfx::Font::Weight::NORMAL;
      break;
    case style::STYLE_CAPTION_MEDIUM:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(9);
      details.weight = gfx::Font::Weight::MEDIUM;
      break;
    case style::STYLE_CAPTION_BOLD:
      details.size_delta = gfx::PlatformFont::GetFontSizeDelta(9);
      details.weight = gfx::Font::Weight::BOLD;
      break;
  }

  return details;
}

ui::ColorId TypographyProvider::GetColorIdImpl(int context, int style) const {
  switch (style) {
    case style::STYLE_DIALOG_BUTTON_DEFAULT:
      return ui::kColorButtonForegroundProminent;
    case style::STYLE_DIALOG_BUTTON_TONAL:
      return ui::kColorButtonForegroundTonal;
    case style::STYLE_DISABLED:
      return GetDisabledColorId(context);
    case style::STYLE_LINK:
    case style::STYLE_LINK_3:
    case style::STYLE_LINK_5:
      return (context == style::CONTEXT_BUBBLE_FOOTER)
                 ? ui::kColorLinkForegroundOnBubbleFooter
                 : ui::kColorLinkForeground;
    case style::STYLE_HINT:
      return GetHintColorId(context);
  }

  switch (context) {
    case style::CONTEXT_BUTTON_MD:
      return ui::kColorButtonForeground;
    case style::CONTEXT_BUBBLE_FOOTER:
    case style::CONTEXT_LABEL:
      if (style == style::STYLE_SECONDARY) {
        return ui::kColorLabelForegroundSecondary;
      }
      break;
    case style::CONTEXT_DIALOG_BODY_TEXT:
      if (style == style::STYLE_PRIMARY || style == style::STYLE_SECONDARY) {
        return ui::kColorDialogForeground;
      }
      break;
    case style::CONTEXT_TEXTFIELD:
      return ui::kColorTextfieldForeground;
    case style::CONTEXT_TEXTFIELD_PLACEHOLDER:
    case style::CONTEXT_TEXTFIELD_SUPPORTING_TEXT:
      return (style == style::STYLE_INVALID)
                 ? ui::kColorTextfieldForegroundPlaceholderInvalid
                 : ui::kColorTextfieldForegroundPlaceholder;
    case style::CONTEXT_MENU:
    case style::CONTEXT_TOUCH_MENU:
      return GetMenuColorId(style);
  }

  return ui::kColorLabelForeground;
}

int TypographyProvider::GetLineHeightImpl(int context, int style) const {
  static constexpr auto kLineHeights = base::MakeFixedFlatMap<int, int>({
      {style::STYLE_HEADLINE_1, 32},      {style::STYLE_HEADLINE_2, 24},
      {style::STYLE_HEADLINE_3, 24},      {style::STYLE_HEADLINE_4, 24},
      {style::STYLE_HEADLINE_4_BOLD, 24}, {style::STYLE_HEADLINE_5, 20},
      {style::STYLE_BODY_1, 24},          {style::STYLE_BODY_1_MEDIUM, 24},
      {style::STYLE_BODY_1_BOLD, 24},     {style::STYLE_BODY_2, 20},
      {style::STYLE_BODY_2_MEDIUM, 20},   {style::STYLE_BODY_2_BOLD, 20},
      {style::STYLE_BODY_3, 20},          {style::STYLE_BODY_3_MEDIUM, 20},
      {style::STYLE_BODY_3_BOLD, 20},     {style::STYLE_BODY_4, 18},
      {style::STYLE_BODY_4_MEDIUM, 18},   {style::STYLE_BODY_4_BOLD, 18},
      {style::STYLE_BODY_5, 16},          {style::STYLE_BODY_5_MEDIUM, 16},
      {style::STYLE_BODY_5_BOLD, 16},     {style::STYLE_LINK_3, 20},
      {style::STYLE_LINK_5, 16},          {style::STYLE_CAPTION, 12},
  });
  const auto it = kLineHeights.find(style);
  return (it == kLineHeights.end())
             ? GetFontForDetails(GetFontDetailsImpl(context, style)).GetHeight()
             : it->second;
}

void TypographyProvider::AssertContextAndStyleAreValid(int context,
                                                       int style) const {
  CHECK_GE(context, style::VIEWS_TEXT_CONTEXT_START);
  CHECK_LT(context, style::TEXT_CONTEXT_MAX);
  CHECK_GE(style, style::VIEWS_TEXT_STYLE_START);
  CHECK(StyleAllowedForContext(context, style))
      << "context: " << context << " style: " << style;
}

}  // namespace views
