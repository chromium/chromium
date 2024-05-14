// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <Windows.Media.ClosedCaptioning.h>
#include <wrl/client.h>

#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/core_winrt_util.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"

namespace CC = ABI::Windows::Media::ClosedCaptioning;

namespace ui {

namespace {

// Adds !important to all captions styles. They should always override any
// styles added by the video author or by a user stylesheet. This is because on
// Windows, there is an option to turn off captions styles, so any time the
// captions are on, the styles should take priority.
std::string AddCSSImportant(std::string css_string) {
  return css_string + " !important";
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionStyle to a
// CSS FontFamily value.
// These fonts were chosen to satisfy the characteristics represented by values
// of ClosedCaptionStyle Enum in Windows Settings.
void GetFontFamilyString(CC::ClosedCaptionStyle closed_caption_style,
                         std::string* css_font_family,
                         std::string* css_font_variant) {
  *css_font_variant = "normal";
  switch (closed_caption_style) {
    case CC::ClosedCaptionStyle_MonospacedWithSerifs:
      *css_font_family = "Courier New";
      break;
    case CC::ClosedCaptionStyle_ProportionalWithSerifs:
      *css_font_family = "Times New Roman";
      break;
    case CC::ClosedCaptionStyle_MonospacedWithoutSerifs:
      *css_font_family = "Consolas";
      break;
    case CC::ClosedCaptionStyle_ProportionalWithoutSerifs:
      *css_font_family = "Tahoma";
      break;
    case CC::ClosedCaptionStyle_Casual:
      *css_font_family = "Segoe Print";
      break;
    case CC::ClosedCaptionStyle_Cursive:
      *css_font_family = "Segoe Script";
      break;
    case CC::ClosedCaptionStyle_SmallCapitals:
      *css_font_family = "Tahoma";
      *css_font_variant = "small-caps";
      break;
    case CC::ClosedCaptionStyle_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED_IN_MIGRATION();
      *css_font_family = std::string();
      break;
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionEdgeEffect to a
// CSS style value.
std::string GetEdgeEffectString(CC::ClosedCaptionEdgeEffect edge_effect) {
  switch (edge_effect) {
    case CC::ClosedCaptionEdgeEffect_None:
      return "none";
    case CC::ClosedCaptionEdgeEffect_Raised:
      return "-1px 0px 0px silver, 0px -1px 0px silver, 1px 1px 0px black, 2px "
             "2px 0px black, 3px 3px 0px black";
    case CC::ClosedCaptionEdgeEffect_Depressed:
      return "1px 1px 0px silver, 0px 1px 0px silver, -1px -1px 0px black, "
             "-1px "
             "0px 0px black";
    case CC::ClosedCaptionEdgeEffect_Uniform:
      return "0px 0px 4px black, 0px 0px 4px black, 0px 0px 4px black, 0px 0px "
             "4px black";
    case CC::ClosedCaptionEdgeEffect_DropShadow:
      return "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";
    case CC::ClosedCaptionEdgeEffect_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionSize to a CSS
// style value.
std::string GetCaptionSizeString(CC::ClosedCaptionSize caption_size) {
  switch (caption_size) {
    case CC::ClosedCaptionSize_FiftyPercent:
      return "50%";
    case CC::ClosedCaptionSize_OneHundredPercent:
      return "100%";
    case CC::ClosedCaptionSize_OneHundredFiftyPercent:
      return "150%";
    case CC::ClosedCaptionSize_TwoHundredPercent:
      return "200%";
    case CC::ClosedCaptionSize_Default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionOpacity to an
// SkAlpha value.
SkAlpha GetCaptionOpacity(CC::ClosedCaptionOpacity caption_opacity) {
  switch (caption_opacity) {
    case CC::ClosedCaptionOpacity_ZeroPercent:
      return SK_AlphaTRANSPARENT;
    case CC::ClosedCaptionOpacity_TwentyFivePercent:
      return base::ClampRound<SkAlpha>(SK_AlphaOPAQUE * 0.25);
    case CC::ClosedCaptionOpacity_SeventyFivePercent:
      return base::ClampRound<SkAlpha>(SK_AlphaOPAQUE * 0.75);
    case CC::ClosedCaptionOpacity_OneHundredPercent:
    case CC::ClosedCaptionOpacity_Default:
    default:
      return SK_AlphaOPAQUE;
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionColor to an
// SkColor value.
SkColor GetCaptionColor(CC::ClosedCaptionColor caption_color) {
  switch (caption_color) {
    case CC::ClosedCaptionColor_Black:
      return SK_ColorBLACK;
    case CC::ClosedCaptionColor_Red:
      return SK_ColorRED;
    case CC::ClosedCaptionColor_Green:
      return SK_ColorGREEN;
    case CC::ClosedCaptionColor_Blue:
      return SK_ColorBLUE;
    case CC::ClosedCaptionColor_Yellow:
      return SK_ColorYELLOW;
    case CC::ClosedCaptionColor_Magenta:
      return SK_ColorMAGENTA;
    case CC::ClosedCaptionColor_Cyan:
      return SK_ColorCYAN;
    case CC::ClosedCaptionColor_White:
      return SK_ColorWHITE;
    case CC::ClosedCaptionColor_Default:
    default:
      // We shouldn't override with OS Styling for Default case.
      NOTREACHED_IN_MIGRATION();
      return SK_ColorWHITE;
  }
}

// Translates a Windows::Media::ClosedCaptioning::ClosedCaptionColor and a
// Windows::Media::ClosedCaptioning::ClosedCaptionOpacity to an RGBA CSS color
// string.
std::string GetCssColorWithAlpha(CC::ClosedCaptionColor caption_color,
                                 CC::ClosedCaptionOpacity caption_opacity) {
  const SkAlpha opacity = GetCaptionOpacity(caption_opacity);
  const SkColor color = GetCaptionColor(caption_color);
  return color_utils::SkColorToRgbaString(SkColorSetA(color, opacity));
}

std::optional<CaptionStyle> InitializeFromSystemSettings() {
  TRACE_EVENT0("ui", "InitializeFromSystemSettings");
  DCHECK(base::FeatureList::IsEnabled(features::kSystemCaptionStyle));

  base::win::ScopedHString closed_caption_properties_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Media_ClosedCaptioning_ClosedCaptionProperties);
  Microsoft::WRL::ComPtr<CC::IClosedCaptionPropertiesStatics>
      closed_caption_properties_statics;
  HRESULT hr = base::win::RoGetActivationFactory(
      closed_caption_properties_string.get(),
      IID_PPV_ARGS(&closed_caption_properties_statics));
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionSize font_size = CC::ClosedCaptionSize_Default;
  hr = closed_caption_properties_statics->get_FontSize(&font_size);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionEdgeEffect edge_effect = CC::ClosedCaptionEdgeEffect_Default;
  hr = closed_caption_properties_statics->get_FontEffect(&edge_effect);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionStyle font_family = CC::ClosedCaptionStyle_Default;
  hr = closed_caption_properties_statics->get_FontStyle(&font_family);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionColor font_color = CC::ClosedCaptionColor_Default;
  hr = closed_caption_properties_statics->get_FontColor(&font_color);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionOpacity font_opacity = CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_FontOpacity(&font_opacity);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionColor background_color = CC::ClosedCaptionColor_Default;
  hr =
      closed_caption_properties_statics->get_BackgroundColor(&background_color);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionOpacity background_opacity =
      CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_BackgroundOpacity(
      &background_opacity);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionColor region_color = CC::ClosedCaptionColor_Default;
  hr = closed_caption_properties_statics->get_RegionColor(&region_color);
  if (FAILED(hr))
    return std::nullopt;

  CC::ClosedCaptionOpacity region_opacity = CC::ClosedCaptionOpacity_Default;
  hr = closed_caption_properties_statics->get_RegionOpacity(&region_opacity);
  if (FAILED(hr))
    return std::nullopt;

  CaptionStyle caption_style;
  if (font_family != CC::ClosedCaptionStyle_Default) {
    GetFontFamilyString(font_family, &(caption_style.font_family),
                        &(caption_style.font_variant));
    caption_style.font_family = AddCSSImportant(caption_style.font_family);
    caption_style.font_variant = AddCSSImportant(caption_style.font_variant);
  }

  if (font_size != CC::ClosedCaptionSize_Default)
    caption_style.text_size = AddCSSImportant(GetCaptionSizeString(font_size));

  if (edge_effect != CC::ClosedCaptionEdgeEffect_Default) {
    caption_style.text_shadow =
        AddCSSImportant(GetEdgeEffectString(edge_effect));
  }

  if (font_color != CC::ClosedCaptionColor_Default) {
    caption_style.text_color =
        AddCSSImportant(GetCssColorWithAlpha(font_color, font_opacity));
  }

  if (background_color != CC::ClosedCaptionColor_Default) {
    caption_style.background_color = AddCSSImportant(
        GetCssColorWithAlpha(background_color, background_opacity));
  }

  if (region_color != CC::ClosedCaptionColor_Default) {
    caption_style.window_color =
        AddCSSImportant(GetCssColorWithAlpha(region_color, region_opacity));
  }

  return caption_style;
}

}  // namespace

std::optional<CaptionStyle> CaptionStyle::FromSystemSettings() {
  if (base::FeatureList::IsEnabled(features::kSystemCaptionStyle)) {
    return InitializeFromSystemSettings();
  }
  // Return default CaptionStyle if kSystemCaptionStyle is not enabled.
  return std::nullopt;
}

}  // namespace ui
