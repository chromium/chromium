// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/scaling_util.h"

#include <algorithm>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/range/range.h"

namespace {

// Represents the amount of rotation an object has about a coordinate plane.
enum class CoordinateRotation {
  COORDINATE_ROTATE_0,
  COORDINATE_ROTATE_90,
  COORDINATE_ROTATE_180,
  COORDINATE_ROTATE_270,
};

// Returns the CoordinateRotation necessary for |ref| and |other| so that |ref|
// is positioned on top of |other|.
CoordinateRotation ComputeCoordinateRotationRefTop(const gfx::Rect& ref,
                                                   const gfx::Rect& other) {
  if (ref.bottom() <= other.y())
    return CoordinateRotation::COORDINATE_ROTATE_0;
  if (other.right() <= ref.x())
    return CoordinateRotation::COORDINATE_ROTATE_90;
  if (other.bottom() <= ref.y())
    return CoordinateRotation::COORDINATE_ROTATE_180;

  return CoordinateRotation::COORDINATE_ROTATE_270;
}

gfx::Rect CoordinateRotateRectangle90(const gfx::Rect& rect) {
  return gfx::Rect(rect.y(), -rect.x() - rect.width(),
                   rect.height(), rect.width());
}

gfx::Rect CoordinateRotateRectangle180(const gfx::Rect& rect) {
  return gfx::Rect(-rect.x() - rect.width(), -rect.y() -rect.height(),
                   rect.width(), rect.height());
}

gfx::Rect CoordinateRotateRectangle270(const gfx::Rect& rect) {
  return gfx::Rect(-rect.y() - rect.height(), rect.x(),
                   rect.height(), rect.width());
}

gfx::Rect CoordinateRotateRect(const gfx::Rect& rect,
                               CoordinateRotation rotation) {
  switch (rotation) {
    case CoordinateRotation::COORDINATE_ROTATE_90:
      return CoordinateRotateRectangle90(rect);
    case CoordinateRotation::COORDINATE_ROTATE_180:
      return CoordinateRotateRectangle180(rect);
    case CoordinateRotation::COORDINATE_ROTATE_270:
      return CoordinateRotateRectangle270(rect);
    default:
      return rect;
  }
}

bool InRange(int target, int lower_bound, int upper_bound) {
  return lower_bound <= target && target <= upper_bound;
}

// Scaled |unscaled_offset| to the same relative position on |unscaled_length|
// based off of |unscaled_length|'s |scale_factor|.
int ScaleOffset(int unscaled_length, float scale_factor, int unscaled_offset) {
  float scaled_length = static_cast<float>(unscaled_length) / scale_factor;
  float percent =
      static_cast<float>(unscaled_offset) / static_cast<float>(unscaled_length);
  return base::ClampFloor(scaled_length * percent);
}

}  // namespace

