// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_utils.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkColor.h"
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
    {RendererColorId::kColorMenuBackground, kColorMenuBackground},
    {RendererColorId::kColorMenuItemBackgroundSelected,
     kColorMenuItemBackgroundSelected},
    {RendererColorId::kColorMenuSeparator, kColorMenuSeparator},
    {RendererColorId::kColorOverlayScrollbarFill, kColorOverlayScrollbarFill},
    {RendererColorId::kColorOverlayScrollbarFillDark,
     kColorOverlayScrollbarFillDark},
    {RendererColorId::kColorOverlayScrollbarFillLight,
     kColorOverlayScrollbarFillLight},
    {RendererColorId::kColorOverlayScrollbarFillHovered,
     kColorOverlayScrollbarFillHovered},
    {RendererColorId::kColorOverlayScrollbarFillHoveredDark,
     kColorOverlayScrollbarFillHoveredDark},
    {RendererColorId::kColorOverlayScrollbarFillHoveredLight,
     kColorOverlayScrollbarFillHoveredLight},
    {RendererColorId::kColorOverlayScrollbarStroke,
     kColorOverlayScrollbarStroke},
    {RendererColorId::kColorOverlayScrollbarStrokeDark,
     kColorOverlayScrollbarStrokeDark},
    {RendererColorId::kColorOverlayScrollbarStrokeLight,
     kColorOverlayScrollbarStrokeLight},
    {RendererColorId::kColorOverlayScrollbarStrokeHovered,
     kColorOverlayScrollbarStrokeHovered},
    {RendererColorId::kColorOverlayScrollbarStrokeHoveredDark,
     kColorOverlayScrollbarStrokeHoveredDark},
    {RendererColorId::kColorOverlayScrollbarStrokeHoveredLight,
     kColorOverlayScrollbarStrokeHoveredLight},
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
    {RendererColorId::kColorWebNativeControlBackground,
     kColorWebNativeControlBackground},
    {RendererColorId::kColorWebNativeControlBackgroundDisabled,
     kColorWebNativeControlBackgroundDisabled},
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
    {RendererColorId::kColorWebNativeControlScrollbarArrowBackgroundHovered,
     kColorWebNativeControlScrollbarArrowBackgroundHovered},
    {RendererColorId::kColorWebNativeControlScrollbarArrowBackgroundPressed,
     kColorWebNativeControlScrollbarArrowBackgroundPressed},
    {RendererColorId::kColorWebNativeControlScrollbarArrowForeground,
     kColorWebNativeControlScrollbarArrowForeground},
    {RendererColorId::kColorWebNativeControlScrollbarArrowForegroundPressed,
     kColorWebNativeControlScrollbarArrowForegroundPressed},
    {RendererColorId::kColorWebNativeControlScrollbarCorner,
     kColorWebNativeControlScrollbarCorner},
    {RendererColorId::kColorWebNativeControlScrollbarThumb,
     kColorWebNativeControlScrollbarThumb},
    {RendererColorId::kColorWebNativeControlScrollbarThumbHovered,
     kColorWebNativeControlScrollbarThumbHovered},
    {RendererColorId::kColorWebNativeControlScrollbarThumbInactive,
     kColorWebNativeControlScrollbarThumbInactive},
    {RendererColorId::kColorWebNativeControlScrollbarThumbPressed,
     kColorWebNativeControlScrollbarThumbPressed},
    {RendererColorId::kColorWebNativeControlScrollbarTrack,
     kColorWebNativeControlScrollbarTrack},
    {RendererColorId::kColorWebNativeControlSlider,
     kColorWebNativeControlSlider},
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

base::StringPiece ColorModeName(ColorProviderKey::ColorMode color_mode) {
  switch (color_mode) {
    case ColorProviderKey::ColorMode::kLight:
      return "kLight";
    case ColorProviderKey::ColorMode::kDark:
      return "kDark";
    default:
      return "<invalid>";
  }
}

base::StringPiece ContrastModeName(
    ColorProviderKey::ContrastMode contrast_mode) {
  switch (contrast_mode) {
    case ColorProviderKey::ContrastMode::kNormal:
      return "kNormal";
    case ColorProviderKey::ContrastMode::kHigh:
      return "kHigh";
    default:
      return "<invalid>";
  }
}

