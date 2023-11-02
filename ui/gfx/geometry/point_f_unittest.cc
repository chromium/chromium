// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point_f.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace gfx {

TEST(PointFTest, PointToPointF) {
  // Check that explicit conversion from integer to float compiles.
  Point a(10, 20);
  PointF b = PointF(a);

  EXPECT_EQ(static_cast<float>(a.x()), b.x());
  EXPECT_EQ(static_cast<float>(a.y()), b.y());
}

TEST(PointFTest, IsOrigin) {
  EXPECT_FALSE(PointF(0.1f, 0).IsOrigin());
  EXPECT_FALSE(PointF(0, 0.1f).IsOrigin());
  EXPECT_FALSE(PointF(0.1f, 2).IsOrigin());
  EXPECT_FALSE(PointF(-0.1f, 0).IsOrigin());
  EXPECT_FALSE(PointF(0, -0.1f).IsOrigin());
  EXPECT_FALSE(PointF(-0.1f, -2).IsOrigin());
  EXPECT_TRUE(PointF(0, 0).IsOrigin());
}

TEST(PointFTest, ToRoundedPoint) {
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0, 0)));
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0.0001f, 0.0001f)));
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0.4999f, 0.4999f)));
  EXPECT_EQ(Point(1, 1), ToRoundedPoint(PointF(0.5f, 0.5f)));
  EXPECT_EQ(Point(1, 1), ToRoundedPoint(PointF(0.9999f, 0.9999f)));

  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10, 10)));
  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10.0001f, 10.0001f)));
  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10.4999f, 10.4999f)));
  EXPECT_EQ(Point(11, 11), ToRoundedPoint(PointF(10.5f, 10.5f)));
  EXPECT_EQ(Point(11, 11), ToRoundedPoint(PointF(10.9999f, 10.9999f)));

  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10, -10)));
  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10.0001f, -10.0001f)));
  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10.4999f, -10.4999f)));
  EXPECT_EQ(Point(-11, -11), ToRoundedPoint(PointF(-10.5f, -10.5f)));
  EXPECT_EQ(Point(-11, -11), ToRoundedPoint(PointF(-10.9999f, -10.9999f)));
}

TEST(PointFTest, Scale) {
  EXPECT_EQ(PointF(2, -2), ScalePoint(PointF(1, -1), 2));
  EXPECT_EQ(PointF(2, -2), ScalePoint(PointF(1, -1), 2, 2));

  PointF zero;
  PointF one(1, -1);

  zero.Scale(2);
  zero.Scale(3, 1.5);

  one.Scale(2);
  one.Scale(3, 1.5);

  EXPECT_EQ(PointF(), zero);
  EXPECT_EQ(PointF(6, -3), one);
}

TEST(PointFTest, SetToMinMax) {
  PointF a;

  a = PointF(3.5f, 5.5f);
  EXPECT_EQ(PointF(3.5f, 5.5f), a);
  a.SetToMax(PointF(2.5f, 4.5f));
  EXPECT_EQ(PointF(3.5f, 5.5f), a);
  a.SetToMax(PointF(3.5f, 5.5f));
  EXPECT_EQ(PointF(3.5f, 5.5f), a);
  a.SetToMax(PointF(4.5f, 2.5f));
  EXPECT_EQ(PointF(4.5f, 5.5f), a);
  a.SetToMax(PointF(8.5f, 10.5f));
  EXPECT_EQ(PointF(8.5f, 10.5f), a);

  a.SetToMin(PointF(9.5f, 11.5f));
  EXPECT_EQ(PointF(8.5f, 10.5f), a);
  a.SetToMin(PointF(8.5f, 10.5f));
  EXPECT_EQ(PointF(8.5f, 10.5f), a);
  a.SetToMin(PointF(11.5f, 9.5f));
  EXPECT_EQ(PointF(8.5f, 9.5f), a);
  a.SetToMin(PointF(7.5f, 11.5f));
  EXPECT_EQ(PointF(7.5f, 9.5f), a);
  a.SetToMin(PointF(3.5f, 5.5f));
  EXPECT_EQ(PointF(3.5f, 5.5f), a);
}

TEST(PointFTest, IsWithinDistance) {
  PointF pt(10.f, 10.f);
  EXPECT_TRUE(pt.IsWithinDistance(PointF(10.f, 10.f), 0.0000000000001f));
  EXPECT_FALSE(pt.IsWithinDistance(PointF(8.f, 8.f), 1.f));

  pt = PointF(-10.f, -10.f);
  EXPECT_FALSE(
      pt.IsWithinDistance(PointF(10.f, 10.f), /*allowed_distance=*/10.f));
  EXPECT_TRUE(pt.IsWithinDistance(PointF(-9.9988f, -10.0013f), 0.0017689f));

  pt = PointF(std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max());
  EXPECT_FALSE(pt.IsWithinDistance(PointF(std::numeric_limits<float>::min(),
                                          std::numeric_limits<float>::min()),
                                   100.f));
}

TEST(PointFTest, Transpose) {
  gfx::PointF p(-1.5f, 2.5f);
  EXPECT_EQ(gfx::PointF(2.5f, -1.5f), TransposePoint(p));
  p.Transpose();
  EXPECT_EQ(gfx::PointF(2.5f, -1.5f), p);
}

TEST(PointFTest, ToString) {
  EXPECT_EQ("1,2", PointF(1, 2).ToString());
  EXPECT_EQ("1.03125,2.5", PointF(1.03125, 2.5).ToString());
}

}  // namespace gfx
