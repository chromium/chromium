// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_FINDER_H_
#define UI_DISPLAY_DISPLAY_FINDER_H_

#include <vector>

#include "ui/display/display_export.h"

namespace gfx {
class Point;
class Rect;
}

namespace display {
class Display;

// Returns the display containing |point|. If no displays contain |point|, then
// this returns the display closest to |point|.
DISPLAY_EXPORT const Display* FindDisplayNearestPoint(
    const std::vector<Display>& displays,
    const gfx::Point& point);

// Returns the display in |displays| with the biggest intersection of |rect|.
// If none of the displays intersect |rect| null is returned.
DISPLAY_EXPORT const Display* FindDisplayWithBiggestIntersection(
    const std::vector<Display>& displays,
    const gfx::Rect& rect);

// Returns an iterator into |displays| of the Display whose bounds contains
// |point_in_screen|, or displays.end() if no Displays contains
// |point_in_screen|.
DISPLAY_EXPORT std::vector<Display>::const_iterator FindDisplayContainingPoint(
    const std::vector<Display>& displays,
    const gfx::Point& point_in_screen);

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_FINDER_H_
