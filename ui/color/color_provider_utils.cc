// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_utils.h"

#include <memory>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {
namespace {

using RendererColorId = color::mojom::RendererColorId;

// Below defines the mapping between RendererColorIds and ColorIds.
struct RendererColorIdTable {
  RendererColorId renderer_color_id;
  ColorId color_id;
};
constexpr RendererColorIdTable kRendererColorIdMap[] = {
    {RendererColorId::kColorCssSystemActiveText, kColorCssSystemActiveText},
    {RendererColorId::kColorCssSystemBtnFace, kColorCssSystemBtnFace},
    {RendererColorId::kColorCssSystemBtnText, kColorCssSystemBtnText},
    {RendererColorId::kColorCssSystemField, kColorCssSystemField},
    {RendererColorId::kColorCssSystemFieldText, kColorCssSystemFieldText},
    {RendererColorId::kColorCssSystemGrayText, kColorCssSystemGrayText},
    {RendererColorId::kColorCssSystemHighlight, kColorCssSystemHighlight},
    {RendererColorId::kColorCssSystemHighlightText,
     kColorCssSystemHighlightText},
    {RendererColorId::kColorCssSystemHotlight, kColorCssSystemHotlight},
    {RendererColorId::kColorCssSystemLinkText, kColorCssSystemLinkText},
    {RendererColorId::kColorCssSystemMenuHilight, kColorCssSystemMenuHilight},
    {RendererColorId::kColorCssSystemScrollbar, kColorCssSystemScrollbar},
    {RendererColorId::kColorCssSystemVisitedText, kColorCssSystemVisitedText},
    {RendererColorId::kColorCssSystemWindow, kColorCssSystemWindow},
    {RendererColorId::kColorCssSystemWindowText, kColorCssSystemWindowText},
    {RendererColorId::kColorMenuBackground, kColorMenuBackground},
    {RendererColorId::kColorMenuItemBackgroundSelected,
     kColorMenuItemBackgroundSelected},
    {RendererColorId::kColorMenuSeparator, kColorMenuSeparator},
    {RendererColorId::kColorOverlayScrollbarFill, kColorOverlayScrollbarFill},
    {RendererColorId::kColorOverlayScrollbarFillHovered,
     kColorOverlayScrollbarFillHovered},
    {RendererColorId::kColorOverlayScrollbarStroke,
     kColorOverlayScrollbarStroke},
    {RendererColorId::kColorOverlayScrollbarStrokeHovered,
     kColorOverlayScrollbarStrokeHovered},
    {RendererColorId::kColorWebNativeControlAccent,
     kColorWebNativeControlAccent},
    {RendererColorId::kColorWebNativeControlAccentDisabled,
     kColorWebNativeControlAccentDisabled},
    {RendererColorId::kColorWebNativeControlAccentHovered,
     kColorWebNativeControlAccentHovered},
    {RendererColorId::kColorWebNativeControlAccentPressed,
     kColorWebNativeControlAccentPressed},
    {RendererColorId::kColorWebNativeControlAutoCompleteBackground,
     kColorWebNativeControlAutoCompleteBackground},
    {RendererColorId::kColorWebNativeControlCheckboxBackground,
     kColorWebNativeControlCheckboxBackground},
    {RendererColorId::kColorWebNativeControlCheckboxBackgroundDisabled,
     kColorWebNativeControlCheckboxBackgroundDisabled},
    {RendererColorId::kColorWebNativeControlBorder,
     kColorWebNativeControlBorder},
    {RendererColorId::kColorWebNativeControlBorderDisabled,
     kColorWebNativeControlBorderDisabled},
    {RendererColorId::kColorWebNativeControlBorderHovered,
     kColorWebNativeControlBorderHovered},
    {RendererColorId::kColorWebNativeControlBorderPressed,
     kColorWebNativeControlBorderPressed},
    {RendererColorId::kColorWebNativeControlButtonBorder,
     kColorWebNativeControlButtonBorder},
    {RendererColorId::kColorWebNativeControlButtonBorderDisabled,
     kColorWebNativeControlButtonBorderDisabled},
    {RendererColorId::kColorWebNativeControlButtonBorderHovered,
     kColorWebNativeControlButtonBorderHovered},
    {RendererColorId::kColorWebNativeControlButtonBorderPressed,
     kColorWebNativeControlButtonBorderPressed},
    {RendererColorId::kColorWebNativeControlButtonFill,
     kColorWebNativeControlButtonFill},
    {RendererColorId::kColorWebNativeControlButtonFillDisabled,
     kColorWebNativeControlButtonFillDisabled},
    {RendererColorId::kColorWebNativeControlButtonFillHovered,
     kColorWebNativeControlButtonFillHovered},
    {RendererColorId::kColorWebNativeControlButtonFillPressed,
     kColorWebNativeControlButtonFillPressed},
    {RendererColorId::kColorWebNativeControlFill, kColorWebNativeControlFill},
    {RendererColorId::kColorWebNativeControlFillDisabled,
     kColorWebNativeControlFillDisabled},
    {RendererColorId::kColorWebNativeControlFillHovered,
     kColorWebNativeControlFillHovered},
    {RendererColorId::kColorWebNativeControlFillPressed,
     kColorWebNativeControlFillPressed},
    {RendererColorId::kColorWebNativeControlLightenLayer,
     kColorWebNativeControlLightenLayer},
    {RendererColorId::kColorWebNativeControlProgressValue,
     kColorWebNativeControlProgressValue},
    {RendererColorId::kColorWebNativeControlScrollbarArrowBackgroundDisabled,
     kColorWebNativeControlScrollbarArrowBackgroundDisabled},
    {RendererColorId::kColorWebNativeControlScrollbarArrowBackgroundHovered,
     kColorWebNativeControlScrollbarArrowBackgroundHovered},
    {RendererColorId::kColorWebNativeControlScrollbarArrowBackgroundPressed,
     kColorWebNativeControlScrollbarArrowBackgroundPressed},
    {RendererColorId::kColorWebNativeControlScrollbarArrowForeground,
     kColorWebNativeControlScrollbarArrowForeground},
    {RendererColorId::kColorWebNativeControlScrollbarArrowForegroundDisabled,
     kColorWebNativeControlScrollbarArrowForegroundDisabled},
    {RendererColorId::kColorWebNativeControlScrollbarArrowForegroundPressed,
     kColorWebNativeControlScrollbarArrowForegroundPressed},
    {RendererColorId::kColorWebNativeControlScrollbarCorner,
     kColorWebNativeControlScrollbarCorner},
    {RendererColorId::kColorWebNativeControlScrollbarThumb,
     kColorWebNativeControlScrollbarThumb},
    {RendererColorId::kColorWebNativeControlScrollbarThumbHovered,
     kColorWebNativeControlScrollbarThumbHovered},
    {RendererColorId::kColorWebNativeControlScrollbarThumbOverlayMinimalMode,
     kColorWebNativeControlScrollbarThumbOverlayMinimalMode},
    {RendererColorId::kColorWebNativeControlScrollbarThumbPressed,
     kColorWebNativeControlScrollbarThumbPressed},
    {RendererColorId::kColorWebNativeControlScrollbarTrack,
     kColorWebNativeControlScrollbarTrack},
    {RendererColorId::kColorWebNativeControlSlider,
     kColorWebNativeControlSlider},
    {RendererColorId::kColorWebNativeControlSliderBorder,
     kColorWebNativeControlSliderBorder},
    {RendererColorId::kColorWebNativeControlSliderBorderHovered,
     kColorWebNativeControlSliderBorderHovered},
    {RendererColorId::kColorWebNativeControlSliderBorderPressed,
     kColorWebNativeControlSliderBorderPressed},
    {RendererColorId::kColorWebNativeControlSliderDisabled,
     kColorWebNativeControlSliderDisabled},
    {RendererColorId::kColorWebNativeControlSliderHovered,
     kColorWebNativeControlSliderHovered},
    {RendererColorId::kColorWebNativeControlSliderPressed,
     kColorWebNativeControlSliderPressed},
};

ColorProviderUtilsCallbacks* g_color_provider_utils_callbacks = nullptr;

}  // namespace

