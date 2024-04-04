// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_THREE_POINT_CUBIC_BEZIER_H_
#define UI_GFX_GEOMETRY_THREE_POINT_CUBIC_BEZIER_H_

#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

class GEOMETRY_EXPORT ThreePointCubicBezier {
 public:
  // Creates a curve composed of two cubic curves sharing a common midpoint. The
  // curve runs through the (0,0), the midpoint, and (1,1). |p1x|, |p1y|, |p2x|,
  // and |p2y| are the coordinates of the control points for the first curve and
  // |p3x|, |p3y|, |p4x|, and |p4y| are the coordinates of the control points
  // for the second curve.
  ThreePointCubicBezier(double p1x,
                        double p1y,
                        double p2x,
                        double p2y,
                        double midpointx,
                        double midpointy,
                        double p3x,
                        double p3y,
                        double p4x,
                        double p4y);
  ThreePointCubicBezier(const ThreePointCubicBezier& other);

  ThreePointCubicBezier& operator=(const ThreePointCubicBezier&) = delete;

  // Evaluates y at the given x.
  double Solve(double x) const;

 private:
  CubicBezier first_curve_;
  CubicBezier second_curve_;

  double midpointx_;
  double midpointy_;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_THREE_POINT_CUBIC_BEZIER_H_
