// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCALING_UTIL_H_
#define UI_DISPLAY_WIN_SCALING_UTIL_H_

#include <stdint.h>

#include "ui/display/display_export.h"
#include "ui/display/display_layout.h"
#include "ui/display/win/display_info.h"

namespace gfx {
class Rect;
}

namespace display {
namespace win {

// Whether or not |a| shares an edge with |b|.
DISPLAY_EXPORT bool DisplayInfosTouch(const internal::DisplayInfo& a,
                                      const internal::DisplayInfo& b);

// Returns a DisplayPlacement for |current| relative to |parent|.
// Note that DisplayPlacement's are always in DIPs, so this also performs the
// required scaling.
//
// Examples (The offset is indicated by the arrow.):
// Scaled and Unscaled Coordinates
// +--------------+    +          Since both DisplayInfos are of the same scale
// |              |    |          factor, relative positions remain the same.
// |    Parent    |    V
// |      1x      +----------+
// |              |          |
// +--------------+  Current |
//                |    1x    |
//                +----------+
//
// Unscaled Coordinates
// +--------------+               The 2x DisplayInfo is offset to maintain a
// |              |               similar neighboring relationship with the 1x
// |    Parent    |               parent. Current's position is based off of the
// |      1x      +----------+    percentage position along its parent. This
// |              |          |    percentage position is preserved in the scaled
// +--------------+  Current |    coordinates.
//                |    2x    |
//                +----------+
// Scaled Coordinates
// +--------------+  +
// |              |  |
// |    Parent    |  V
// |      1x      +-----+
// |              + C 2x|
// +--------------+-----+
//
//
// Unscaled Coordinates
// +--------------+               The parent DisplayInfo has a 2x scale factor.
// |              |               The offset is adjusted to maintain the
// |              |               relative positioning of the 1x DisplayInfo in
// |    Parent    +----------+    the scaled coordinate space. Current's
// |      2x      |          |    position is based off of the percentage
// |              |  Current |    position along its parent. This percentage
// |              |    1x    |    position is preserved in the scaled
// +--------------+          |    coordinates.
//                |          |
//                +----------+
// Scaled Coordinates
// +-------+    +
// |       |    V
// | Parent+----------+
// |   2x  |          |
// +-------+  Current |
//         |    1x    |
//         |          |
//         |          |
//         +----------+
//
// Unscaled Coordinates
//         +----------+           In this case, parent lies between the top and
//         |          |           bottom of parent. The roles are reversed when
// +-------+          |           this occurs, and current is placed to maintain
// |       |  Current |           parent's relative position along current.
// | Parent|    1x    |
// |   2x  |          |
// +-------+          |
//         +----------+
// Scaled Coordinates
//  ^      +----------+
//  |      |          |
//  + +----+          |
//    |Prnt|  Current |
//    | 2x |    1x    |
//    +----+          |
//         |          |
//         +----------+
//
// Scaled and Unscaled Coordinates
// +--------+                     If the two DisplayInfos are bottom aligned or
// |        |                     right aligned, the DisplayPlacement will
// |        +--------+            have an offset of 0 relative to the
// |        |        |            bottom-right of the DisplayInfo.
// |        |        |
// +--------+--------+
DISPLAY_EXPORT DisplayPlacement
CalculateDisplayPlacement(const internal::DisplayInfo& parent,
                          const internal::DisplayInfo& current);

// Returns the squared distance between two rects.
// The distance between two rects is the length of the shortest segment that can
// be drawn between two rectangles. This segment generally connects two opposing
// corners between rectangles like this...
//
// +----------+
// |          |
// +----------+
//             \  <--- Shortest Segment
//              \
//               +---+
//               |   |
//               |   |
//               +---+
//
// For rectangles that share coordinates within the same axis, that generally
// means the segment is parallel to the axis and perpendicular to the edges.
//
//                 One of many shortest segments
//  +----------+  /                            \    +--------+
//  |          |  |                             \   |        |
//  |          |  V  +---+                       \  +--------+
//  |          |-----|   |                        \-->|
//  +----------+     |   |                            +----+
//                   |   |                            |    |
//                   +---+                            +----+
//
// For rectangles that intersect each other, the distance is the negative value
// of the overlapping area, so callers can distinguish different amounts of
// overlap.
//
// The squared distance is used to avoid taking the square root as the common
// usage is to compare distances greater than 1 unit.
DISPLAY_EXPORT int64_t SquaredDistanceBetweenRects(const gfx::Rect& ref,
                                                   const gfx::Rect& rect);

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCALING_UTIL_H_