ColorProviderUtilsCallbacks::~ColorProviderUtilsCallbacks() = default;

#include "ui/color/color_id_map_macros.inc"

std::string ColorIdName(ColorId color_id) {
  static constexpr auto kColorIdMap =
      base::MakeFixedFlatMap<ColorId, const char*>({COLOR_IDS});
  auto it = kColorIdMap.find(color_id);
  if (it != kColorIdMap.cend()) {
    return {it->second};
  }
  std::string_view color_name;
  if (g_color_provider_utils_callbacks &&
      g_color_provider_utils_callbacks->ColorIdName(color_id, &color_name)) {
    return std::string(color_name);
  }
  return base::StringPrintf("ColorId(%d)", color_id);
}

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_map_macros.inc"  // NOLINT(build/include)

std::string SkColorName(SkColor color) {
  static const auto color_name_map =
      base::MakeFixedFlatMap<SkColor, const char*>({
          {gfx::kGoogleBlue050, "kGoogleBlue050"},
          {gfx::kGoogleBlue100, "kGoogleBlue100"},
          {gfx::kGoogleBlue200, "kGoogleBlue200"},
          {gfx::kGoogleBlue300, "kGoogleBlue300"},
          {gfx::kGoogleBlue400, "kGoogleBlue400"},
          {gfx::kGoogleBlue500, "kGoogleBlue500"},
          {gfx::kGoogleBlue600, "kGoogleBlue600"},
          {gfx::kGoogleBlue700, "kGoogleBlue700"},
          {gfx::kGoogleBlue800, "kGoogleBlue800"},
          {gfx::kGoogleBlue900, "kGoogleBlue900"},
          {gfx::kGoogleRed050, "kGoogleRed050"},
          {gfx::kGoogleRed100, "kGoogleRed100"},
          {gfx::kGoogleRed200, "kGoogleRed200"},
          {gfx::kGoogleRed300, "kGoogleRed300"},
          {gfx::kGoogleRed400, "kGoogleRed400"},
          {gfx::kGoogleRed500, "kGoogleRed500"},
          {gfx::kGoogleRed600, "kGoogleRed600"},
          {gfx::kGoogleRed700, "kGoogleRed700"},
          {gfx::kGoogleRed800, "kGoogleRed800"},
          {gfx::kGoogleRed900, "kGoogleRed900"},
          {gfx::kGoogleGreen050, "kGoogleGreen050"},
          {gfx::kGoogleGreen100, "kGoogleGreen100"},
          {gfx::kGoogleGreen200, "kGoogleGreen200"},
          {gfx::kGoogleGreen300, "kGoogleGreen300"},
          {gfx::kGoogleGreen400, "kGoogleGreen400"},
          {gfx::kGoogleGreen500, "kGoogleGreen500"},
          {gfx::kGoogleGreen600, "kGoogleGreen600"},
          {gfx::kGoogleGreen700, "kGoogleGreen700"},
          {gfx::kGoogleGreen800, "kGoogleGreen800"},
          {gfx::kGoogleGreen900, "kGoogleGreen900"},
          {gfx::kGoogleYellow050, "kGoogleYellow050"},
          {gfx::kGoogleYellow100, "kGoogleYellow100"},
          {gfx::kGoogleYellow200, "kGoogleYellow200"},
          {gfx::kGoogleYellow300, "kGoogleYellow300"},
          {gfx::kGoogleYellow400, "kGoogleYellow400"},
          {gfx::kGoogleYellow500, "kGoogleYellow500"},
          {gfx::kGoogleYellow600, "kGoogleYellow600"},
          {gfx::kGoogleYellow700, "kGoogleYellow700"},
          {gfx::kGoogleYellow800, "kGoogleYellow800"},
          {gfx::kGoogleYellow900, "kGoogleYellow900"},
          {gfx::kGoogleGrey050, "kGoogleGrey050"},
          {gfx::kGoogleGrey100, "kGoogleGrey100"},
          {gfx::kGoogleGrey200, "kGoogleGrey200"},
          {gfx::kGoogleGrey300, "kGoogleGrey300"},
          {gfx::kGoogleGrey400, "kGoogleGrey400"},
          {gfx::kGoogleGrey500, "kGoogleGrey500"},
          {gfx::kGoogleGrey600, "kGoogleGrey600"},
          {gfx::kGoogleGrey700, "kGoogleGrey700"},
          {gfx::kGoogleGrey800, "kGoogleGrey800"},
          {gfx::kGoogleGrey900, "kGoogleGrey900"},
          {gfx::kGoogleOrange050, "kGoogleOrange050"},
          {gfx::kGoogleOrange100, "kGoogleOrange100"},
          {gfx::kGoogleOrange200, "kGoogleOrange200"},
          {gfx::kGoogleOrange300, "kGoogleOrange300"},
          {gfx::kGoogleOrange400, "kGoogleOrange400"},
          {gfx::kGoogleOrange500, "kGoogleOrange500"},
          {gfx::kGoogleOrange600, "kGoogleOrange600"},
          {gfx::kGoogleOrange700, "kGoogleOrange700"},
          {gfx::kGoogleOrange800, "kGoogleOrange800"},
          {gfx::kGoogleOrange900, "kGoogleOrange900"},
          {gfx::kGooglePink050, "kGooglePink050"},
          {gfx::kGooglePink100, "kGooglePink100"},
          {gfx::kGooglePink200, "kGooglePink200"},
          {gfx::kGooglePink300, "kGooglePink300"},
          {gfx::kGooglePink400, "kGooglePink400"},
          {gfx::kGooglePink500, "kGooglePink500"},
          {gfx::kGooglePink600, "kGooglePink600"},
          {gfx::kGooglePink700, "kGooglePink700"},
          {gfx::kGooglePink800, "kGooglePink800"},
          {gfx::kGooglePink900, "kGooglePink900"},
          {gfx::kGooglePurple050, "kGooglePurple050"},
          {gfx::kGooglePurple100, "kGooglePurple100"},
          {gfx::kGooglePurple200, "kGooglePurple200"},
          {gfx::kGooglePurple300, "kGooglePurple300"},
          {gfx::kGooglePurple400, "kGooglePurple400"},
          {gfx::kGooglePurple500, "kGooglePurple500"},
          {gfx::kGooglePurple600, "kGooglePurple600"},
          {gfx::kGooglePurple700, "kGooglePurple700"},
          {gfx::kGooglePurple800, "kGooglePurple800"},
          {gfx::kGooglePurple900, "kGooglePurple900"},
          {gfx::kGoogleCyan050, "kGoogleCyan050"},
          {gfx::kGoogleCyan100, "kGoogleCyan100"},
          {gfx::kGoogleCyan200, "kGoogleCyan200"},
          {gfx::kGoogleCyan300, "kGoogleCyan300"},
          {gfx::kGoogleCyan400, "kGoogleCyan400"},
          {gfx::kGoogleCyan500, "kGoogleCyan500"},
          {gfx::kGoogleCyan600, "kGoogleCyan600"},
          {gfx::kGoogleCyan700, "kGoogleCyan700"},
          {gfx::kGoogleCyan800, "kGoogleCyan800"},
          {gfx::kGoogleCyan900, "kGoogleCyan900"},
          {SK_ColorTRANSPARENT, "SK_ColorTRANSPARENT"},
          {SK_ColorBLACK, "SK_ColorBLACK"},
          {SK_ColorDKGRAY, "SK_ColorDKGRAY"},
          {SK_ColorGRAY, "SK_ColorGRAY"},
          {SK_ColorLTGRAY, "SK_ColorLTGRAY"},
          {SK_ColorWHITE, "SK_ColorWHITE"},
          {SK_ColorRED, "kPlaceholderColor"},
          {SK_ColorGREEN, "SK_ColorGREEN"},
          {SK_ColorBLUE, "SK_ColorBLUE"},
          {SK_ColorYELLOW, "SK_ColorYELLOW"},
          {SK_ColorCYAN, "SK_ColorCYAN"},
          {SK_ColorMAGENTA, "SK_ColorMAGENTA"},
      });
  auto color_with_alpha = color;
  SkAlpha color_alpha = SkColorGetA(color_with_alpha);
  color = SkColorSetA(color, color_alpha != 0 ? SK_AlphaOPAQUE : color_alpha);
  auto it = color_name_map.find(color);
  if (it == color_name_map.cend()) {
    return color_utils::SkColorToRgbaString(color);
  }
  if (SkColorGetA(color_with_alpha) == SkColorGetA(color)) {
    return it->second;
  }
  return base::StringPrintf("rgba(%s, %f)", it->second, 1.0 / color_alpha);
}

