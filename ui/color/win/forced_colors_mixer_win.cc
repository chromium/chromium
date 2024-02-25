// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

// Adds the default Windows system colors to `mixer` when High Contrast mode is
// enabled.
void AddSystemForcedColorsToMixer(ColorMixer& mixer) {
  mixer[kColorForcedBtnFace] = {kColorNativeBtnFace};
  mixer[kColorForcedBtnText] = {kColorNativeBtnText};
  mixer[kColorForcedGrayText] = {kColorNativeGrayText};
  mixer[kColorForcedHighlight] = {kColorNativeHighlight};
  mixer[kColorForcedHighlightText] = {kColorNativeHighlightText};
  mixer[kColorForcedHotlight] = {kColorNativeHotlight};
  mixer[kColorForcedMenuHilight] = {kColorNativeMenuHilight};
  mixer[kColorForcedScrollbar] = {kColorNativeScrollbar};
  mixer[kColorForcedWindow] = {kColorNativeWindow};
  mixer[kColorForcedWindowText] = {kColorNativeWindowText};
}

}  // namespace ui
