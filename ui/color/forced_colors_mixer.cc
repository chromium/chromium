// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/forced_colors_mixer.h"

#include "build/build_config.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

#if !BUILDFLAG(IS_WIN)
void AddSystemForcedColorsToMixer(ColorMixer& mixer) {}
#endif

void AddEmulatedForcedColorsToMixer(ColorMixer& mixer, bool dark_mode) {
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
}

void AddDuskPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorForcedBtnFace] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorForcedBtnText] = {SkColorSetRGB(0xB6, 0xF6, 0xF0)};
  mixer[kColorForcedGrayText] = {SkColorSetRGB(0xA6, 0xA6, 0xA6)};
  mixer[kColorForcedHighlight] = {SkColorSetRGB(0xA1, 0xBF, 0xDE)};
  mixer[kColorForcedHighlightText] = {SkColorSetRGB(0x19, 0x22, 0x2D)};
  mixer[kColorForcedHotlight] = {SkColorSetRGB(0x70, 0xEB, 0xDE)};
  mixer[kColorForcedMenuHilight] = {SkColorSetRGB(0x80, 0x00, 0x80)};
  mixer[kColorForcedScrollbar] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorForcedWindow] = {SkColorSetRGB(0x2D, 0x32, 0x36)};
  mixer[kColorForcedWindowText] = {SK_ColorWHITE};
}

void AddDesertPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorForcedBtnFace] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorForcedBtnText] = {SkColorSetRGB(0x20, 0x20, 0x20)};
  mixer[kColorForcedGrayText] = {SkColorSetRGB(0x67, 0x67, 0x67)};
  mixer[kColorForcedHighlight] = {SkColorSetRGB(0x90, 0x39, 0x09)};
  mixer[kColorForcedHighlightText] = {SkColorSetRGB(0xFF, 0xF5, 0xE3)};
  mixer[kColorForcedHotlight] = {SkColorSetRGB(0x00, 0x63, 0xB3)};
  mixer[kColorForcedMenuHilight] = {SK_ColorBLACK};
  mixer[kColorForcedScrollbar] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorForcedWindow] = {SkColorSetRGB(0xFF, 0xFA, 0xEF)};
  mixer[kColorForcedWindowText] = {SkColorSetRGB(0x3D, 0x3D, 0x3D)};
}

void AddBlackPageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorForcedBtnFace] = {SK_ColorBLACK};
  mixer[kColorForcedBtnText] = {SK_ColorWHITE};
  mixer[kColorForcedGrayText] = {SkColorSetRGB(0x3F, 0xF2, 0x3F)};
  mixer[kColorForcedHighlight] = {SkColorSetRGB(0x1A, 0xEB, 0xFF)};
  mixer[kColorForcedHighlightText] = {SK_ColorBLACK};
  mixer[kColorForcedHotlight] = {SK_ColorYELLOW};
  mixer[kColorForcedMenuHilight] = {SkColorSetRGB(0x80, 0x00, 0x80)};
  mixer[kColorForcedScrollbar] = {SK_ColorBLACK};
  mixer[kColorForcedWindow] = {SK_ColorBLACK};
  mixer[kColorForcedWindowText] = {SK_ColorWHITE};
}

void AddWhitePageColorsToMixer(ColorMixer& mixer) {
  mixer[kColorForcedBtnFace] = {SK_ColorWHITE};
  mixer[kColorForcedBtnText] = {SK_ColorBLACK};
  mixer[kColorForcedGrayText] = {SkColorSetRGB(0x60, 0x00, 0x00)};
  mixer[kColorForcedHighlight] = {SkColorSetRGB(0x37, 0x00, 0x6E)};
  mixer[kColorForcedHighlightText] = {SK_ColorWHITE};
  mixer[kColorForcedHotlight] = {SkColorSetRGB(0x00, 0x00, 0x9F)};
  mixer[kColorForcedMenuHilight] = {SK_ColorBLACK};
  mixer[kColorForcedScrollbar] = {SK_ColorWHITE};
  mixer[kColorForcedWindow] = {SK_ColorWHITE};
  mixer[kColorForcedWindowText] = {SK_ColorBLACK};
}

void AddForcedColorsColorMixer(ColorProvider* provider,
                               const ColorProviderKey& key) {
  const ColorProviderKey::ForcedColors forced_colors = key.forced_colors;

  if (forced_colors == ColorProviderKey::ForcedColors::kNone) {
    return;
  }

  ColorMixer& mixer = provider->AddMixer();
  switch (forced_colors) {
    case ColorProviderKey::ForcedColors::kEmulated: {
      AddEmulatedForcedColorsToMixer(
          mixer,
          /*dark_mode=*/key.color_mode == ColorProviderKey::ColorMode::kDark);
      break;
    }
    case ColorProviderKey::ForcedColors::kActive:
      AddSystemForcedColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kDusk:
      AddDuskPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kDesert:
      AddDesertPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kBlack:
      AddBlackPageColorsToMixer(mixer);
      break;
    case ColorProviderKey::ForcedColors::kWhite:
      AddWhitePageColorsToMixer(mixer);
      break;
    default:
      NOTREACHED_NORETURN();
  }

  CompleteControlsForcedColorsDefinition(mixer);
}

}  // namespace ui
