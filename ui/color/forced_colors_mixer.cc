// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/forced_colors_mixer.h"

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/win/forced_colors_mixer_win.h"

namespace ui {

void AddEmulatedForcedColorsColorMixer(ColorProvider* provider,
                                       const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;
  ColorMixer& mixer = provider->AddMixer();

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
  CompleteControlsForcedColorsDefinition(mixer);
}

void AddForcedColorsColorMixer(ColorProvider* provider,
                               const ColorProviderKey& key) {
  if (key.forced_colors == ColorProviderKey::ForcedColors::kActive) {
#if BUILDFLAG(IS_WIN)
    AddSystemForcedColorsColorMixer(provider, key);
#endif  // BUILDFLAG(IS_WIN)
  } else if (key.forced_colors == ColorProviderKey::ForcedColors::kEmulated) {
    AddEmulatedForcedColorsColorMixer(provider, key);
  }
}

}  // namespace ui