base::StringPiece ForcedColorsName(
    ColorProviderKey::ForcedColors forced_colors) {
  switch (forced_colors) {
    case ColorProviderKey::ForcedColors::kNone:
      return "kNone";
    case ColorProviderKey::ForcedColors::kEmulated:
      return "kEmulated";
    case ColorProviderKey::ForcedColors::kActive:
      return "kActive";
    case ColorProviderKey::ForcedColors::kDusk:
      return "kDusk";
    case ColorProviderKey::ForcedColors::kDesert:
      return "kDesert";
    case ColorProviderKey::ForcedColors::kBlack:
      return "kBlack";
    case ColorProviderKey::ForcedColors::kWhite:
      return "kWhite";
    default:
      return "<invalid>";
  }
}

base::StringPiece SystemThemeName(ui::SystemTheme system_theme) {
  switch (system_theme) {
    case ui::SystemTheme::kDefault:
      return "kDefault";
#if BUILDFLAG(IS_LINUX)
    case ui::SystemTheme::kGtk:
      return "kGtk";
    case ui::SystemTheme::kQt:
      return "kQt";
#endif
    default:
      return "<invalid>";
  }
}

#include "ui/color/color_id_map_macros.inc"

std::string ColorIdName(ColorId color_id) {
  static constexpr const auto color_id_map =
      base::MakeFixedFlatMap<ColorId, const char*>({COLOR_IDS});
  auto* i = color_id_map.find(color_id);
  if (i != color_id_map.cend())
    return {i->second};
  base::StringPiece color_name;
  if (g_color_provider_utils_callbacks &&
      g_color_provider_utils_callbacks->ColorIdName(color_id, &color_name))
    return std::string(color_name.data(), color_name.length());
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
  auto* i = color_name_map.find(color);
  if (i != color_name_map.cend()) {
    if (SkColorGetA(color_with_alpha) == SkColorGetA(color))
      return i->second;
    return base::StringPrintf("rgba(%s, %f)", i->second, 1.0 / color_alpha);
  }
  return color_utils::SkColorToRgbaString(color);
}

std::string ConvertColorProviderColorIdToCSSColorId(std::string color_id_name) {
  color_id_name.replace(color_id_name.begin(), color_id_name.begin() + 1, "-");
  std::string css_color_id_name;
  for (char i : color_id_name) {
    if (base::IsAsciiUpper(i))
      css_color_id_name += std::string("-");
    css_color_id_name += base::ToLowerASCII(i);
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

ColorProvider CreateColorProviderFromRendererColorMap(
    const RendererColorMap& renderer_color_map) {
  ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();

  for (const auto& table : kRendererColorIdMap)
    mixer[table.color_id] = {renderer_color_map.at(table.renderer_color_id)};
  color_provider.GenerateColorMap();

  return color_provider;
}

ColorProvider CreateEmulatedForcedColorsColorProvider(bool dark_mode) {
  ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();
  // Colors were chosen based on Windows 10 default light and dark high contrast
  // themes.
  mixer[kColorForcedBtnFace] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorForcedBtnText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
  mixer[kColorForcedGrayText] = {dark_mode ? SkColorSetRGB(0x3F, 0xF2, 0x3F)
                                           : SkColorSetRGB(0x60, 0x00, 0x00)};
  mixer[kColorForcedHighlight] = {dark_mode ? SkColorSetRGB(0x1A, 0xEB, 0xFF)
                                            : SkColorSetRGB(0x37, 0x00, 0x6E)};
  mixer[kColorForcedHighlightText] = {dark_mode ? SK_ColorBLACK
                                                : SK_ColorWHITE};
  mixer[kColorForcedHotlight] = {dark_mode ? SkColorSetRGB(0xFF, 0xFF, 0x00)
                                           : SkColorSetRGB(0x00, 0x00, 0x9F)};
  mixer[kColorForcedMenuHilight] = {dark_mode ? SkColorSetRGB(0x80, 0x00, 0x80)
                                              : SK_ColorBLACK};
  mixer[kColorForcedScrollbar] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorForcedWindow] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorForcedWindowText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};

  // Set the colors for the scrollbar parts based on the emulated definitions
  // above.
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      kColorForcedHighlight};
  mixer[kColorWebNativeControlScrollbarCorner] = {kColorForcedBtnFace};
  CompleteControlsForcedColorsDefinition(mixer);
  color_provider.GenerateColorMap();
  return color_provider;
}