std::string ConvertColorProviderColorIdToCSSColorId(
    std::string_view color_id_name) {
  std::string css_color_id_name;
  // Based on runtime analysis, the most number of capital letters in
  // `css_color_id_name` is 11. Reserve slightly more space than that.
  css_color_id_name.reserve(color_id_name.size() + 15);

  // Preprocess the first character and skip it in the loop.
  css_color_id_name.push_back('-');
  for (char c : color_id_name.substr(1)) {
    if (base::IsAsciiUpper(c)) {
      css_color_id_name.push_back('-');
      css_color_id_name.push_back(base::ToLowerASCII(c));
    } else {
      css_color_id_name.push_back(c);
    }
  }
  return css_color_id_name;
}

std::string ConvertSkColorToCSSColor(SkColor color) {
  return base::StringPrintf("#%.2x%.2x%.2x%.2x", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color),
                            SkColorGetA(color));
}

RendererColorMap CreateRendererColorMap(const ColorProvider& color_provider) {
  RendererColorMap map;
  for (const auto& table : kRendererColorIdMap) {
    map.emplace(table.renderer_color_id,
                color_provider.GetColor(table.color_id));
  }
  return map;
}

std::unique_ptr<ColorProvider> CreateColorProviderFromRendererColorMap(
    const RendererColorMap& renderer_color_map) {
  std::unique_ptr<ColorProvider> color_provider =
      std::make_unique<ColorProvider>();
  ui::ColorMixer& mixer = color_provider->AddMixer();

  for (const auto& table : kRendererColorIdMap) {
    mixer[table.color_id] = {renderer_color_map.at(table.renderer_color_id)};
  }

  return color_provider;
}

