// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/three_point_cubic_bezier.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

TEST(ThreePointCubicBezierTest, Basic) {
  ThreePointCubicBezier function(0.125, 0.0, 0.375, 0.5, 0.5, 0.5, 0.625, 0.5,
                                 0.875, 1);

  double epsilon = 0.00015;

  EXPECT_NEAR(function.Solve(0), 0, epsilon);
  EXPECT_NEAR(function.Solve(0.05), 0.01989, epsilon);
  EXPECT_NEAR(function.Solve(0.1), 0.06402, epsilon);
  EXPECT_NEAR(function.Solve(0.15), 0.12058, epsilon);
  EXPECT_NEAR(function.Solve(0.2), 0.18381, epsilon);
  EXPECT_NEAR(function.Solve(0.25), 0.25, epsilon);
  EXPECT_NEAR(function.Solve(0.3), 0.31619, epsilon);
  EXPECT_NEAR(function.Solve(0.35), 0.37942, epsilon);
  EXPECT_NEAR(function.Solve(0.4), 0.43598, epsilon);
  EXPECT_NEAR(function.Solve(0.45), 0.48011, epsilon);
  EXPECT_NEAR(function.Solve(0.5), 0.5, epsilon);
  EXPECT_NEAR(function.Solve(0.55), 0.51989, epsilon);
  EXPECT_NEAR(function.Solve(0.6), 0.56402, epsilon);
  EXPECT_NEAR(function.Solve(0.65), 0.62058, epsilon);
  EXPECT_NEAR(function.Solve(0.7), 0.68381, epsilon);
  EXPECT_NEAR(function.Solve(0.75), 0.75, epsilon);
  EXPECT_NEAR(function.Solve(0.8), 0.81619, epsilon);
  EXPECT_NEAR(function.Solve(0.85), 0.87942, epsilon);
  EXPECT_NEAR(function.Solve(0.9), 0.93598, epsilon);
  EXPECT_NEAR(function.Solve(0.95), 0.98001, epsilon);
  EXPECT_NEAR(function.Solve(1), 1, epsilon);
}

}  // namespace
}  // namespace gfx