ColorProvider CreateEmulatedForcedColorsColorProviderForTest() {
  ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();

  mixer[kColorWebNativeControlAccent] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAccentDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlAccentHovered] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAccentPressed] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlAutoCompleteBackground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlBackground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlBackgroundDisabled] = {SK_ColorBLACK};
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
  mixer[kColorWebNativeControlFill] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillDisabled] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillHovered] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlFillPressed] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlLightenLayer] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlProgressValue] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarCorner] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarThumb] = {SK_ColorBLACK};
  mixer[kColorWebNativeControlScrollbarThumbHovered] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarThumbInactive] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlScrollbarThumbPressed] = {
      SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorWebNativeControlScrollbarTrack] = {SK_ColorWHITE};
  mixer[kColorWebNativeControlSlider] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlSliderDisabled] = {SK_ColorGREEN};
  mixer[kColorWebNativeControlSliderHovered] = {SK_ColorCYAN};
  mixer[kColorWebNativeControlSliderPressed] = {SK_ColorCYAN};

  color_provider.GenerateColorMap();
  return color_provider;
}

ColorProvider COMPONENT_EXPORT(COLOR)
    CreateColorProviderForBlinkTests(bool dark_mode) {
  ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();

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
    mixer[kColorWebNativeControlBackground] = {SkColorSetRGB(0x3B, 0x3B, 0x3B)};
    mixer[kColorWebNativeControlBackgroundDisabled] = {
        SkColorSetRGB(0x3B, 0x3B, 0x3B)};
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
    mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
        SkColorSetRGB(0x4F, 0x4F, 0x4F)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
        SkColorSetRGB(0xB1, 0xB1, 0xB1)};
    mixer[kColorWebNativeControlScrollbarArrowForeground] = {SK_ColorWHITE};
    mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
        SK_ColorBLACK};
    mixer[kColorWebNativeControlScrollbarCorner] = {
        SkColorSetRGB(0x12, 0x12, 0x12)};
    mixer[kColorWebNativeControlScrollbarThumb] = {
        SkColorSetA(SK_ColorWHITE, 0x33)};
    mixer[kColorWebNativeControlScrollbarThumbHovered] = {
        SkColorSetA(SK_ColorWHITE, 0x4D)};
    mixer[kColorWebNativeControlScrollbarThumbInactive] = {SK_ColorWHITE};
    mixer[kColorWebNativeControlScrollbarThumbPressed] = {
        SkColorSetA(SK_ColorWHITE, 0x80)};
    mixer[kColorWebNativeControlScrollbarTrack] = {
        SkColorSetRGB(0x42, 0x42, 0x42)};
    mixer[kColorWebNativeControlSlider] = {SkColorSetRGB(0x99, 0xC8, 0xFF)};
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
    mixer[kColorWebNativeControlBackground] = {SK_ColorWHITE};
    mixer[kColorWebNativeControlBackgroundDisabled] = {
        SkColorSetA(SK_ColorWHITE, 0x99)};
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
    mixer[kColorWebNativeControlScrollbarArrowBackgroundHovered] = {
        SkColorSetRGB(0xD2, 0xD2, 0xD2)};
    mixer[kColorWebNativeControlScrollbarArrowBackgroundPressed] = {
        SkColorSetRGB(0x78, 0x78, 0x78)};
    mixer[kColorWebNativeControlScrollbarArrowForeground] = {
        SkColorSetRGB(0x50, 0x50, 0x50)};
    mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
        SK_ColorWHITE};
    mixer[kColorWebNativeControlScrollbarCorner] = {
        SkColorSetRGB(0xDC, 0xDC, 0xDC)};
    mixer[kColorWebNativeControlScrollbarThumb] = {
        SkColorSetA(SK_ColorBLACK, 0x33)};
    mixer[kColorWebNativeControlScrollbarThumbHovered] = {
        SkColorSetA(SK_ColorBLACK, 0x4D)};
    mixer[kColorWebNativeControlScrollbarThumbInactive] = {
        SkColorSetRGB(0xEA, 0xEA, 0xEA)};
    mixer[kColorWebNativeControlScrollbarThumbPressed] = {
        SkColorSetA(SK_ColorBLACK, 0x80)};
    mixer[kColorWebNativeControlScrollbarTrack] = {
        SkColorSetRGB(0xF1, 0xF1, 0xF1)};
    mixer[kColorWebNativeControlSlider] = {SkColorSetRGB(0x00, 0x75, 0xFF)};
    mixer[kColorWebNativeControlSliderDisabled] = {
        SkColorSetRGB(0xCB, 0xCB, 0xCB)};
    mixer[kColorWebNativeControlSliderHovered] = {
        SkColorSetRGB(0x00, 0x5C, 0xC8)};
    mixer[kColorWebNativeControlSliderPressed] = {
        SkColorSetRGB(0x37, 0x93, 0xFF)};
  }

  color_provider.GenerateColorMap();
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
  mixer[kColorWebNativeControlScrollbarThumbInactive] = {
      kColorWebNativeControlScrollbarThumb};
  mixer[kColorWebNativeControlScrollbarThumbPressed] = {
      kColorWebNativeControlScrollbarThumbHovered};
  mixer[kColorWebNativeControlScrollbarTrack] = {
      kColorWebNativeControlScrollbarCorner};
}

