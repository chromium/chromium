// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_UTIL_H_
#define UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_UTIL_H_

#include <stdint.h>

#include "ui/display/display_export.h"

namespace display {
class Display;
class DisplayList;
}  // namespace display

namespace gfx {
class Rect;
class Insets;
}  // namespace gfx

namespace headless {

// Sets headless display geometry according to the specified physical bounds,
// work area insets and device pixel ratio. This is a helper function intended
// to be used by all headless screen driver implementations to ensure consistent
// headless screen geometry handling.
DISPLAY_EXPORT void SetDisplayGeometry(
    display::Display& display,
    const gfx::Rect& bounds_in_pixels,
    const gfx::Insets& work_area_insets_pixels,
    float device_pixel_ratio);

// Sets primary display for the given display list. Will crash if specified
// display does not exist in the given display list.
DISPLAY_EXPORT void SetPrimaryDisplay(display::DisplayList& display_list,
                                      int64_t display_id);

}  // namespace headless

#endif  // UI_DISPLAY_HEADLESS_HEADLESS_SCREEN_UTIL_H_
