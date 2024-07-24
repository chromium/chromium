// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/vector2d_f.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

TEST(Vector2dTest, Vector2dToVector2dF) {
  Vector2d i(3, 4);
  Vector2dF f = i;
  EXPECT_EQ(i, f);
}

TEST(Vector2dFTest, IsZero) {
  EXPECT_TRUE(Vector2dF().IsZero());
  EXPECT_TRUE(Vector2dF(0, 0).IsZero());
  EXPECT_FALSE(Vector2dF(0.1f, 0).IsZero());
  EXPECT_FALSE(Vector2dF(0, -0.1f).IsZero());
  EXPECT_FALSE(Vector2dF(0.1f, -0.1f).IsZero());
}

TEST(Vector2dFTest, Add) {
  Vector2dF f1(3.1f, 5.1f);
  Vector2dF f2(4.3f, -1.3f);
  EXPECT_VECTOR2DF_EQ(Vector2dF(3.1f, 5.1f), f1 + Vector2dF());
  EXPECT_VECTOR2DF_EQ(Vector2dF(3.1f + 4.3f, 5.1f - 1.3f), f1 + f2);
  EXPECT_VECTOR2DF_EQ(Vector2dF(3.1f - 4.3f, 5.1f + 1.3f), f1 - f2);
}

TEST(Vector2dFTest, Negative) {
  EXPECT_VECTOR2DF_EQ(Vector2dF(), -Vector2dF());
  EXPECT_VECTOR2DF_EQ(Vector2dF(-0.3f, -0.3f), -Vector2dF(0.3f, 0.3f));
  EXPECT_VECTOR2DF_EQ(Vector2dF(0.3f, 0.3f), -Vector2dF(-0.3f, -0.3f));
  EXPECT_VECTOR2DF_EQ(Vector2dF(-0.3f, 0.3f), -Vector2dF(0.3f, -0.3f));
  EXPECT_VECTOR2DF_EQ(Vector2dF(0.3f, -0.3f), -Vector2dF(-0.3f, 0.3f));
}

TEST(Vector2dFTest, Scale) {
  float double_values[][4] = {
      {4.5f, 1.2f, 3.3f, 5.6f},  {4.5f, -1.2f, 3.3f, 5.6f},
      {4.5f, 1.2f, 3.3f, -5.6f}, {4.5f, 1.2f, -3.3f, -5.6f},
      {-4.5f, 1.2f, 3.3f, 5.6f}, {-4.5f, 1.2f, 0, 5.6f},
      {-4.5f, 1.2f, 3.3f, 0},    {4.5f, 0, 3.3f, 5.6f},
      {0, 1.2f, 3.3f, 5.6f}};

  for (auto& values : double_values) {
    Vector2dF v(values[0], values[1]);
    v.Scale(values[2], values[3]);
    EXPECT_EQ(v.x(), values[0] * values[2]);
    EXPECT_EQ(v.y(), values[1] * values[3]);

    Vector2dF v2 = ScaleVector2d(gfx::Vector2dF(values[0], values[1]),
                                 values[2], values[3]);
    EXPECT_EQ(values[0] * values[2], v2.x());
    EXPECT_EQ(values[1] * values[3], v2.y());
  }

  float single_values[][3] = {
      {4.5f, 1.2f, 3.3f},  {4.5f, -1.2f, 3.3f}, {4.5f, 1.2f, 3.3f},
      {4.5f, 1.2f, -3.3f}, {-4.5f, 1.2f, 3.3f}, {-4.5f, 1.2f, 0},
      {-4.5f, 1.2f, 3.3f}, {4.5f, 0, 3.3f},     {0, 1.2f, 3.3f}};

  for (auto& values : single_values) {
    Vector2dF v(values[0], values[1]);
    v.Scale(values[2]);
    EXPECT_EQ(v.x(), values[0] * values[2]);
    EXPECT_EQ(v.y(), values[1] * values[2]);

    Vector2dF v2 =
        ScaleVector2d(gfx::Vector2dF(values[0], values[1]), values[2]);
    EXPECT_EQ(values[0] * values[2], v2.x());
    EXPECT_EQ(values[1] * values[2], v2.y());
  }
}

TEST(Vector2dFTest, SetToMinMax) {
  Vector2dF a;

  a = Vector2dF(3.5f, 5.5f);
  EXPECT_EQ(Vector2dF(3.5f, 5.5f), a);
  a.SetToMax(Vector2dF(2.5f, 4.5f));
  EXPECT_EQ(Vector2dF(3.5f, 5.5f), a);
  a.SetToMax(Vector2dF(3.5f, 5.5f));
  EXPECT_EQ(Vector2dF(3.5f, 5.5f), a);
  a.SetToMax(Vector2dF(4.5f, 2.5f));
  EXPECT_EQ(Vector2dF(4.5f, 5.5f), a);
  a.SetToMax(Vector2dF(8.5f, 10.5f));
  EXPECT_EQ(Vector2dF(8.5f, 10.5f), a);

  a.SetToMin(Vector2dF(9.5f, 11.5f));
  EXPECT_EQ(Vector2dF(8.5f, 10.5f), a);
  a.SetToMin(Vector2dF(8.5f, 10.5f));
  EXPECT_EQ(Vector2dF(8.5f, 10.5f), a);
  a.SetToMin(Vector2dF(11.5f, 9.5f));
  EXPECT_EQ(Vector2dF(8.5f, 9.5f), a);
  a.SetToMin(Vector2dF(7.5f, 11.5f));
  EXPECT_EQ(Vector2dF(7.5f, 9.5f), a);
  a.SetToMin(Vector2dF(3.5f, 5.5f));
  EXPECT_EQ(Vector2dF(3.5f, 5.5f), a);
}

TEST(Vector2dFTest, Length) {
  constexpr float kFloatMax = std::numeric_limits<float>::max();
  EXPECT_FLOAT_EQ(0.f, Vector2dF(0, 0).Length());
  EXPECT_FLOAT_EQ(1.f, Vector2dF(1, 0).Length());
  EXPECT_FLOAT_EQ(1.414214f, Vector2dF(1, 1).Length());
  EXPECT_FLOAT_EQ(2.236068f, Vector2dF(-1, -2).Length());

  // The Pythagorean triples 3-4-5 and 5-12-13.
  EXPECT_FLOAT_EQ(5.f, Vector2dF(3.f, 4.f).Length());
  EXPECT_FLOAT_EQ(13.f, Vector2dF(5.f, 12.f).Length());

  // Very small numbers.
  EXPECT_FLOAT_EQ(.7071068e-20f, Vector2dF(.5e-20f, .5e-20f).Length());

  // Very large numbers.
  EXPECT_FLOAT_EQ(.7071068e20f, Vector2dF(.5e20f, .5e20f).Length());
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

TEST(Vector2dFTest, Transpose) {
  gfx::Vector2dF v(-1.5f, 2.5f);
  EXPECT_EQ(gfx::Vector2dF(2.5f, -1.5f), TransposeVector2d(v));
  v.Transpose();
  EXPECT_EQ(gfx::Vector2dF(2.5f, -1.5f), v);
}

TEST(Vector2dFTest, ToString) {
  EXPECT_EQ("[1 2]", Vector2dF(1, 2).ToString());
  EXPECT_EQ("[1.03125 2.5]", Vector2dF(1.03125, 2.5).ToString());
}

}  // namespace gfx
