// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/three_point_cubic_bezier.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/check_op.h"

namespace gfx {

ThreePointCubicBezier::ThreePointCubicBezier(double p1x,
                                             double p1y,
                                             double p2x,
                                             double p2y,
                                             double midpointx,
                                             double midpointy,
                                             double p3x,
                                             double p3y,
                                             double p4x,
                                             double p4y)
    : first_curve_(p1x / midpointx,
                   p1y / midpointy,
                   p2x / midpointx,
                   p2y / midpointy),
      second_curve_((p3x - midpointx) / (1 - midpointx),
                    (p3y - midpointy) / (1 - midpointy),
                    (p4x - midpointx) / (1 - midpointx),
                    (p4y - midpointy) / (1 - midpointy)),
      midpointx_(midpointx),
      midpointy_(midpointy) {}

ThreePointCubicBezier::ThreePointCubicBezier(
    const ThreePointCubicBezier& other) = default;

double ThreePointCubicBezier::Solve(double x) const {
  const bool in_first_curve = x < midpointx_;
  const double scaled_x = (x - (in_first_curve ? 0.0 : midpointx_)) /
                          (in_first_curve ? midpointx_ : (1 - midpointx_));
  if (in_first_curve) {
    return first_curve_.Solve(scaled_x) * midpointy_;
  }
  return second_curve_.Solve(scaled_x) * (1 - midpointy_) + midpointy_;
}

}  // namespace gfx
