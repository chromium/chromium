// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

// Maps the native Windows system colors to their corresponding CSS system
// colors.
void MapNativeColorsToCssSystemColors(ColorMixer& mixer, ColorProviderKey key) {
  // Windows' native colors don't get updated for dark mode, so unless we're
  // forcing system colors (in which case we assume they're correct), only use
  // them in light mode.
  if (key.forced_colors == ColorProviderKey::ForcedColors::kSystem ||
      key.color_mode == ColorProviderKey::ColorMode::kLight) {
    mixer[kColorCssSystemBtnFace] = {kColorNativeBtnFace};
    mixer[kColorCssSystemBtnText] = {kColorNativeBtnText};
    mixer[kColorCssSystemGrayText] = {kColorNativeGrayText};
    mixer[kColorCssSystemHotlight] = {kColorNativeHotlight};
    mixer[kColorCssSystemMenuHilight] = {kColorNativeMenuHilight};
    mixer[kColorCssSystemScrollbar] = {kColorNativeScrollbar};
    mixer[kColorCssSystemWindow] = {kColorNativeWindow};
    mixer[kColorCssSystemWindowText] = {kColorNativeWindowText};
    mixer[kColorCssSystemField] = {kColorCssSystemWindow};
    mixer[kColorCssSystemFieldText] = {kColorCssSystemWindowText};
    mixer[kColorCssSystemActiveText] = {kColorCssSystemHotlight};
    mixer[kColorCssSystemLinkText] = {kColorCssSystemHotlight};
    mixer[kColorCssSystemVisitedText] = {kColorCssSystemHotlight};
  }

  // In general, leave the Highlight colors at their defaults, which match the
  // text selection colors as required by the CSS specification.
  // https://www.w3.org/TR/css-color-4/#css-system-colors
  //
  // If we are forcing system colors, however, text selection evaluation
  // looks to the system highlight color for resolution. The two will still
  // be in sync, but this becomes the source of truth, so should be mapped
  // to the actual native color. crbug.com/40262493
  if (key.forced_colors == ColorProviderKey::ForcedColors::kSystem) {
    mixer[kColorCssSystemHighlight] = {kColorNativeHighlight};
    mixer[kColorCssSystemHighlightText] = {kColorNativeHighlightText};
  }
}

}  // namespace ui
