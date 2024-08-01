// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/vector2d.h"

#include <stddef.h>
#include <cmath>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(Vector2dTest, IsZero) {
  EXPECT_TRUE(Vector2d().IsZero());
  EXPECT_TRUE(Vector2d(0, 0).IsZero());
  EXPECT_FALSE(Vector2d(1, 0).IsZero());
  EXPECT_FALSE(Vector2d(0, -2).IsZero());
  EXPECT_FALSE(Vector2d(1, -2).IsZero());
}

TEST(Vector2dTest, Add) {
  Vector2d i1(3, 5);
  Vector2d i2(4, -1);
  EXPECT_EQ(Vector2d(3, 5), i1 + Vector2d());
  EXPECT_EQ(Vector2d(3 + 4, 5 - 1), i1 + i2);
  EXPECT_EQ(Vector2d(3 - 4, 5 + 1), i1 - i2);
}

TEST(Vector2dTest, Negative) {
  EXPECT_EQ(Vector2d(0, 0), -Vector2d(0, 0));
  EXPECT_EQ(Vector2d(-3, -3), -Vector2d(3, 3));
  EXPECT_EQ(Vector2d(3, 3), -Vector2d(-3, -3));
  EXPECT_EQ(Vector2d(-3, 3), -Vector2d(3, -3));
  EXPECT_EQ(Vector2d(3, -3), -Vector2d(-3, 3));
}

TEST(Vector2dTest, Length) {
  int values[][2] = {
      {0, 0}, {10, 20}, {20, 10}, {-10, -20}, {-20, 10}, {10, -20},
  };

  for (auto& value : values) {
    int v0 = value[0];
    int v1 = value[1];
    double length_squared =
        static_cast<double>(v0) * v0 + static_cast<double>(v1) * v1;
    double length = std::sqrt(length_squared);
    Vector2d vector(v0, v1);
    EXPECT_EQ(static_cast<float>(length_squared), vector.LengthSquared());
    EXPECT_EQ(static_cast<float>(length), vector.Length());
  }
}

TEST(Vector2dTest, SetToMinMax) {
  Vector2d a;

  a = Vector2d(3, 5);
  EXPECT_EQ(Vector2d(3, 5), a);
  a.SetToMax(Vector2d(2, 4));
  EXPECT_EQ(Vector2d(3, 5), a);
  a.SetToMax(Vector2d(3, 5));
  EXPECT_EQ(Vector2d(3, 5), a);
  a.SetToMax(Vector2d(4, 2));
  EXPECT_EQ(Vector2d(4, 5), a);
  a.SetToMax(Vector2d(8, 10));
  EXPECT_EQ(Vector2d(8, 10), a);

  a.SetToMin(Vector2d(9, 11));
  EXPECT_EQ(Vector2d(8, 10), a);
  a.SetToMin(Vector2d(8, 10));
  EXPECT_EQ(Vector2d(8, 10), a);
  a.SetToMin(Vector2d(11, 9));
  EXPECT_EQ(Vector2d(8, 9), a);
  a.SetToMin(Vector2d(7, 11));
  EXPECT_EQ(Vector2d(7, 9), a);
  a.SetToMin(Vector2d(3, 5));
  EXPECT_EQ(Vector2d(3, 5), a);
}

TEST(Vector2dTest, IntegerOverflow) {
  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  Vector2d max_vector(int_max, int_max);
  Vector2d min_vector(int_min, int_min);
  Vector2d test;

  test = Vector2d();
  test += Vector2d(int_max, int_max);
  EXPECT_EQ(test, max_vector);

  test = Vector2d();
  test += Vector2d(int_min, int_min);
  EXPECT_EQ(test, min_vector);

  test = Vector2d(10, 20);
  test += Vector2d(int_max, int_max);
  EXPECT_EQ(test, max_vector);

  test = Vector2d(-10, -20);
  test += Vector2d(int_min, int_min);
  EXPECT_EQ(test, min_vector);

  test = Vector2d();
  test -= Vector2d(int_max, int_max);
  EXPECT_EQ(test, Vector2d(-int_max, -int_max));

  test = Vector2d();
  test -= Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_vector);

  test = Vector2d(10, 20);
  test -= Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_vector);

  test = Vector2d(-10, -20);
  test -= Vector2d(int_max, int_max);
  EXPECT_EQ(test, min_vector);

  test = Vector2d();
  test -= Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_vector);

  test = -Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_vector);
}

TEST(Vector2dTest, Transpose) {
  gfx::Vector2d v(1, -2);
  EXPECT_EQ(gfx::Vector2d(-2, 1), TransposeVector2d(v));
  v.Transpose();
  EXPECT_EQ(gfx::Vector2d(-2, 1), v);
}

}  // namespace gfx