std::unique_ptr<ColorProvider> CreateEmulatedForcedColorsColorProvider(
    bool dark_mode) {
  std::unique_ptr<ColorProvider> color_provider =
      std::make_unique<ColorProvider>();
  ui::ColorMixer& mixer = color_provider->AddMixer();

  // Colors were chosen based on Windows 10 default light and dark high contrast
  // themes.
  mixer[kColorCssSystemBtnFace] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorCssSystemBtnText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
  mixer[kColorCssSystemGrayText] = {dark_mode
                                        ? SkColorSetRGB(0x3F, 0xF2, 0x3F)
                                        : SkColorSetRGB(0x60, 0x00, 0x00)};
  mixer[kColorCssSystemHighlight] = {dark_mode
                                         ? SkColorSetRGB(0x1A, 0xEB, 0xFF)
                                         : SkColorSetRGB(0x37, 0x00, 0x6E)};
  mixer[kColorCssSystemHighlightText] = {dark_mode ? SK_ColorBLACK
                                                   : SK_ColorWHITE};
  mixer[kColorCssSystemHotlight] = {dark_mode
                                        ? SkColorSetRGB(0xFF, 0xFF, 0x00)
                                        : SkColorSetRGB(0x00, 0x00, 0x9F)};
  mixer[kColorCssSystemMenuHilight] = {
      dark_mode ? SkColorSetRGB(0x80, 0x00, 0x80) : SK_ColorBLACK};
  mixer[kColorCssSystemScrollbar] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorCssSystemWindow] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorCssSystemWindowText] = {dark_mode ? SK_ColorWHITE
                                                : SK_ColorBLACK};
  mixer[kColorCssSystemField] = {kColorCssSystemWindow};
  mixer[kColorCssSystemFieldText] = {kColorCssSystemWindowText};
  mixer[kColorCssSystemActiveText] = {kColorCssSystemHotlight};
  mixer[kColorCssSystemLinkText] = {kColorCssSystemHotlight};
  mixer[kColorCssSystemVisitedText] = {kColorCssSystemHotlight};

  // Set the colors for the scrollbar parts based on the emulated definitions
  // above.
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {
      kColorCssSystemBtnText};
  mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
      kColorWebNativeControlScrollbarArrowForeground};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      kColorCssSystemHighlight};
  mixer[kColorWebNativeControlScrollbarCorner] = {kColorCssSystemBtnFace};
  CompleteControlsForcedColorsDefinition(mixer);

  // Set these color ids as some non web native RendererColorIds depend on their
  // values.
  mixer[kColorPrimaryBackground] = {kColorCssSystemWindow};
  mixer[kColorEndpointForeground] =
      GetColorWithMaxContrast(kColorPrimaryBackground);
  mixer[kColorEndpointBackground] =
      GetColorWithMaxContrast(kColorEndpointForeground);
  mixer[kColorMenuSelectionBackground] =
      AlphaBlend(kColorEndpointForeground, kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha200);
  mixer[kColorMidground] = {kColorCssSystemWindowText};
  mixer[kColorSeparator] = {kColorMidground};
  CompleteDefaultNonWebNativeRendererColorIdsDefinition(mixer);

  return color_provider;
}

