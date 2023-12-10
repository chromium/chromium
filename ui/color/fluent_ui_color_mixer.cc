// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/fluent_ui_color_mixer.h"

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

void AddFluentUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key) {
  ColorMixer& mixer = provider->AddMixer();
  if (key.contrast_mode == ColorProviderKey::ContrastMode::kNormal) {
    const bool dark_mode = key.color_mode == ColorProviderKey::ColorMode::kDark;

    mixer[kColorWebNativeControlScrollbarArrowForeground] = {
        dark_mode ? SkColorSetA(SK_ColorWHITE, 0x8B)
                  : SkColorSetA(SK_ColorBLACK, 0x72)};
    mixer[kColorWebNativeControlScrollbarArrowForegroundPressed] = {
        dark_mode ? SkColorSetA(SK_ColorWHITE, 0xC8)
                  : SkColorSetA(SK_ColorBLACK, 0x9B)};
    mixer[kColorWebNativeControlScrollbarCorner] = {
        dark_mode ? SkColorSetRGB(0x2C, 0x2C, 0x2C)
                  : SkColorSetRGB(0xFC, 0xFC, 0xFC)};
  }
  CompleteScrollbarColorsDefinition(mixer);
}

}  // namespace ui
