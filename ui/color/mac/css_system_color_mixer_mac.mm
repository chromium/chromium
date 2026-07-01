// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/feature_list.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

// Maps the native Mac system colors to their corresponding CSS system colors.
void MapNativeColorsToCssSystemColors(ColorMixer& mixer, ColorProviderKey key) {
  // The default blue color for the system highlight, can be obtained through:
  // `NSSystemColorToSkColor(NSColor.selectedTextBackgroundColor)`.
  // This results in #b3d7ff.
  //
  // The actual system color isn't used due to fingerprinting concerns.
  // See: https://crbug.com/436597797
  mixer[kColorCssSystemHighlight] = {SkColorSetRGB(0xb3, 0xd7, 0xff)};
  mixer[kColorCssSystemHighlightText] = {SK_ColorBLACK};
}

}  // namespace ui
