// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_RECT_BASED_TARGETING_UTILS_H_
#define UI_VIEWS_RECT_BASED_TARGETING_UTILS_H_

#include "ui/views/views_export.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace views {

// Returns true if |rect| is 1x1.
VIEWS_EXPORT bool UsePointBasedTargeting(const gfx::Rect& rect);

// Returns the percentage of |rect_1|'s area that is covered by |rect_2|.
VIEWS_EXPORT float PercentCoveredBy(const gfx::Rect& rect_1,
                                    const gfx::Rect& rect_2);

// Returns the square of the distance from |point| to the center of |rect|.
VIEWS_EXPORT int DistanceSquaredFromCenterToPoint(const gfx::Point& point,
                                                  const gfx::Rect& rect);

}  // namespace views

#endif  // UI_VIEWS_RECT_BASED_TARGETING_UTILS_H_
