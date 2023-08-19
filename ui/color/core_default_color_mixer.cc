// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/core_default_color_mixer.h"

#include <utility>

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

void AddCoreDefaultColorMixer(ColorProvider* provider,
                              const ColorProviderKey& key) {
  const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;
  DVLOG(2) << "Adding CoreDefaultColorMixer to ColorProvider for "
           << (dark_mode ? "Dark" : "Light") << " window.";
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAccent] = {dark_mode ? gfx::kGoogleBlue300 : gfx::kGoogleBlue600};
  // 4.5 and 7.0 approximate the default light and dark theme contrasts of
  // accent-against-primary-background.
  mixer[kColorAccentWithGuaranteedContrastAtopPrimaryBackground] =
      PickGoogleColor(kColorAccent, kColorPrimaryBackground, 4.5f, 7.0f);
  mixer[kColorAlertHighSeverity] = {dark_mode ? gfx::kGoogleRed300
                                              : gfx::kGoogleRed600};
  mixer[kColorAlertLowSeverity] = {dark_mode ? gfx::kGoogleGreen300
                                             : gfx::kGoogleGreen700};
  mixer[kColorAlertMediumSeverityIcon] = {dark_mode ? gfx::kGoogleYellow300
                                                    : gfx::kGoogleYellow700};
  // Color used for alert text should more readable than the color above which
  // is for icons.
  mixer[kColorAlertMediumSeverityText] = {dark_mode ? gfx::kGoogleYellow300
                                                    : gfx::kGoogleOrange900};
  mixer[kColorDisabledForeground] =
      PickGoogleColor(gfx::kGoogleGrey600, kColorPrimaryBackground,
                      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorEndpointBackground] =
      GetColorWithMaxContrast(kColorEndpointForeground);
  mixer[kColorEndpointForeground] =
      GetColorWithMaxContrast(kColorPrimaryBackground);
  mixer[kColorItemHighlight] =
      PickGoogleColor(kColorAccent, kColorPrimaryBackground,
                      color_utils::kMinimumVisibleContrastRatio, 5.0f);
  mixer[kColorItemSelectionBackground] =
      AlphaBlend(kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
                 kColorPrimaryBackground, 0x3C);
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
  mixer[kColorSubtleAccent] =
      AlphaBlend(kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
                 kColorPrimaryBackground, gfx::kGoogleGreyAlpha400);
  mixer[kColorSubtleEmphasisBackground] =
      BlendTowardMaxContrast(kColorPrimaryBackground, gfx::kGoogleGreyAlpha100);
  mixer[kColorTextSelectionBackground] =
      AlphaBlend(kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
                 kColorPrimaryBackground, gfx::kGoogleGreyAlpha500);
  mixer[kColorTextSelectionForeground] =
      GetColorWithMaxContrast(kColorTextSelectionBackground);
}

}  // namespace ui
