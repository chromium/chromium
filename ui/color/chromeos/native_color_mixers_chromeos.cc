// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderManager::Key& key) {
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorAshSystemUIMenuBackground] = {kColorMenuBackground};
  mixer[kColorAshSystemUIMenuIcon] = {kColorMenuIcon};
  mixer[kColorAshSystemUIMenuItemBackgroundSelected] = {
      kColorMenuItemBackgroundSelected};
  mixer[kColorAshSystemUIMenuSeparator] = {kColorMenuSeparator};
  if (key.color_mode == ColorProviderManager::ColorMode::kDark) {
    const bool high_elevation =
        key.elevation_mode == ColorProviderManager::ElevationMode::kHigh;
    const SkColor base_color =
        high_elevation
            ? color_utils::AlphaBlend(SK_ColorWHITE, gfx::kGoogleGrey900, 0.08f)
            : gfx::kGoogleGrey900;
    mixer[kColorNativeColor1] = {gfx::kGoogleBlue400};
    mixer[kColorNativeColor1Shade1] = {color_utils::AlphaBlend(
        gfx::kGoogleBlue600, base_color, high_elevation ? 0.4f : 0.3f)};
    mixer[kColorNativeColor1Shade2] = {
        color_utils::AlphaBlend(gfx::kGoogleBlue300, base_color, 0.3f)};
    mixer[kColorNativeColor2] = {gfx::kGoogleGreen400};
    mixer[kColorNativeColor3] = {gfx::kGoogleYellow400};
    mixer[kColorNativeColor4] = {gfx::kGoogleRed500};
    mixer[kColorNativeColor5] = {gfx::kGoogleMagenta300};
    mixer[kColorNativeColor6] = {gfx::kGoogleElectric300};
    mixer[kColorNativeBaseColor] = {base_color};
    mixer[kColorNativeSecondaryColor] = {
        high_elevation
            ? gfx::kGoogleGrey700
            : color_utils::AlphaBlend(gfx::kGoogleGrey200, base_color, 0.3f)};
  } else {
    mixer[kColorNativeColor1] = {gfx::kGoogleBlue500};
    mixer[kColorNativeColor1Shade1] = {gfx::kGoogleBlue300};
    mixer[kColorNativeColor1Shade2] = {gfx::kGoogleBlue100};
    mixer[kColorNativeColor2] = {gfx::kGoogleGreen500};
    mixer[kColorNativeColor3] = {gfx::kGoogleYellow500};
    mixer[kColorNativeColor4] = {gfx::kGoogleRed500};
    mixer[kColorNativeColor5] = {gfx::kGoogleMagenta400};
    mixer[kColorNativeColor6] = {gfx::kGoogleElectric400};
    mixer[kColorNativeBaseColor] = {SK_ColorWHITE};
    mixer[kColorNativeSecondaryColor] = {gfx::kGoogleGrey100};
  }
}

}  // namespace ui