std::unique_ptr<ColorProvider>
CreateEmulatedForcedColorsColorProviderForTest() {
  std::unique_ptr<ColorProvider> color_provider =
      std::make_unique<ColorProvider>();
  ui::ColorMixer& mixer = color_provider->AddMixer();

  mixer[kColorWebNativeControlAccent] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAccentDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlAccentHovered] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAccentPressed] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAutoCompleteBackground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlBorder] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlBorderDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlBorderHovered] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlBorderPressed] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlButtonBorder] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlButtonBorderDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlButtonBorderHovered] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlButtonBorderPressed] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlButtonFill] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlButtonFillDisabled] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlButtonFillHovered] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlButtonFillPressed] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlCheckboxBackground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlCheckboxBackgroundDisabled] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFill] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillDisabled] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillHovered] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillPressed] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlLightenLayer] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlProgressValue] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundDisabled] = {
      SK_ColorWHITE};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarArrowForegroundDisabled] = {
      SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarCorner] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarThumb] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarThumbHovered] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
      SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarThumbPressed] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarTrack] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlSlider] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlSliderBorder] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlSliderBorderHovered] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlSliderBorderPressed] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlSliderDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlSliderHovered] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlSliderPressed] = {SK_ColorCYAN};

  return color_provider;
}

std::unique_ptr<ColorProvider> COMPONENT_EXPORT(COLOR)
    CreateDefaultColorProviderForBlink(bool dark_mode) {
  std::unique_ptr<ColorProvider> color_provider =
      std::make_unique<ColorProvider>();
  ui::ColorMixer& mixer = color_provider->AddMixer();

  mixer[kColorPrimaryBackground] = {dark_mode ? SkColorSetRGB(0x29, 0x2A, 0x2D)
                                              : SK_ColorWHITE};
  mixer[kColorEndpointForeground] =
      GetColorWithMaxContrast(kColorPrimaryBackground);
  mixer[kColorEndpointBackground] =
      GetColorWithMaxContrast(kColorEndpointForeground);
  mixer[kColorMenuSelectionBackground] =
      AlphaBlend(kColorEndpointForeground, kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha200);
  mixer[kColorMidground] = {dark_mode ? gfx::kGoogleGrey800
                                      : gfx::kGoogleGrey300};
  mixer[kColorSeparator] = {kColorMidground};
  CompleteDefaultWebNativeRendererColorIdsDefinition(mixer, dark_mode, false);
  CompleteDefaultNonWebNativeRendererColorIdsDefinition(mixer);
  CompleteDefaultCssSystemColorDefinition(mixer, dark_mode);

  return color_provider;
}

void CompleteScrollbarColorsDefinition(ui::ColorMixer& mixer) {
  mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
      kColorWebNativeControlScrollbarCorner};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
      kColorWebNativeControlScrollbarArrowBackgroundHovered};
  mixer[kColorWebNativeControlScrollbarThumb] = {
      kColorWebNativeControlScrollbarArrowForeground};
  mixer[kColorWebNativeControlScrollbarThumbHovered] = {
      kColorWebNativeControlScrollbarArrowForegroundPressed};
  mixer[kColorWebNativeControlScrollbarThumbPressed] = {
      kColorWebNativeControlScrollbarThumbHovered};
  mixer[kColorWebNativeControlScrollbarTrack] = {
      kColorWebNativeControlScrollbarCorner};
}

void CompleteControlsForcedColorsDefinition(ui::ColorMixer& mixer) {
  mixer[kColorWebNativeControlAccent] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlAccentDisabled] = {kColorCssSystemGrayText};
  mixer[kColorWebNativeControlAccentHovered] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlAccentPressed] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlAutoCompleteBackground] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlBorder] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlBorderDisabled] = {kColorCssSystemGrayText};
  mixer[kColorWebNativeControlBorderHovered] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlBorderPressed] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlButtonBorder] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlButtonBorderDisabled] = {kColorCssSystemGrayText};
  mixer[kColorWebNativeControlButtonBorderHovered] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlButtonBorderPressed] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlButtonFill] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlButtonFillDisabled] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlButtonFillHovered] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlButtonFillPressed] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlCheckboxBackground] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlCheckboxBackgroundDisabled] = {
      kColorCssSystemWindow};
  mixer[kColorWebNativeControlFill] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlFillDisabled] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlFillHovered] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlFillPressed] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlLightenLayer] = {kColorCssSystemWindow};
  mixer[kColorWebNativeControlProgressValue] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundDisabled] = {
      kColorCssSystemWindow};
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {
      kColorCssSystemBtnText};
  mixer[kColorWebNativeControlScrollbarArrowForegroundDisabled] = {
      kColorCssSystemGrayText};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      kColorCssSystemHighlight};
  mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
      kColorCssSystemBtnText};
  mixer[kColorWebNativeControlScrollbarCorner] = {kColorCssSystemBtnFace};
  mixer[kColorWebNativeControlSlider] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlSliderBorder] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlSliderBorderHovered] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlSliderBorderPressed] = {kColorCssSystemBtnText};
  mixer[kColorWebNativeControlSliderDisabled] = {kColorCssSystemGrayText};
  mixer[kColorWebNativeControlSliderHovered] = {kColorCssSystemHighlight};
  mixer[kColorWebNativeControlSliderPressed] = {kColorCssSystemHighlight};
  CompleteScrollbarColorsDefinition(mixer);
}

