// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/css_system_color_mixer.h"

#include "build/build_config.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
void MapNativeColorsToCssSystemColors(ColorMixer& mixer, ColorProviderKey key) {
}
#endif

void AddDuskPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorCssSystemBtnFace] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorCssSystemBtnText] = {SkColorSetRGB(0xB6, 0xF6, 0xF0)};
  mixer[kColorCssSystemGrayText] = {SkColorSetRGB(0xA6, 0xA6, 0xA6)};
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0xA1, 0xBF, 0xDE)};
  mixer[kColorCssSystemHighlightText] = {SkColorSetRGB(0x19, 0x22, 0x2D)};
  mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x70, 0xEB, 0xDE)};
  mixer[kColorCssSystemMenuHilight] = {SkColorSetRGB(0x80, 0x00, 0x80)};
  mixer[kColorCssSystemScrollbar] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorCssSystemWindow] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorCssSystemWindowText] = {SK_ColorWHITE};
}

void AddDesertPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorCssSystemBtnFace] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorCssSystemBtnText] = {SkColorSetRGB(0x20, 0x20, 0x20)};
  mixer[kColorCssSystemGrayText] = {SkColorSetRGB(0x67, 0x67, 0x67)};
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0x90, 0x39, 0x09)};
  mixer[kColorCssSystemHighlightText] = {SkColorSetRGB(0xFF, 0xF5, 0xE3)};
  mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x00, 0x63, 0xB3)};
  mixer[kColorCssSystemMenuHilight] = {SK_ColorBLACK};
  mixer[kColorCssSystemScrollbar] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorCssSystemWindow] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorCssSystemWindowText] = {SkColorSetRGB(0x3D, 0x3D, 0x3D)};
}

void AddNightSkyPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorCssSystemBtnFace] = {SK_ColorBLACK};
  mixer[kColorCssSystemBtnText] = {SkColorSetRGB(0xFF, 0xEE, 0x32)};
  mixer[kColorCssSystemGrayText] = {SkColorSetRGB(0xA6, 0xA6, 0xA6)};
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0xD5, 0xB4, 0xFD)};
  mixer[kColorCssSystemHighlightText] = {SkColorSetRGB(0x2B, 0x2B, 0x2B)};
  mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x80, 0x80, 0xFF)};
  mixer[kColorCssSystemMenuHilight] = {SkColorSetRGB(0x80, 0x00, 0x80)};
  mixer[kColorCssSystemScrollbar] = {SK_ColorBLACK};
  mixer[kColorCssSystemWindow] = {SK_ColorBLACK};
  mixer[kColorCssSystemWindowText] = {SK_ColorWHITE};
}

void AddWhitePageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorCssSystemBtnFace] = {SK_ColorWHITE};
  mixer[kColorCssSystemBtnText] = {SK_ColorBLACK};
  mixer[kColorCssSystemGrayText] = {SkColorSetRGB(0x60, 0x00, 0x00)};
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0x37, 0x00, 0x6E)};
  mixer[kColorCssSystemHighlightText] = {SK_ColorWHITE};
  mixer[kColorCssSystemHotlight] = {SkColorSetRGB(0x00, 0x00, 0x9F)};
  mixer[kColorCssSystemMenuHilight] = {SK_ColorBLACK};
  mixer[kColorCssSystemScrollbar] = {SK_ColorWHITE};
  mixer[kColorCssSystemWindow] = {SK_ColorWHITE};
  mixer[kColorCssSystemWindowText] = {SK_ColorBLACK};
}

void AddCssSystemColorMixer(ColorProvider* provider,
                            const ColorProviderKey& key) {
  ColorMixer& mixer = provider->AddMixer();
  const ColorProviderKey::ForcedColors forced_colors = key.forced_colors;

  switch (forced_colors) {
    case ColorProviderKey::ForcedColors::kEmulated: {
      AddEmulatedForcedColorsToMixer(
          mixer,
          /*dark_mode=*/key.color_mode == ColorProviderKey::ColorMode::kDark);
      break;
    }
    case ColorProviderKey::ForcedColors::kNone:
      CompleteDefaultCssSystemColorDefinition(
          mixer,
          /*dark_mode=*/key.color_mode == ColorProviderKey::ColorMode::kDark);
      MapNativeColorsToCssSystemColors(mixer, key);
      break;
    case ColorProviderKey::ForcedColors::kActive:
      MapNativeColorsToCssSystemColors(mixer, key);
      break;
    case ColorProviderKey::ForcedColors::kDusk:
      AddDuskPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kDesert:
      AddDesertPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kNightSky:
      AddNightSkyPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kWhite:
      AddWhitePageColorsToMixer(mixer);
      break;
    default:
      NOTREACHED();
  }

  // Don't apply system colors to web native controls if forced colors is
  // `kNone`.
  if (forced_colors != ColorProviderKey::ForcedColors::kNone) {
    CompleteControlsForcedColorsDefinition(mixer);
  }
}

}  // namespace ui
