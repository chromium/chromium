// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point.h"

#include <stddef.h>
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(PointTest, IsOrigin) {
  EXPECT_FALSE(Point(1, 0).IsOrigin());
  EXPECT_FALSE(Point(0, 1).IsOrigin());
  EXPECT_FALSE(Point(1, 2).IsOrigin());
  EXPECT_FALSE(Point(-1, 0).IsOrigin());
  EXPECT_FALSE(Point(0, -1).IsOrigin());
  EXPECT_FALSE(Point(-1, -2).IsOrigin());
  EXPECT_TRUE(Point(0, 0).IsOrigin());
}

TEST(PointTest, VectorArithmetic) {
  Point a(1, 5);
  Vector2d v1(3, -3);
  Vector2d v2(-8, 1);

  static const struct {
    Point expected;
    Point actual;
  } tests[] = {
    { Point(4, 2), a + v1 },
    { Point(-2, 8), a - v1 },
    { a, a - v1 + v1 },
    { a, a + v1 - v1 },
    { a, a + Vector2d() },
    { Point(12, 1), a + v1 - v2 },
    { Point(-10, 9), a - v1 + v2 }
  };

  for (auto& test : tests)
    EXPECT_EQ(test.expected, test.actual);
}

TEST(PointTest, OffsetFromPoint) {
  Point a(1, 5);
  Point b(-20, 8);
  EXPECT_EQ(Vector2d(-20 - 1, 8 - 5), (b - a));
}

TEST(PointTest, SetToMinMax) {
  Point a;

  a = Point(3, 5);
  EXPECT_EQ(Point(3, 5), a);
  a.SetToMax(Point(2, 4));
  EXPECT_EQ(Point(3, 5), a);
  a.SetToMax(Point(3, 5));
  EXPECT_EQ(Point(3, 5), a);
  a.SetToMax(Point(4, 2));
  EXPECT_EQ(Point(4, 5), a);
  a.SetToMax(Point(8, 10));
  EXPECT_EQ(Point(8, 10), a);

  a.SetToMin(Point(9, 11));
  EXPECT_EQ(Point(8, 10), a);
  a.SetToMin(Point(8, 10));
  EXPECT_EQ(Point(8, 10), a);
  a.SetToMin(Point(11, 9));
  EXPECT_EQ(Point(8, 9), a);
  a.SetToMin(Point(7, 11));
  EXPECT_EQ(Point(7, 9), a);
  a.SetToMin(Point(3, 5));
  EXPECT_EQ(Point(3, 5), a);
}

TEST(PointTest, Offset) {
  Point test(3, 4);
  test.Offset(5, -8);
  EXPECT_EQ(test, Point(8, -4));
}

TEST(PointTest, VectorMath) {
  Point test = Point(3, 4);
  test += Vector2d(5, -8);
  EXPECT_EQ(test, Point(8, -4));

  Point test2 = Point(3, 4);
  test2 -= Vector2d(5, -8);
  EXPECT_EQ(test2, Point(-2, 12));
}

TEST(PointTest, IntegerOverflow) {
  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  Point max_point(int_max, int_max);
  Point min_point(int_min, int_min);
  Point test;

  test = Point();
  test.Offset(int_max, int_max);
  EXPECT_EQ(test, max_point);

  test = Point();
  test.Offset(int_min, int_min);
  EXPECT_EQ(test, min_point);

  test = Point(10, 20);
  test.Offset(int_max, int_max);
  EXPECT_EQ(test, max_point);

  test = Point(-10, -20);
  test.Offset(int_min, int_min);
  EXPECT_EQ(test, min_point);

  test = Point();
  test += Vector2d(int_max, int_max);
  EXPECT_EQ(test, max_point);

  test = Point();
  test += Vector2d(int_min, int_min);
  EXPECT_EQ(test, min_point);

  test = Point(10, 20);
  test += Vector2d(int_max, int_max);
  EXPECT_EQ(test, max_point);

  test = Point(-10, -20);
  test += Vector2d(int_min, int_min);
  EXPECT_EQ(test, min_point);

  test = Point();
  test -= Vector2d(int_max, int_max);
  EXPECT_EQ(test, Point(-int_max, -int_max));

  test = Point();
  test -= Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_point);

  test = Point(10, 20);
  test -= Vector2d(int_min, int_min);
  EXPECT_EQ(test, max_point);

  test = Point(-10, -20);
  test -= Vector2d(int_max, int_max);
  EXPECT_EQ(test, min_point);
}

TEST(PointTest, Transpose) {
  gfx::Point p(1, -2);
  EXPECT_EQ(gfx::Point(-2, 1), TransposePoint(p));
  p.Transpose();
  EXPECT_EQ(gfx::Point(-2, 1), p);
}

}  // namespace gfx