void CompleteDefaultCssSystemColorDefinition(ui::ColorMixer& mixer,
                                             bool dark_mode) {
  mixer[kColorCssSystemActiveText] = {SkColorSetRGB(0xFF, 0x00, 0x00)};
  mixer[kColorCssSystemGrayText] = {SkColorSetRGB(0x80, 0x80, 0x80)};
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0x19, 0x67, 0xD2)};
  mixer[kColorCssSystemHighlightText] = {SK_ColorWHITE};
  if (dark_mode) {
    mixer[kColorCssSystemBtnFace] = {SkColorSetRGB(0x6B, 0x6B, 0x6B)};
    mixer[kColorCssSystemBtnText] = {SK_ColorWHITE};
    mixer[kColorCssSystemField] = {SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorCssSystemFieldText] = {SK_ColorWHITE};
    mixer[kColorCssSystemMenuHilight] = {SkColorSetRGB(0x80, 0x00, 0x80)};
    mixer[kColorCssSystemScrollbar] = {SkColorSetRGB(0x12, 0x12, 0x12)};
    mixer[kColorCssSystemWindow] = {SkColorSetRGB(0x12, 0x12, 0x12)};
    mixer[kColorCssSystemWindowText] = {SK_ColorWHITE};
    mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x9E, 0x9E, 0xFF)};
    mixer[kColorCssSystemLinkText] = {SkColorSetRGB(0x9E, 0x9E, 0xFF)};
    mixer[kColorCssSystemVisitedText] = {SkColorSetRGB(0xD0, 0xAD, 0xF0)};
  } else {
    mixer[kColorCssSystemBtnFace] = {SkColorSetRGB(0xEF, 0xEF, 0xEF)};
    mixer[kColorCssSystemBtnText] = {SK_ColorBLACK};
    mixer[kColorCssSystemField] = {SK_ColorWHITE};
    mixer[kColorCssSystemFieldText] = {SK_ColorBLACK};
    mixer[kColorCssSystemMenuHilight] = {SK_ColorBLACK};
    mixer[kColorCssSystemScrollbar] = {SK_ColorWHITE};
    mixer[kColorCssSystemWindow] = {SK_ColorWHITE};
    mixer[kColorCssSystemWindowText] = {SK_ColorBLACK};
    mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x00, 0x00, 0xEE)};
    mixer[kColorCssSystemLinkText] = {SkColorSetRGB(0x00, 0x00, 0xEE)};
    mixer[kColorCssSystemVisitedText] = {SkColorSetRGB(0x55, 0x1A, 0x8B)};
  }
}

