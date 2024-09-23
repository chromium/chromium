// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/vector3d_f.h"

#include <stddef.h>

#include <cmath>
#include <limits>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(Vector3dFTest, IsZero) {
  gfx::Vector3dF float_zero(0, 0, 0);
  gfx::Vector3dF float_nonzero(0.1f, -0.1f, 0.1f);

  EXPECT_TRUE(float_zero.IsZero());
  EXPECT_FALSE(float_nonzero.IsZero());
}

TEST(Vector3dFTest, Add) {
  gfx::Vector3dF f1(3.1f, 5.1f, 2.7f);
  gfx::Vector3dF f2(4.3f, -1.3f, 8.1f);

  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF actual;
  } float_tests[] = {
    { gfx::Vector3dF(3.1F, 5.1F, 2.7f), f1 + gfx::Vector3dF() },
    { gfx::Vector3dF(3.1f + 4.3f, 5.1f - 1.3f, 2.7f + 8.1f), f1 + f2 },
    { gfx::Vector3dF(3.1f - 4.3f, 5.1f + 1.3f, 2.7f - 8.1f), f1 - f2 }
  };

  for (size_t i = 0; i < std::size(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector3dFTest, Negative) {
  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF actual;
  } float_tests[] = {
    { gfx::Vector3dF(-0.0f, -0.0f, -0.0f), -gfx::Vector3dF(0, 0, 0) },
    { gfx::Vector3dF(-0.3f, -0.3f, -0.3f), -gfx::Vector3dF(0.3f, 0.3f, 0.3f) },
    { gfx::Vector3dF(0.3f, 0.3f, 0.3f), -gfx::Vector3dF(-0.3f, -0.3f, -0.3f) },
    { gfx::Vector3dF(-0.3f, 0.3f, -0.3f), -gfx::Vector3dF(0.3f, -0.3f, 0.3f) },
    { gfx::Vector3dF(0.3f, -0.3f, -0.3f), -gfx::Vector3dF(-0.3f, 0.3f, 0.3f) },
    { gfx::Vector3dF(-0.3f, -0.3f, 0.3f), -gfx::Vector3dF(0.3f, 0.3f, -0.3f) }
  };

  for (size_t i = 0; i < std::size(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector3dFTest, Scale) {
  float triple_values[][6] = {
    { 4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, -1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, -1.2f -1.8f, 3.3f, 5.6f, 4.2f },

    { 4.5f, 1.2f, 1.8f, 3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, 1.8f, -3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, -1.8f, 3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, -1.8f, -3.3f, -5.6f, -4.2f },

    { -4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, 1.8f, 0, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 0, 5.6f, 4.2f },

    { -4.5f, 1.2f, 1.8f, 3.3f, 0, 4.2f },
    { 4.5f, 0, 1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 3.3f, 0, 4.2f },
    { 4.5f, 0, -1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 0 },
    { -4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 0 },

    { 0, 1.2f, 0, 3.3f, 5.6f, 4.2f },
    { 0, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f }
  };

  for (size_t i = 0; i < std::size(triple_values); ++i) {
    gfx::Vector3dF v(triple_values[i][0],
                     triple_values[i][1],
                     triple_values[i][2]);
    v.Scale(triple_values[i][3], triple_values[i][4], triple_values[i][5]);
    EXPECT_EQ(triple_values[i][0] * triple_values[i][3], v.x());
    EXPECT_EQ(triple_values[i][1] * triple_values[i][4], v.y());
    EXPECT_EQ(triple_values[i][2] * triple_values[i][5], v.z());

    Vector3dF v2 = ScaleVector3d(
        gfx::Vector3dF(triple_values[i][0],
                       triple_values[i][1],
                       triple_values[i][2]),
        triple_values[i][3], triple_values[i][4], triple_values[i][5]);
    EXPECT_EQ(triple_values[i][0] * triple_values[i][3], v2.x());
    EXPECT_EQ(triple_values[i][1] * triple_values[i][4], v2.y());
    EXPECT_EQ(triple_values[i][2] * triple_values[i][5], v2.z());
  }

  float single_values[][4] = {
    { 4.5f, 1.2f, 1.8f, 3.3f },
    { 4.5f, -1.2f, 1.8f, 3.3f },
    { 4.5f, 1.2f, -1.8f, 3.3f },
    { 4.5f, -1.2f, -1.8f, 3.3f },
    { -4.5f, 1.2f, 3.3f },
    { -4.5f, 1.2f, 0 },
    { -4.5f, 1.2f, 1.8f, 3.3f },
    { -4.5f, 1.2f, 1.8f, 0 },
    { 4.5f, 0, 1.8f, 3.3f },
    { 0, 1.2f, 1.8f, 3.3f },
    { 4.5f, 0, 1.8f, 3.3f },
    { 0, 1.2f, 1.8f, 3.3f },
    { 4.5f, 1.2f, 0, 3.3f },
    { 4.5f, 1.2f, 0, 3.3f }
  };

  for (size_t i = 0; i < std::size(single_values); ++i) {
    gfx::Vector3dF v(single_values[i][0],
                     single_values[i][1],
                     single_values[i][2]);
    v.Scale(single_values[i][3]);
    EXPECT_EQ(single_values[i][0] * single_values[i][3], v.x());
    EXPECT_EQ(single_values[i][1] * single_values[i][3], v.y());
    EXPECT_EQ(single_values[i][2] * single_values[i][3], v.z());

    Vector3dF v2 = ScaleVector3d(
        gfx::Vector3dF(single_values[i][0],
                       single_values[i][1],
                       single_values[i][2]),
        single_values[i][3]);
    EXPECT_EQ(single_values[i][0] * single_values[i][3], v2.x());
    EXPECT_EQ(single_values[i][1] * single_values[i][3], v2.y());
    EXPECT_EQ(single_values[i][2] * single_values[i][3], v2.z());
  }
}

TEST(Vector3dFTest, Length) {
  float float_values[][3] = {
    { 0, 0, 0 },
    { 10.5f, 20.5f, 8.5f },
    { 20.5f, 10.5f, 8.5f },
    { 8.5f, 20.5f, 10.5f },
    { 10.5f, 8.5f, 20.5f },
    { -10.5f, -20.5f, -8.5f },
    { -20.5f, 10.5f, -8.5f },
    { -8.5f, -20.5f, -10.5f },
    { -10.5f, -8.5f, -20.5f },
    { 10.5f, -20.5f, 8.5f },
    { -10.5f, 20.5f, 8.5f },
    { 10.5f, -20.5f, -8.5f },
    { -10.5f, 20.5f, -8.5f },
    // A large vector that fails if the Length function doesn't use
    // double precision internally.
    { 1236278317862780234892374893213178027.12122348904204230f,
      335890352589839028212313231225425134332.38123f,
      27861786423846742743236423478236784678.236713617231f }
  };

  for (size_t i = 0; i < std::size(float_values); ++i) {
    double v0 = float_values[i][0];
    double v1 = float_values[i][1];
    double v2 = float_values[i][2];
    double length_squared =
        static_cast<double>(v0) * v0 +
        static_cast<double>(v1) * v1 +
        static_cast<double>(v2) * v2;
    double length = std::sqrt(length_squared);
    gfx::Vector3dF vector(v0, v1, v2);
    EXPECT_DOUBLE_EQ(length_squared, vector.LengthSquared());
    EXPECT_FLOAT_EQ(static_cast<float>(length), vector.Length());
  }
}

TEST(Vector3dFTest, DotProduct) {
  const struct {
    float expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {
    { 0, gfx::Vector3dF(1, 0, 0), gfx::Vector3dF(0, 1, 1) },
    { 0, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(1, 0, 1) },
    { 0, gfx::Vector3dF(0, 0, 1), gfx::Vector3dF(1, 1, 0) },

    { 3, gfx::Vector3dF(1, 1, 1), gfx::Vector3dF(1, 1, 1) },

    { 1.2f, gfx::Vector3dF(1.2f, -1.2f, 1.2f), gfx::Vector3dF(1, 1, 1) },
    { 1.2f, gfx::Vector3dF(1, 1, 1), gfx::Vector3dF(1.2f, -1.2f, 1.2f) },

    { 38.72f,
      gfx::Vector3dF(1.1f, 2.2f, 3.3f), gfx::Vector3dF(4.4f, 5.5f, 6.6f) }
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    float actual = gfx::DotProduct(tests[i].input1, tests[i].input2);
    EXPECT_EQ(tests[i].expected, actual);
  }
}

TEST(Vector3dFTest, CrossProduct) {
  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {
    { Vector3dF(), Vector3dF(), Vector3dF(1, 1, 1) },
    { Vector3dF(), Vector3dF(1, 1, 1), Vector3dF() },
    { Vector3dF(), Vector3dF(1, 1, 1), Vector3dF(1, 1, 1) },
    { Vector3dF(),
      Vector3dF(1.6f, 10.6f, -10.6f),
      Vector3dF(1.6f, 10.6f, -10.6f) },

    { Vector3dF(1, -1, 0), Vector3dF(1, 1, 1), Vector3dF(0, 0, 1) },
    { Vector3dF(-1, 0, 1), Vector3dF(1, 1, 1), Vector3dF(0, 1, 0) },
    { Vector3dF(0, 1, -1), Vector3dF(1, 1, 1), Vector3dF(1, 0, 0) },

    { Vector3dF(-1, 1, 0), Vector3dF(0, 0, 1), Vector3dF(1, 1, 1) },
    { Vector3dF(1, 0, -1), Vector3dF(0, 1, 0), Vector3dF(1, 1, 1) },
    { Vector3dF(0, -1, 1), Vector3dF(1, 0, 0), Vector3dF(1, 1, 1) }
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    SCOPED_TRACE(i);
    Vector3dF actual = gfx::CrossProduct(tests[i].input1, tests[i].input2);
    EXPECT_EQ(tests[i].expected.ToString(), actual.ToString());
  }
}

TEST(Vector3dFTest, ClampVector3dF) {
  Vector3dF a;

  a = Vector3dF(3.5f, 5.5f, 7.5f);
  EXPECT_EQ(Vector3dF(3.5f, 5.5f, 7.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(2, 4.5f, 6.5f));
  EXPECT_EQ(Vector3dF(3.5f, 5.5f, 7.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(3.5f, 5.5f, 7.5f));
  EXPECT_EQ(Vector3dF(3.5f, 5.5f, 7.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(4.5f, 2, 6.5f));
  EXPECT_EQ(Vector3dF(4.5f, 5.5f, 7.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(3.5f, 6.5f, 6.5f));
  EXPECT_EQ(Vector3dF(4.5f, 6.5f, 7.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(3.5f, 5.5f, 8.5f));
  EXPECT_EQ(Vector3dF(4.5f, 6.5f, 8.5f).ToString(), a.ToString());
  a.SetToMax(Vector3dF(8.5f, 10.5f, 12.5f));
  EXPECT_EQ(Vector3dF(8.5f, 10.5f, 12.5f).ToString(), a.ToString());

  a.SetToMin(Vector3dF(9.5f, 11.5f, 13.5f));
  EXPECT_EQ(Vector3dF(8.5f, 10.5f, 12.5f).ToString(), a.ToString());
  a.SetToMin(Vector3dF(8.5f, 10.5f, 12.5f));
  EXPECT_EQ(Vector3dF(8.5f, 10.5f, 12.5f).ToString(), a.ToString());
  a.SetToMin(Vector3dF(7.5f, 11.5f, 13.5f));
  EXPECT_EQ(Vector3dF(7.5f, 10.5f, 12.5f).ToString(), a.ToString());
  a.SetToMin(Vector3dF(9.5f, 9.5f, 13.5f));
  EXPECT_EQ(Vector3dF(7.5f, 9.5f, 12.5f).ToString(), a.ToString());
  a.SetToMin(Vector3dF(9.5f, 11.5f, 11.5f));
  EXPECT_EQ(Vector3dF(7.5f, 9.5f, 11.5f).ToString(), a.ToString());
  a.SetToMin(Vector3dF(3.5f, 5.5f, 7.5f));
  EXPECT_EQ(Vector3dF(3.5f, 5.5f, 7.5f).ToString(), a.ToString());
}

TEST(Vector3dFTest, AngleBetweenVectorsInDegress) {
  const struct {
    float expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {{0, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, 1, 0)},
               {90, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, 0, 1)},
               {45, gfx::Vector3dF(0, 1, 0),
                gfx::Vector3dF(0, 0.70710678188f, 0.70710678188f)},
               {180, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, -1, 0)},
               // Two vectors that are sufficiently close enough together to
               // trigger an issue that produces NANs if the value passed to
               // acos is not clamped due to floating point precision.
               {0, gfx::Vector3dF(0, -0.990842f, -0.003177f),
                gfx::Vector3dF(0, -0.999995f, -0.003124f)}};

  for (size_t i = 0; i < std::size(tests); ++i) {
    float actual =
        gfx::AngleBetweenVectorsInDegrees(tests[i].input1, tests[i].input2);
    EXPECT_FLOAT_EQ(tests[i].expected, actual);
    actual =
        gfx::AngleBetweenVectorsInDegrees(tests[i].input2, tests[i].input1);
    EXPECT_FLOAT_EQ(tests[i].expected, actual);
  }
}

TEST(Vector3dFTest, ClockwiseAngleBetweenVectorsInDegress) {
  const struct {
    float expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {
      {0, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, 1, 0)},
      {90, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, 0, -1)},
      {45,
       gfx::Vector3dF(0, -1, 0),
       gfx::Vector3dF(0, -0.70710678188f, 0.70710678188f)},
      {180, gfx::Vector3dF(0, -1, 0), gfx::Vector3dF(0, 1, 0)},
      {270, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(0, 0, 1)},
  };

  const gfx::Vector3dF normal_vector(1.0f, 0.0f, 0.0f);

  for (size_t i = 0; i < std::size(tests); ++i) {
    float actual = gfx::ClockwiseAngleBetweenVectorsInDegrees(
        tests[i].input1, tests[i].input2, normal_vector);
    EXPECT_FLOAT_EQ(tests[i].expected, actual);
    actual = -gfx::ClockwiseAngleBetweenVectorsInDegrees(
                 tests[i].input2, tests[i].input1, normal_vector);
    if (actual < 0.0f)
      actual += 360.0f;
    EXPECT_FLOAT_EQ(tests[i].expected, actual);
  }
}

TEST(Vector3dFTest, GetNormalized) {
  const struct {
    bool expected;
    gfx::Vector3dF v;
    gfx::Vector3dF normalized;
  } tests[] = {
      {false, gfx::Vector3dF(0, 0, 0), gfx::Vector3dF(0, 0, 0)},
      {false,
       gfx::Vector3dF(std::numeric_limits<float>::min(),
                      std::numeric_limits<float>::min(),
                      std::numeric_limits<float>::min()),
       gfx::Vector3dF(std::numeric_limits<float>::min(),
                      std::numeric_limits<float>::min(),
                      std::numeric_limits<float>::min())},
      {true, gfx::Vector3dF(1, 0, 0), gfx::Vector3dF(1, 0, 0)},
      {true, gfx::Vector3dF(std::numeric_limits<float>::max(), 0, 0),
       gfx::Vector3dF(1, 0, 0)},
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    gfx::Vector3dF n;
    EXPECT_EQ(tests[i].expected, tests[i].v.GetNormalized(&n));
    EXPECT_EQ(tests[i].normalized.ToString(), n.ToString());
  }
}

TEST(Vector3dFTest, ToString) {
  EXPECT_EQ("[1.03125 2.5 -3]", Vector3dF(1.03125, 2.5, -3).ToString());
}

}  // namespace gfx
