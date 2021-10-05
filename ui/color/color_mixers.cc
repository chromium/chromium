// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "ui/color/core_default_color_mixer.h"
#include "ui/color/native_color_mixers.h"
#include "ui/color/ui_color_mixer.h"

namespace ui {

void AddColorMixers(ColorProvider* provider,
                    ColorProviderManager::ColorMode color_mode,
                    ColorProviderManager::ContrastMode contrast_mode,
                    ColorProviderManager::SystemTheme system_theme) {
  const bool dark_mode = color_mode == ColorProviderManager::ColorMode::kDark;
  const bool high_contrast =
      contrast_mode == ColorProviderManager::ContrastMode::kHigh;
  ui::AddCoreDefaultColorMixer(provider, dark_mode, high_contrast);
  ui::AddNativeCoreColorMixer(provider, dark_mode, high_contrast);
  ui::AddUiColorMixer(provider, dark_mode, high_contrast);
  ui::AddNativeUiColorMixer(provider, dark_mode, high_contrast);
  ui::AddNativePostprocessingMixer(provider);
}

}  // namespace ui