void COMPONENT_EXPORT(COLOR)
    CompleteDefaultWebNativeRendererColorIdsDefinition(ui::ColorMixer& mixer,
                                                       bool dark_mode,
                                                       bool high_contrast) {
  if (dark_mode) {
    mixer[kColorWebNativeControlAccent] = {SkColorSetRGB(0x99, 0xC8, 0xFF)};
    mixer[kColorWebNativeControlAccentDisabled] = {
        SkColorSetRGB(0x75, 0x75, 0x75)};
    mixer[kColorWebNativeControlAccentHovered] = {
        SkColorSetRGB(0xD1, 0xE6, 0xFF)};
    mixer[kColorWebNativeControlAccentPressed] = {
        SkColorSetRGB(0x61, 0xA9, 0xFF)};
    mixer[kColorWebNativeControlAutoCompleteBackground] = {
        SkColorSetARGB(0x66, 0x46, 0x5a, 0x7E)};
    mixer[kColorWebNativeControlBorder] = {SkColorSetRGB(0x85, 0x85, 0x85)};
    mixer[kColorWebNativeControlBorderDisabled] = {
        SkColorSetRGB(0x62, 0x62, 0x62)};
    mixer[kColorWebNativeControlBorderHovered] = {
        SkColorSetRGB(0xAC, 0xAC, 0xAC)};
    mixer[kColorWebNativeControlBorderPressed] = {
        SkColorSetRGB(0x6E, 0x6E, 0x6E)};
    mixer[kColorWebNativeControlButtonBorder] = {
        SkColorSetRGB(0x6B, 0x6B, 0x6B)};
    mixer[kColorWebNativeControlButtonBorderDisabled] = {
        SkColorSetRGB(0x36, 0x36, 0x36)};
    mixer[kColorWebNativeControlButtonBorderHovered] = {
        SkColorSetRGB(0x7B, 0x7B, 0x7B)};
    mixer[kColorWebNativeControlButtonBorderPressed] = {
        SkColorSetRGB(0x61, 0x61, 0x61)};
    mixer[kColorWebNativeControlButtonFill] = {SkColorSetRGB(0x6B, 0x6B, 0x6B)};
    mixer[kColorWebNativeControlButtonFillDisabled] = {
        SkColorSetRGB(0x36, 0x36, 0x36)};
    mixer[kColorWebNativeControlButtonFillHovered] = {
        SkColorSetRGB(0x7B, 0x7B, 0x7B)};
    mixer[kColorWebNativeControlButtonFillPressed] = {
        SkColorSetRGB(0x61, 0x61, 0x61)};
    mixer[kColorWebNativeControlCheckboxBackground] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlCheckboxBackgroundDisabled] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlFill] = {SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlFillDisabled] = {
        SkColorSetRGB(0x36, 0x36, 0x36)};
    mixer[kColorWebNativeControlFillHovered] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlFillPressed] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlLightenLayer] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlProgressValue] = {
        SkColorSetRGB(0x63, 0xAD, 0xE5)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundDisabled] = {
        SkColorSetRGB(0x36, 0x36, 0x36)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
        SkColorSetRGB(0x4F, 0x4F, 0x4F)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
        SkColorSetRGB(0xB1, 0xB1, 0xB1)};
    mixer[kColorWebNativeControlScrollbarArrowForeground] = {SK_ColorWHITE};
    mixer[kColorWebNativeControlScrollbarArrowForegroundDisabled] = {
        SkColorSetRGB(0xAF, 0xAF, 0xAF)};
    mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
        SK_ColorBLACK};
    mixer[kColorWebNativeControlScrollbarCorner] = {
        SkColorSetRGB(0x12, 0x12, 0x12)};
    mixer[kColorWebNativeControlScrollbarThumb] = {
        SkColorSetA(SK_ColorWHITE, 0x33)};
    mixer[kColorWebNativeControlScrollbarThumbHovered] = {
        SkColorSetA(SK_ColorWHITE, 0x4D)};
    mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
        SkColorSetA(SK_ColorWHITE, 0x8B)};
    mixer[kColorWebNativeControlScrollbarThumbPressed] = {
        SkColorSetA(SK_ColorWHITE, 0x80)};
    mixer[kColorWebNativeControlScrollbarTrack] = {
        SkColorSetRGB(0x42, 0x42, 0x42)};
    mixer[kColorWebNativeControlSlider] = {SkColorSetRGB(0x99, 0xC8, 0xFF)};
    mixer[kColorWebNativeControlSliderBorder] = {kColorWebNativeControlBorder};
    mixer[kColorWebNativeControlSliderBorderHovered] = {
        kColorWebNativeControlBorderHovered};
    mixer[kColorWebNativeControlSliderBorderPressed] = {
        kColorWebNativeControlBorderPressed};
    mixer[kColorWebNativeControlSliderDisabled] = {
        SkColorSetRGB(0x75, 0x75, 0x75)};
    mixer[kColorWebNativeControlSliderHovered] = {
        SkColorSetRGB(0xD1, 0xE6, 0xFF)};
    mixer[kColorWebNativeControlSliderPressed] = {
        SkColorSetRGB(0x61, 0xA9, 0xFF)};
  } else {
    mixer[kColorWebNativeControlAccent] = {SkColorSetRGB(0x00, 0x75, 0xFF)};
    mixer[kColorWebNativeControlAccentDisabled] = {
        SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
    mixer[kColorWebNativeControlAccentHovered] = {
        SkColorSetRGB(0x00, 0x5C, 0xC8)};
    mixer[kColorWebNativeControlAccentPressed] = {
        SkColorSetRGB(0x37, 0x93, 0xFF)};
    mixer[kColorWebNativeControlAutoCompleteBackground] = {
        SkColorSetRGB(0xE8, 0xF0, 0xFE)};
    mixer[kColorWebNativeControlBorder] = {SkColorSetRGB(0x76, 0x76, 0x76)};
    mixer[kColorWebNativeControlBorderDisabled] = {
        SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
    mixer[kColorWebNativeControlBorderHovered] = {
        SkColorSetRGB(0x4F, 0x4F, 0x4F)};
    mixer[kColorWebNativeControlBorderPressed] = {
        SkColorSetRGB(0x8D, 0x8D, 0x8D)};
    mixer[kColorWebNativeControlButtonBorder] = {
        SkColorSetRGB(0x76, 0x76, 0x76)};
    mixer[kColorWebNativeControlButtonBorderDisabled] = {
        SkColorSetARGB(0x4D, 0x76, 0x76, 0x76)};
    mixer[kColorWebNativeControlButtonBorderHovered] = {
        SkColorSetRGB(0x4F, 0x4F, 0x4F)};
    mixer[kColorWebNativeControlButtonBorderPressed] = {
        SkColorSetRGB(0x8D, 0x8D, 0x8D)};
    mixer[kColorWebNativeControlButtonFill] = {SkColorSetRGB(0xEF, 0xEF, 0xEF)};
    mixer[kColorWebNativeControlButtonFillDisabled] = {
        SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF)};
    mixer[kColorWebNativeControlButtonFillHovered] = {
        SkColorSetRGB(0xE5, 0xE5, 0xE5)};
    mixer[kColorWebNativeControlButtonFillPressed] = {
        SkColorSetRGB(0xF5, 0xF5, 0xF5)};
    mixer[kColorWebNativeControlCheckboxBackground] = {SK_ColorWHITE};
    mixer[kColorWebNativeControlCheckboxBackgroundDisabled] = {
        SkColorSetA(SK_ColorWHITE, 0x99)};
    mixer[kColorWebNativeControlFill] = {SkColorSetRGB(0xEF, 0xEF, 0xEF)};
    mixer[kColorWebNativeControlFillDisabled] = {
        SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF)};
    mixer[kColorWebNativeControlFillHovered] = {
        SkColorSetRGB(0xE5, 0xE5, 0xE5)};
    mixer[kColorWebNativeControlFillPressed] = {
        SkColorSetRGB(0xF5, 0xF5, 0xF5)};
    mixer[kColorWebNativeControlLightenLayer] = {
        SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9)};
    mixer[kColorWebNativeControlProgressValue] = {
        SkColorSetRGB(0x00, 0x75, 0xFF)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundDisabled] = {
        SkColorSetRGB(0xF1, 0xF1, 0xF1)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
        SkColorSetRGB(0xD2, 0xD2, 0xD2)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
        SkColorSetRGB(0x78, 0x78, 0x78)};
    mixer[kColorWebNativeControlScrollbarArrowForeground] = {
        SkColorSetRGB(0x50, 0x50, 0x50)};
    mixer[kColorWebNativeControlScrollbarArrowForegroundDisabled] = {
        SkColorSetARGB(0x4D, 0x50, 0x50, 0x50)};
    mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
        SK_ColorWHITE};
    mixer[kColorWebNativeControlScrollbarCorner] = {
        SkColorSetRGB(0xDC, 0xDC, 0xDC)};
    mixer[kColorWebNativeControlScrollbarThumb] = {
        SkColorSetA(SK_ColorBLACK, 0x33)};
    mixer[kColorWebNativeControlScrollbarThumbHovered] = {
        SkColorSetA(SK_ColorBLACK, 0x4D)};
    mixer[kColorWebNativeControlScrollbarThumbOverlayMinimalMode] = {
        SkColorSetA(SK_ColorBLACK, 0x72)};
    mixer[kColorWebNativeControlScrollbarThumbPressed] = {
        SkColorSetA(SK_ColorBLACK, 0x80)};
    mixer[kColorWebNativeControlScrollbarTrack] = {
        SkColorSetRGB(0xF1, 0xF1, 0xF1)};
    mixer[kColorWebNativeControlSlider] = {SkColorSetRGB(0x00, 0x75, 0xFF)};
    mixer[kColorWebNativeControlSliderBorder] =
        high_contrast ? kColorWebNativeControlBorder
                      : SetAlpha(kColorWebNativeControlBorder, 0x80);
    mixer[kColorWebNativeControlSliderBorderHovered] =
        high_contrast ? kColorWebNativeControlBorderHovered
                      : SetAlpha(kColorWebNativeControlBorderHovered, 0x80);
    mixer[kColorWebNativeControlSliderBorderPressed] =
        high_contrast ? kColorWebNativeControlBorderPressed
                      : SetAlpha(kColorWebNativeControlBorderPressed, 0x80);
    mixer[kColorWebNativeControlSliderDisabled] = {
        SkColorSetRGB(0xCB, 0xCB, 0xCB)};
    mixer[kColorWebNativeControlSliderHovered] = {
        SkColorSetRGB(0x00, 0x5C, 0xC8)};
    mixer[kColorWebNativeControlSliderPressed] = {
        SkColorSetRGB(0x37, 0x93, 0xFF)};
  }
}

