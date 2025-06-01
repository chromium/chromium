// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MONOGRAM_UTILS_H_
#define UI_GFX_MONOGRAM_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;

// Draws a monogram in a colored circle on the passed-in `canvas`.
// `monogram_text` is a string in order to support 2 letter monograms.
//
// TODO(https://crbug.com/421202274): This is duplicative of
// chrome/browser/shortcuts/shortcut_icon_generator.h which also draws a letter
// in a shape; figure out how to be less redundant.
COMPONENT_EXPORT(GFX)
void DrawMonogramInCanvas(Canvas* canvas,
                          int canvas_size,
                          int circle_size,
                          std::u16string_view monogram_text,
                          const std::vector<std::string>& font_names,
                          SkColor monogram_color,
                          SkColor background_color);

}  // namespace gfx

#endif  // UI_GFX_MONOGRAM_UTILS_H_
