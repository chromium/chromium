// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DISPLAY_UTIL_H_
#define UI_BASE_X_X11_DISPLAY_UTIL_H_

#include "ui/base/x/ui_base_x_export.h"
#include "ui/display/display.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

// Return the version for xrandr. It multiplies the major number by 100 and
// adds the minor like MAJOR * 100 + MINOR. It returns zero if no xrandr is
// present.
UI_BASE_X_EXPORT int GetXrandrVersion(XDisplay* xdisplay);

// Builds a list of displays for fallback.
UI_BASE_X_EXPORT std::vector<display::Display> GetFallbackDisplayList(
    float scale);

// Builds a list of displays from the current screen information offered by
// the X server.
UI_BASE_X_EXPORT std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    int version,
    float scale,
    int64_t* primary_display_index_out);

}  // namespace ui

#endif  // UI_BASE_X_X11_DISPLAY_UTIL_H_
