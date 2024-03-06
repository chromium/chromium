// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_recipe.h"

namespace ui {

// Maps the native Windows system colors to their corresponding CSS system
// colors when in light or High Contrast mode.
void MapNativeColorsToCSSSystemColors(ColorMixer& mixer) {
  mixer[kColorCssSystemBtnFace] = {kColorNativeBtnFace};
  mixer[kColorCssSystemBtnText] = {kColorNativeBtnText};
  mixer[kColorCssSystemGrayText] = {kColorNativeGrayText};
  mixer[kColorCssSystemHighlight] = {kColorNativeHighlight};
  mixer[kColorCssSystemHighlightText] = {kColorNativeHighlightText};
  mixer[kColorCssSystemHotlight] = {kColorNativeHotlight};
  mixer[kColorCssSystemMenuHilight] = {kColorNativeMenuHilight};
  mixer[kColorCssSystemScrollbar] = {kColorNativeScrollbar};
  mixer[kColorCssSystemWindow] = {kColorNativeWindow};
  mixer[kColorCssSystemWindowText] = {kColorNativeWindowText};
}

}  // namespace ui
