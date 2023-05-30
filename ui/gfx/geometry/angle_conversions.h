// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_ANGLE_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_ANGLE_CONVERSIONS_H_

#include "base/numerics/math_constants.h"
#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

GEOMETRY_EXPORT constexpr double DegToRad(double deg) {
  return deg * base::kDegToRadDouble;
}
GEOMETRY_EXPORT constexpr float DegToRad(float deg) {
  return deg * base::kDegToRadFloat;
}

GEOMETRY_EXPORT constexpr double RadToDeg(double rad) {
  return rad * base::kRadToDegDouble;
}
GEOMETRY_EXPORT constexpr float RadToDeg(float rad) {
  return rad * base::kRadToDegFloat;
}

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_ANGLE_CONVERSIONS_H_
