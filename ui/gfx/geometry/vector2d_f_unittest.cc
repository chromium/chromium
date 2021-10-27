// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/vector2d_f.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

// Some Vector2dF unittests are in vector2d_unittest.cc sharing the same tests
// with Vector2d.

TEST(Vector2dFTest, Length) {
  constexpr float kFloatMax = std::numeric_limits<float>::max();
  EXPECT_FLOAT_EQ(0.f, Vector2dF(0, 0).Length());
  EXPECT_FLOAT_EQ(1.f, Vector2dF(1, 0).Length());
  EXPECT_FLOAT_EQ(1.414214f, Vector2dF(1, 1).Length());
  EXPECT_FLOAT_EQ(2.236068f, Vector2dF(-1, -2).Length());
  EXPECT_FLOAT_EQ(kFloatMax, Vector2dF(kFloatMax, 0).Length());
  EXPECT_FLOAT_EQ(kFloatMax, Vector2dF(kFloatMax, kFloatMax).Length());
}

TEST(Vector2dFTest, SlopeAngleRadians) {
  // The function is required to be very accurate, so we use a smaller
  // tolerance than EXPECT_FLOAT_EQ().
  constexpr float kTolerance = 1e-7f;
  constexpr float kPi = 3.1415927f;
  EXPECT_NEAR(0, Vector2dF(0, 0).SlopeAngleRadians(), kTolerance);
  EXPECT_NEAR(0, Vector2dF(1, 0).SlopeAngleRadians(), kTolerance);
  EXPECT_NEAR(kPi / 4, Vector2dF(1, 1).SlopeAngleRadians(), kTolerance);
  EXPECT_NEAR(kPi / 2, Vector2dF(0, 1).SlopeAngleRadians(), kTolerance);
  EXPECT_NEAR(kPi, Vector2dF(-50, 0).SlopeAngleRadians(), kTolerance);
  EXPECT_NEAR(-kPi * 3 / 4, Vector2dF(-50, -50).SlopeAngleRadians(),
              kTolerance);
  EXPECT_NEAR(-kPi / 4, Vector2dF(1, -1).SlopeAngleRadians(), kTolerance);
}

}  // namespace gfx
