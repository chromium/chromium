// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/win/forced_colors_mixer_win.h"

#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

// TODO(crbug.com/1231644): Implementation of the new browser setting Page
// Colors will allow users to enable forced colors mode on different platforms.
// Need to ensure compatibility with the existing forced colors mode on Windows.
void AddSystemForcedColorsColorMixer(ColorProvider* provider,
                                     const ColorProviderKey& key) {
  ColorMixer& mixer = provider->AddMixer();

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
  CompleteControlsForcedColorsDefinition(mixer);
}

}  // namespace ui