void CompleteControlsForcedColorsDefinition(ui::ColorMixer& mixer) {
  mixer[kColorWebNativeControlAccent] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlAccentDisabled] = {kColorForcedGrayText};
  mixer[kColorWebNativeControlAccentHovered] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlAccentPressed] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlAutoCompleteBackground] = {kColorForcedWindow};
  mixer[kColorWebNativeControlBackground] = {kColorForcedWindow};
  mixer[kColorWebNativeControlBackgroundDisabled] = {kColorForcedWindow};
  mixer[kColorWebNativeControlBorder] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlBorderDisabled] = {kColorForcedGrayText};
  mixer[kColorWebNativeControlBorderHovered] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlBorderPressed] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlButtonBorder] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlButtonBorderDisabled] = {kColorForcedGrayText};
  mixer[kColorWebNativeControlButtonBorderHovered] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlButtonBorderPressed] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlButtonFill] = {kColorForcedWindow};
  mixer[kColorWebNativeControlButtonFillDisabled] = {kColorForcedWindow};
  mixer[kColorWebNativeControlButtonFillHovered] = {kColorForcedWindow};
  mixer[kColorWebNativeControlButtonFillPressed] = {kColorForcedWindow};
  mixer[kColorWebNativeControlFill] = {kColorForcedWindow};
  mixer[kColorWebNativeControlFillDisabled] = {kColorForcedWindow};
  mixer[kColorWebNativeControlFillHovered] = {kColorForcedWindow};
  mixer[kColorWebNativeControlFillPressed] = {kColorForcedWindow};
  mixer[kColorWebNativeControlLightenLayer] = {kColorForcedWindow};
  mixer[kColorWebNativeControlProgressValue] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlScrollbarArrowForeground] = {kColorForcedBtnText};
  mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
      kColorForcedHighlight};
  mixer[kColorWebNativeControlScrollbarCorner] = {kColorForcedBtnFace};
  mixer[kColorWebNativeControlSlider] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlSliderDisabled] = {kColorForcedGrayText};
  mixer[kColorWebNativeControlSliderHovered] = {kColorForcedHighlight};
  mixer[kColorWebNativeControlSliderPressed] = {kColorForcedHighlight};
  CompleteScrollbarColorsDefinition(mixer);
}

bool IsRendererColorMappingEquivalent(
    const ColorProvider& color_provider,
    const RendererColorMap& renderer_color_map) {
  for (const auto& table : kRendererColorIdMap) {
    // The `renderer_color_map_` should map the full set of renderer color ids.
    DCHECK(base::Contains(renderer_color_map, table.renderer_color_id));
    if (color_provider.GetColor(table.color_id) !=
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