namespace display {
namespace win {

bool DisplayInfosTouch(const internal::DisplayInfo& a,
                       const internal::DisplayInfo& b) {
  const gfx::Rect& a_rect = a.screen_rect();
  const gfx::Rect& b_rect = b.screen_rect();
  int max_left = std::max(a_rect.x(), b_rect.x());
  int max_top = std::max(a_rect.y(), b_rect.y());
  int min_right = std::min(a_rect.right(), b_rect.right());
  int min_bottom = std::min(a_rect.bottom(), b_rect.bottom());
  return (max_left == min_right &&
             a_rect.y() <= b_rect.bottom() &&
             b_rect.y() <= a_rect.bottom()) ||
         (max_top == min_bottom &&
             a_rect.x() <= b_rect.right() &&
             b_rect.x() <= a_rect.right());
}

DisplayPlacement::Position CalculateDisplayPosition(
    const internal::DisplayInfo& parent,
    const internal::DisplayInfo& current) {
  const gfx::Rect& parent_rect = parent.screen_rect();
  const gfx::Rect& current_rect = current.screen_rect();
  int max_left = std::max(parent_rect.x(), current_rect.x());
  int max_top = std::max(parent_rect.y(), current_rect.y());
  int min_right = std::min(parent_rect.right(), current_rect.right());
  int min_bottom = std::min(parent_rect.bottom(), current_rect.bottom());
  if (max_left == min_right && max_top == min_bottom) {
    // Corner touching.
    if (parent_rect.bottom() == max_top)
      return DisplayPlacement::Position::BOTTOM;
    if (parent_rect.x() == max_left)
      return DisplayPlacement::Position::LEFT;

    return DisplayPlacement::Position::TOP;
  }
  if (max_left == min_right &&
      parent_rect.y() <= current_rect.bottom() &&
      current_rect.y() <= parent_rect.bottom()) {
    // Vertical edge touching.
    return parent_rect.x() == max_left
        ? DisplayPlacement::Position::LEFT
        : DisplayPlacement::Position::RIGHT;
  }
  if (max_top == min_bottom &&
      parent_rect.x() <= current_rect.right() &&
      current_rect.x() <= parent_rect.right()) {
    // Horizontal edge touching.
    return parent_rect.y() == max_top
        ? DisplayPlacement::Position::TOP
        : DisplayPlacement::Position::BOTTOM;
  }
  NOTREACHED_IN_MIGRATION()
      << "CalculateDisplayPosition relies on touching DisplayInfos.";
  return DisplayPlacement::Position::RIGHT;
}

DisplayPlacement CalculateDisplayPlacement(
    const internal::DisplayInfo& parent,
    const internal::DisplayInfo& current) {
  DCHECK(DisplayInfosTouch(parent, current)) << "DisplayInfos must touch.";

  DisplayPlacement placement;
  placement.parent_display_id = parent.id();
  placement.display_id = current.id();
  placement.position = CalculateDisplayPosition(parent, current);

  int parent_begin = 0;
  int parent_end = 0;
  int current_begin = 0;
  int current_end = 0;

  switch (placement.position) {
    case DisplayPlacement::Position::TOP:
    case DisplayPlacement::Position::BOTTOM:
      parent_begin = parent.screen_rect().x();
      parent_end = parent.screen_rect().right();
      current_begin = current.screen_rect().x();
      current_end = current.screen_rect().right();
      break;
    case DisplayPlacement::Position::LEFT:
    case DisplayPlacement::Position::RIGHT:
      parent_begin = parent.screen_rect().y();
      parent_end = parent.screen_rect().bottom();
      current_begin = current.screen_rect().y();
      current_end = current.screen_rect().bottom();
      break;
  }

  // Since we're talking offsets, make everything relative to parent_begin.
  parent_end -= parent_begin;
  current_begin -= parent_begin;
  current_end -= parent_begin;
  parent_begin = 0;

  // There are a few ways lines can intersect:
  // End Aligned
  // CURRENT's offset is relative to the end (in our world, BOTTOM_RIGHT).
  //                 +-PARENT----------------+
  //                    +-CURRENT------------+
  //
  // Positioning based off of |current_begin|.
  // CURRENT's offset is simply a percentage of its position on PARENT.
  //                 +-PARENT----------------+
  //                        +-CURRENT------------+
  //
  // Positioning based off of |current_end|.
  // CURRENT's offset is dependent on the percentage of its end position on
  // PARENT.
  //                 +-PARENT----------------+
  //           +-CURRENT------------+
  //
  // Positioning based off of |parent_begin| on current.
  // CURRENT's offset is dependent on the percentage of its end position on
  // PARENT.
  //                 +-PARENT----------------+
  //           +-CURRENT--------------------------+
  if (parent_end == current_end) {
    // End aligned.
    placement.offset_reference =
        DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
    placement.offset = 0;
  } else if (InRange(current_begin, parent_begin, parent_end)) {
    placement.offset = ScaleOffset(parent_end,
                                   parent.device_scale_factor(),
                                   current_begin);
  } else if (InRange(current_end, parent_begin, parent_end)) {
    placement.offset_reference =
        DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
    placement.offset = ScaleOffset(parent_end,
                                   parent.device_scale_factor(),
                                   parent_end - current_end);
  } else {
    DCHECK(InRange(parent_begin, current_begin, current_end));
    placement.offset = ScaleOffset(current_end - current_begin,
                                   current.device_scale_factor(),
                                   current_begin);
  }

  return placement;
}

// This function rotates the rectangles so that |ref| is always on top of
// |rect|, allowing the function to concentrate on comparing |ref|'s bottom
// corners and |rect|'s top corners when the rects don't overlap vertically.
int64_t SquaredDistanceBetweenRects(const gfx::Rect& ref,
                                    const gfx::Rect& rect) {
  gfx::Rect intersection_rect = gfx::IntersectRects(ref, rect);
  if (!intersection_rect.IsEmpty())
    return -(intersection_rect.width() * intersection_rect.height());

  CoordinateRotation degrees = ComputeCoordinateRotationRefTop(ref, rect);
  gfx::Rect top_rect(CoordinateRotateRect(ref, degrees));
  gfx::Rect bottom_rect(CoordinateRotateRect(rect, degrees));
  if (bottom_rect.right() < top_rect.x())
    return (bottom_rect.top_right() - top_rect.bottom_left()).LengthSquared();
  else if (top_rect.right() < bottom_rect.x())
    return (bottom_rect.origin() - top_rect.bottom_right()).LengthSquared();

  int distance = bottom_rect.y() - top_rect.bottom();
  return distance * distance;
}

}  // namespace win
}  // namespace display
