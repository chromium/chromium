// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "skia/ext/skia_utils_mac.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"

namespace ui {

// Maps the native Mac system colors to their corresponding CSS system
// colors.
void MapNativeColorsToCssSystemColors(ColorMixer& mixer, ColorProviderKey key) {
  // TODO(samomekarajr): Consider pulling other system colors for forced colors
  // mode.
  const SkColor system_highlight_color =
      skia::NSSystemColorToSkColor(NSColor.selectedTextBackgroundColor);
  mixer[kColorCssSystemHighlight] = {system_highlight_color};
}

}  // namespace ui
