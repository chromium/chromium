// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_finder.h"

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

using Displays = std::vector<Display>;

const Display* FindDisplayNearestPoint(const Displays& displays,
                                       const gfx::Point& point) {
  DCHECK(!displays.empty());
  auto iter = FindDisplayContainingPoint(displays, point);
  if (iter != displays.end())
    return &(*iter);

  int min_distance = std::numeric_limits<int>::max();
  const Display* nearest_display = nullptr;
  for (const auto& display : displays) {
    const int distance = display.bounds().ManhattanDistanceToPoint(point);
    if (distance < min_distance) {
      min_distance = distance;
      nearest_display = &display;
    }
  }
  // There should always be at least one display that is less than INT_MAX away.
  DCHECK(nearest_display);
  return nearest_display;
}

const Display* FindDisplayWithBiggestIntersection(const Displays& displays,
                                                  const gfx::Rect& rect) {
  DCHECK(!displays.empty());
  int max_area = 0;
  const Display* matching = nullptr;
  for (const auto& display : displays) {
    const gfx::Rect intersect = IntersectRects(display.bounds(), rect);
    const int area = intersect.width() * intersect.height();
    if (area > max_area) {
      max_area = area;
      matching = &display;
    }
  }
  return matching;
}

Displays::const_iterator FindDisplayContainingPoint(
    const Displays& displays,
    const gfx::Point& point_in_screen) {
  return std::find_if(displays.begin(), displays.end(),
                      [point_in_screen](const Display& display) {
                        return display.bounds().Contains(point_in_screen);
                      });
}

}  // namespace display
