// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_DISPLAY_UTIL_H_
#define UI_BASE_X_X11_DISPLAY_UTIL_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/display/display.h"
#include "ui/display/types/display_config.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// Builds a list of displays for fallback.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<display::Display> GetFallbackDisplayList(
    float scale,
    size_t* primary_display_index_out);

// Builds a list of displays from the current screen information offered by
// the X server.
COMPONENT_EXPORT(UI_BASE_X)
std::vector<display::Display> BuildDisplaysFromXRandRInfo(
    const display::DisplayConfig& display_config,
    size_t* primary_display_index_out);

// Returns the refresh interval of the display in `displays` that has the
// largest intersection with `bounds_px` (in root window pixel coordinates).
// Falls back to the display at `primary_display_index` if `bounds_px` is not
// set or doesn't intersect any display.
COMPONENT_EXPORT(UI_BASE_X)
base::TimeDelta GetDisplayRefreshInterval(
    const std::vector<display::Display>& displays,
    size_t primary_display_index,
    const std::optional<gfx::Rect>& bounds_px);

// A distance metric for ranges [min1, max1), [min2, max2).  Exposed for unit
// testing.
// - Returns 0 if the ranges touch at an endpoint but don't overlap.
//   Eg. [10, 20), [20, 30) -> 0
// - Returns a positive value if the ranges don't overlap.  The value is the
//   space between the ranges.
//   Eg. [10, 20), [30, 40) -> 10
// - Returns a negative value if the ranges overlap.  The value is the
//   amount of overlap between the ranges.
//   Eg. [10, 30), [20, 30) -> -10
// - Returns a negative value if one range fully encompasses the other.  The
//   value will have a magnitude between the size of each range.
//   Eg. [10, 40), [20, 30) -> -20
COMPONENT_EXPORT(UI_BASE_X)
int RangeDistance(int min1, int max1, int min2, int max2);

// A distance metric for rectangles.  Uses `RangeDistance()` along each
// dimension. See the comment for `RangeDistance()` for an explanation of the
// return value for overlapping or encompassing ranges.  The first value of the
// returned pair is the larger of the two distances.
COMPONENT_EXPORT(UI_BASE_X)
std::pair<int, int> RectDistance(const gfx::Rect& p, const gfx::Rect& q);

// Given `displays` with bounds in pixel coordinates, uses each display scale
// factor to update bounds to DIP coordinates.
COMPONENT_EXPORT(UI_BASE_X)
void ConvertDisplayBoundsToDips(std::vector<display::Display>* displays,
                                size_t primary_display_index);

}  // namespace ui

#endif  // UI_BASE_X_X11_DISPLAY_UTIL_H_
