// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DISPLAY_UTIL_H_
#define UI_BASE_X_X11_DISPLAY_UTIL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/display/display.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

// Return the version for xrandr. It multiplies the major number by 100 and
// adds the minor like MAJOR * 100 + MINOR. It returns zero if no xrandr is
// present.
COMPONENT_EXPORT(UI_BASE_X) int GetXrandrVersion(XDisplay* xdisplay);

// Builds a list of displays for fallback.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<display::Display> GetFallbackDisplayList(float scale);

// Builds a list of displays from the current screen information offered by
// the X server.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    int version,
    float scale,
    int64_t* primary_display_index_out);

// Returns the refresh interval of the primary display. If there is no connected
// primary display, returns the refresh interval of the first connected display.
COMPONENT_EXPORT(UI_BASE_X)
base::TimeDelta GetPrimaryDisplayRefreshIntervalFromXrandr(Display* display);

}  // namespace ui

#endif  // UI_BASE_X_X11_DISPLAY_UTIL_H_
