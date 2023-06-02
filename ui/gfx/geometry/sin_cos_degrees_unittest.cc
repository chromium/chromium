// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // To get M_PI on Windows.

#include "ui/gfx/geometry/sin_cos_degrees.h"

#include <math.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

TEST(SinCosDegreesTest, ExactValues) {
  for (int turn = -5 * 360; turn <= 5 * 360; turn += 360) {
    EXPECT_EQ(0.0, SinCosDegrees(turn + 0).sin);
    EXPECT_EQ(1.0, SinCosDegrees(turn + 0).cos);

    EXPECT_EQ(1.0, SinCosDegrees(turn + 90).sin);
    EXPECT_EQ(0.0, SinCosDegrees(turn + 90).cos);

    EXPECT_EQ(0.0, SinCosDegrees(turn + 180).sin);
    EXPECT_EQ(-1.0, SinCosDegrees(turn + 180).cos);

    EXPECT_EQ(-1.0, SinCosDegrees(turn + 270).sin);
    EXPECT_EQ(0.0, SinCosDegrees(turn + 270).cos);
  }
}

TEST(SinCosDegreesTest, CloseToLibc) {
  for (int d = -3600; d <= 3600; ++d) {
    double degrees = (d * 0.1);
    EXPECT_NEAR(sin(degrees * M_PI / 180.0), SinCosDegrees(degrees).sin, 1e-6);
    EXPECT_NEAR(cos(degrees * M_PI / 180.0), SinCosDegrees(degrees).cos, 1e-6);
  }
}

TEST(SinCosDegreesTest, AccurateRangeReduction) {
  EXPECT_EQ(SinCosDegrees(90000123).sin, SinCosDegrees(90000123).sin);
  EXPECT_EQ(SinCosDegrees(90000123).cos, SinCosDegrees(90000123).cos);

  EXPECT_EQ(SinCosDegrees(90e5).sin, 0.0);
  EXPECT_EQ(SinCosDegrees(90e5).cos, 1.0);
}

TEST(SinCosDegreesTest, HugeValues) {
  EXPECT_NEAR(SinCosDegrees(360e10 + 20).sin, sin(20 * (M_PI / 180.0)), 1e-6);
  EXPECT_NEAR(SinCosDegrees(360e10 + 20).cos, cos(20 * (M_PI / 180.0)), 1e-6);
}

}  // namespace
}  // namespace gfx
