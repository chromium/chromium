// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#import <Cocoa/Cocoa.h>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/mac/system_color_utils.h"
#include "ui/gfx/color_utils.h"

namespace ui {

ColorTransform ApplySystemControlTintIfNeeded() {
  return base::BindRepeating(
      [](SkColor input_color, const ui::ColorMixer& mixer) -> SkColor {
        return IsSystemGraphiteTinted() ? ColorToGrayscale(input_color)
                                        : input_color;
      });
}

}  // namespace ui
