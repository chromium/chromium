// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/triangle_f.h"

#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

bool PointIsInTriangle(const PointF& point,
                       const PointF& r1,
                       const PointF& r2,
                       const PointF& r3) {
  // Compute the barycentric coordinates (u, v, w) of |point| relative to the
  // triangle (r1, r2, r3) by the solving the system of equations:
  //   1) point = u * r1 + v * r2 + w * r3
  //   2) u + v + w = 1
  // This algorithm comes from Christer Ericson's Real-Time Collision Detection.

  Vector2dF r31 = r1 - r3;
  Vector2dF r32 = r2 - r3;
  Vector2dF r3p = point - r3;

  // Promote to doubles so all the math below is done with doubles, because
  // otherwise it gets incorrect results on arm64.
  double r31x = r31.x();
  double r31y = r31.y();
  double r32x = r32.x();
  double r32y = r32.y();

  double denom = r32y * r31x - r32x * r31y;
  double u = (r32y * r3p.x() - r32x * r3p.y()) / denom;
  double v = (r31x * r3p.y() - r31y * r3p.x()) / denom;
  double w = 1.0 - u - v;

  // Use the barycentric coordinates to test if |point| is inside the
  // triangle (r1, r2, r2).
  return (u >= 0) && (v >= 0) && (w >= 0);
}

}  // namespace gfx