void COMPONENT_EXPORT(COLOR)
    CompleteDefaultNonWebNativeRendererColorIdsDefinition(
        ui::ColorMixer& mixer) {
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuItemBackgroundSelected] = {kColorMenuSelectionBackground};
  mixer[kColorMenuSeparator] = {kColorSeparator};
  mixer[kColorOverlayScrollbarFill] =
      SetAlpha(kColorEndpointForeground, gfx::kGoogleGreyAlpha700);
  mixer[kColorOverlayScrollbarFillHovered] =
      SetAlpha(kColorEndpointForeground, gfx::kGoogleGreyAlpha800);
  mixer[kColorOverlayScrollbarStroke] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha400);
  mixer[kColorOverlayScrollbarStrokeHovered] =
      SetAlpha(kColorEndpointBackground, gfx::kGoogleGreyAlpha500);
}

RendererColorMap COMPONENT_EXPORT(COLOR)
    GetDefaultBlinkColorProviderColorMaps(bool dark_mode,
                                          bool is_forced_colors) {
  return CreateRendererColorMap(
      is_forced_colors ? *CreateEmulatedForcedColorsColorProvider(dark_mode)
                       : *CreateDefaultColorProviderForBlink(dark_mode));
}

bool IsRendererColorMappingEquivalent(
    const ColorProvider* color_provider,
    const RendererColorMap& renderer_color_map) {
  if (!color_provider) {
    return false;
  }
  CHECK(!renderer_color_map.empty());

  for (const auto& table : kRendererColorIdMap) {
    // The `renderer_color_map_` should map the full set of renderer color ids.
    DCHECK(base::Contains(renderer_color_map, table.renderer_color_id));
    if (color_provider->GetColor(table.color_id) !=
        renderer_color_map.at(table.renderer_color_id)) {
      return false;
    }
  }
  return true;
}

void SetColorProviderUtilsCallbacks(ColorProviderUtilsCallbacks* callbacks) {
  g_color_provider_utils_callbacks = callbacks;
}

}  // namespace ui
