// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/rect_based_targeting_utils.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {

bool UsePointBasedTargeting(const gfx::Rect& rect) {
  return rect.width() == 1 && rect.height() == 1;
}

float PercentCoveredBy(const gfx::Rect& rect_1, const gfx::Rect& rect_2) {
  gfx::Rect intersection(rect_1);
  intersection.Intersect(rect_2);
  int intersect_area = intersection.size().GetArea();
  int rect_1_area = rect_1.size().GetArea();
  return rect_1_area ? static_cast<float>(intersect_area) /
                           static_cast<float>(rect_1_area)
                     : 0;
}

int DistanceSquaredFromCenterToPoint(const gfx::Point& point,
                                     const gfx::Rect& rect) {
  gfx::Point center_point = rect.CenterPoint();
  int dx = center_point.x() - point.x();
  int dy = center_point.y() - point.y();
  return (dx * dx) + (dy * dy);
}

}  // namespace views
