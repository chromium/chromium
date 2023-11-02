// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/core_default_color_mixer.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

namespace {

// TODO(pkasting): Construct colors from contrast ratios
// TODO(pkasting): Construct palette from accent, key, tint/bg, shade/fg colors

ColorTransform GoogleColorWithContrastRatio(ColorTransform foreground_transform,
                                            ColorTransform background_transform,
                                            float contrast_ratio) {
  const auto generator = [](ColorTransform foreground_transform,
                            ColorTransform background_transform,
                            float contrast_ratio, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor foreground_color =
        foreground_transform.Run(input_color, mixer);
    const SkColor background_color =
        background_transform.Run(input_color, mixer);
    contrast_ratio *=
        color_utils::GetContrastRatio(foreground_color, background_color);
    const SkColor result_color = color_utils::PickGoogleColor(
        foreground_color, background_color, contrast_ratio);
    DVLOG(2) << "ColorTransform GoogleColorWithContrastRatio:"
             << " FG Transform Color: " << SkColorName(foreground_color)
             << " BG Transform Color: " << SkColorName(background_color)
             << " Contrast Ratio: " << base::NumberToString(contrast_ratio)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform), contrast_ratio);
}

}  // namespace

void AddCoreDefaultColorMixer(ColorProvider* provider,
                              const ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ColorProviderManager::ColorMode::kDark;
  DVLOG(2) << "Adding CoreDefaultColorMixer to ColorProvider for "
           << (dark_mode ? "Dark" : "Light") << " window.";
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAccent] = {dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600};
  mixer[kColorAlertHighSeverity] = {dark_mode ? gfx::kGoogleRed300
                                              : gfx::kGoogleRed600};
  mixer[kColorAlertLowSeverity] = {dark_mode ? gfx::kGoogleGreen300
                                             : gfx::kGoogleGreen700};
  mixer[kColorAlertMediumSeverity] = {dark_mode ? gfx::kGoogleYellow300
                                                : gfx::kGoogleYellow700};
  mixer[kColorDisabledForeground] = BlendForMinContrast(
      gfx::kGoogleGrey600, kColorPrimaryBackground, kColorPrimaryForeground);
  mixer[kColorEndpointBackground] =
      GetColorWithMaxContrast(kColorEndpointForeground);
  mixer[kColorEndpointForeground] =
      GetColorWithMaxContrast(kColorPrimaryBackground);
  // This produces light and dark item highlight colors of blue 500 and 400,
  // respectively.
  mixer[kColorItemHighlight] = GoogleColorWithContrastRatio(
      kColorAccent, kColorPrimaryBackground, 0.75f);
  mixer[kColorItemSelectionBackground] =
      AlphaBlend(kColorAccent, kColorPrimaryBackground, 0x3C);
  mixer[kColorMenuSelectionBackground] =
      AlphaBlend(kColorEndpointForeground, kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha200);
  mixer[kColorMidground] = {dark_mode ? gfx::kGoogleGrey800
                                      : gfx::kGoogleGrey300};
  mixer[kColorPrimaryBackground] = {dark_mode ? SkColorSetRGB(0x29, 0x2A, 0x2D)
                                              : SK_ColorWHITE};
  mixer[kColorPrimaryForeground] = {dark_mode ? gfx::kGoogleGrey200
                                              : gfx::kGoogleGrey900};
  mixer[kColorSecondaryForeground] = {dark_mode ? gfx::kGoogleGrey500
                                                : gfx::kGoogleGrey700};
  mixer[kColorSubtleAccent] = AlphaBlend(kColorAccent, kColorPrimaryBackground,
                                         gfx::kGoogleGreyAlpha400);
  mixer[kColorSubtleEmphasisBackground] =
      BlendTowardMaxContrast(kColorPrimaryBackground, gfx::kGoogleGreyAlpha100);
  mixer[kColorTextSelectionBackground] = AlphaBlend(
      kColorAccent, kColorPrimaryBackground, gfx::kGoogleGreyAlpha500);
  mixer[kColorTextSelectionForeground] =
      GetColorWithMaxContrast(kColorTextSelectionBackground);
}

}  // namespace ui
