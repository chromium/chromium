// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector2d_conversions.h"

#include "base/numerics/safe_conversions.h"

namespace gfx {

Vector2d ToFlooredVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::ClampFloor(vector2d.x()),
                  base::ClampFloor(vector2d.y()));
}

Vector2d ToCeiledVector2d(const Vector2dF& vector2d) {
  return Vector2d(base::ClampCeil(vector2d.x()), base::ClampCeil(vector2d.y()));
}

Vector2d ToRoundedVector2d(const Vector2dF& vector2d) {
  // Use floor(x + 0.5) instead of std::round(x). This not only aligns with
  // Blink's LayoutUnit::Round(), but also ensures no rounding direction
  // difference when elements are positioned at negative offsets.
  return Vector2d(base::ClampFloor(vector2d.x() + 0.5f),
                  base::ClampFloor(vector2d.y() + 0.5f));
}

}  // namespace gfx

