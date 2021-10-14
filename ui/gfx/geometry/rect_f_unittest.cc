// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rect_f.h"

#include <cmath>

#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace gfx {

TEST(RectFTest, FromRect) {
  // Check that explicit conversion from integer to float compiles.
  Rect a(10, 20, 30, 40);
  RectF b(10, 20, 30, 40);

  RectF c = RectF(a);
  EXPECT_EQ(b, c);
}

TEST(RectFTest, BoundingRect) {
  // If point B dominates A, then A should be the origin.
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 0, 0),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(4.2f, 6.8f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 4.3f, 0),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(8.5f, 6.8f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 0, 2.5f),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(4.2f, 9.3f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 4.3f, 2.5f),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(8.5f, 9.3f)));
  // If point A dominates B, then B should be the origin.
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 0, 0),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(4.2f, 6.8f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 4.3f, 0),
                  BoundingRect(PointF(8.5f, 6.8f), PointF(4.2f, 6.8f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 0, 2.5f),
                  BoundingRect(PointF(4.2f, 9.3f), PointF(4.2f, 6.8f)));
  EXPECT_RECTF_EQ(RectF(4.2f, 6.8f, 4.3f, 2.5f),
                  BoundingRect(PointF(8.5f, 9.3f), PointF(4.2f, 6.8f)));
  // If neither point dominates, then the origin is a combination of the two.
  EXPECT_RECTF_EQ(RectF(4.2f, 4.2f, 2.6f, 2.6f),
                  BoundingRect(PointF(4.2f, 6.8f), PointF(6.8f, 4.2f)));
  EXPECT_RECTF_EQ(RectF(-6.8f, -6.8f, 2.6f, 2.6f),
                  BoundingRect(PointF(-4.2f, -6.8f), PointF(-6.8f, -4.2f)));
  EXPECT_RECTF_EQ(RectF(-4.2f, -4.2f, 11.0f, 11.0f),
                  BoundingRect(PointF(-4.2f, 6.8f), PointF(6.8f, -4.2f)));
}

TEST(RectFTest, CenterPoint) {
  PointF center;

  // When origin is (0, 0).
  center = RectF(0, 0, 20, 20).CenterPoint();
  EXPECT_TRUE(center == PointF(10, 10));

  // When origin is even.
  center = RectF(10, 10, 20, 20).CenterPoint();
  EXPECT_TRUE(center == PointF(20, 20));

  // When origin is odd.
  center = RectF(11, 11, 20, 20).CenterPoint();
  EXPECT_TRUE(center == PointF(21, 21));

  // When 0 width or height.
  center = RectF(10, 10, 0, 20).CenterPoint();
  EXPECT_TRUE(center == PointF(10, 20));
  center = RectF(10, 10, 20, 0).CenterPoint();
  EXPECT_TRUE(center == PointF(20, 10));

  // When an odd size.
  center = RectF(10, 10, 21, 21).CenterPoint();
  EXPECT_TRUE(center == PointF(20.5f, 20.5f));

  // When an odd size and position.
  center = RectF(11, 11, 21, 21).CenterPoint();
  EXPECT_TRUE(center == PointF(21.5f, 21.5f));
}

TEST(RectFTest, ScaleRect) {
  constexpr gfx::RectF input(3, 3, 3, 3);
  EXPECT_RECTF_EQ(gfx::RectF(4.5f, 4.5f, 4.5f, 4.5f), ScaleRect(input, 1.5f));
  EXPECT_RECTF_EQ(gfx::RectF(0, 0, 0, 0), ScaleRect(input, 0));

  constexpr float kMaxFloat = std::numeric_limits<float>::max();
  EXPECT_RECTF_EQ(gfx::RectF(kMaxFloat, kMaxFloat, kMaxFloat, kMaxFloat),
                  ScaleRect(input, kMaxFloat));

  gfx::RectF nan_rect =
      ScaleRect(input, std::numeric_limits<float>::quiet_NaN());
  EXPECT_TRUE(std::isnan(nan_rect.x()));
  EXPECT_TRUE(std::isnan(nan_rect.y()));
  // NaN is clamped to 0 in SizeF constructor.
  EXPECT_EQ(0, nan_rect.width());
  EXPECT_EQ(0, nan_rect.height());
}

TEST(RectFTest, IsExpressibleAsRect) {
  EXPECT_TRUE(RectF().IsExpressibleAsRect());

  constexpr float kMinIntF =
      static_cast<float>(std::numeric_limits<int>::min());
  constexpr float kMaxIntF =
      static_cast<float>(std::numeric_limits<int>::max());
  constexpr float kInfinity = std::numeric_limits<float>::infinity();

  EXPECT_TRUE(
      RectF(kMinIntF + 200, kMinIntF + 200, kMaxIntF - 200, kMaxIntF - 200)
          .IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(kMinIntF - 200, kMinIntF + 200, kMaxIntF + 200, kMaxIntF + 200)
          .IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(kMinIntF + 200, kMinIntF - 200, kMaxIntF + 200, kMaxIntF + 200)
          .IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(kMinIntF + 200, kMinIntF + 200, kMaxIntF + 200, kMaxIntF - 200)
          .IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(kMinIntF + 200, kMinIntF + 200, kMaxIntF - 200, kMaxIntF + 200)
          .IsExpressibleAsRect());

  EXPECT_TRUE(
      RectF(0, 0, kMaxIntF - 200, kMaxIntF - 200).IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(200, 0, kMaxIntF + 200, kMaxIntF - 200).IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(0, 200, kMaxIntF - 200, kMaxIntF + 200).IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(0, 0, kMaxIntF + 200, kMaxIntF - 200).IsExpressibleAsRect());
  EXPECT_FALSE(
      RectF(0, 0, kMaxIntF - 200, kMaxIntF + 200).IsExpressibleAsRect());

  EXPECT_FALSE(RectF(kInfinity, 0, 1, 1).IsExpressibleAsRect());
  EXPECT_FALSE(RectF(0, kInfinity, 1, 1).IsExpressibleAsRect());
  EXPECT_FALSE(RectF(0, 0, kInfinity, 1).IsExpressibleAsRect());
  EXPECT_FALSE(RectF(0, 0, 1, kInfinity).IsExpressibleAsRect());
}

TEST(RectFTest, Offset) {
  RectF f(1.1f, 2.2f, 3.3f, 4.4f);
  EXPECT_EQ(RectF(2.2f, 1.1f, 3.3f, 4.4f), (f + Vector2dF(1.1f, -1.1f)));
  EXPECT_EQ(RectF(2.2f, 1.1f, 3.3f, 4.4f), (Vector2dF(1.1f, -1.1f) + f));
  f += Vector2dF(1.1f, -1.1f);
  EXPECT_EQ(RectF(2.2f, 1.1f, 3.3f, 4.4f), f);
  EXPECT_EQ(RectF(1.1f, 2.2f, 3.3f, 4.4f), (f - Vector2dF(1.1f, -1.1f)));
  f -= Vector2dF(1.1f, -1.1f);
  EXPECT_EQ(RectF(1.1f, 2.2f, 3.3f, 4.4f), f);
}

TEST(RectFTest, Corners) {
  RectF f(1.1f, 2.1f, 3.1f, 4.1f);
  EXPECT_EQ(PointF(1.1f, 2.1f), f.origin());
  EXPECT_EQ(PointF(4.2f, 2.1f), f.top_right());
  EXPECT_EQ(PointF(1.1f, 6.2f), f.bottom_left());
  EXPECT_EQ(PointF(4.2f, 6.2f), f.bottom_right());
}

TEST(RectFTest, Centers) {
  RectF f(10.1f, 20.2f, 30.3f, 40.4f);
  EXPECT_EQ(PointF(10.1f, 40.4f), f.left_center());
  EXPECT_EQ(PointF(25.25f, 20.2f), f.top_center());
  EXPECT_EQ(PointF(40.4f, 40.4f), f.right_center());
  EXPECT_EQ(25.25f, f.bottom_center().x());
  EXPECT_NEAR(60.6f, f.bottom_center().y(), 0.001f);
}

TEST(RectFTest, Transpose) {
  RectF f(10.1f, 20.2f, 30.3f, 40.4f);
  f.Transpose();
  EXPECT_EQ(RectF(20.2f, 10.1f, 40.4f, 30.3f), f);
}

TEST(RectFTest, ManhattanDistanceToPoint) {
  RectF f(1.1f, 2.1f, 3.1f, 4.1f);
  EXPECT_FLOAT_EQ(0.f, f.ManhattanDistanceToPoint(PointF(1.1f, 2.1f)));
  EXPECT_FLOAT_EQ(0.f, f.ManhattanDistanceToPoint(PointF(4.2f, 6.f)));
  EXPECT_FLOAT_EQ(0.f, f.ManhattanDistanceToPoint(PointF(2.f, 4.f)));
  EXPECT_FLOAT_EQ(3.2f, f.ManhattanDistanceToPoint(PointF(0.f, 0.f)));
  EXPECT_FLOAT_EQ(2.1f, f.ManhattanDistanceToPoint(PointF(2.f, 0.f)));
  EXPECT_FLOAT_EQ(2.9f, f.ManhattanDistanceToPoint(PointF(5.f, 0.f)));
  EXPECT_FLOAT_EQ(.8f, f.ManhattanDistanceToPoint(PointF(5.f, 4.f)));
  EXPECT_FLOAT_EQ(2.6f, f.ManhattanDistanceToPoint(PointF(5.f, 8.f)));
  EXPECT_FLOAT_EQ(1.8f, f.ManhattanDistanceToPoint(PointF(3.f, 8.f)));
  EXPECT_FLOAT_EQ(1.9f, f.ManhattanDistanceToPoint(PointF(0.f, 7.f)));
  EXPECT_FLOAT_EQ(1.1f, f.ManhattanDistanceToPoint(PointF(0.f, 3.f)));
}

TEST(RectFTest, ManhattanInternalDistance) {
  RectF f(0.0f, 0.0f, 400.0f, 400.0f);
  static const float kEpsilon = std::numeric_limits<float>::epsilon();

  EXPECT_FLOAT_EQ(
      0.0f, f.ManhattanInternalDistance(gfx::RectF(-1.0f, 0.0f, 2.0f, 1.0f)));
  EXPECT_FLOAT_EQ(kEpsilon, f.ManhattanInternalDistance(
                                gfx::RectF(400.0f, 0.0f, 1.0f, 400.0f)));
  EXPECT_FLOAT_EQ(2.0f * kEpsilon, f.ManhattanInternalDistance(gfx::RectF(
                                       -100.0f, -100.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(1.0f + kEpsilon, f.ManhattanInternalDistance(gfx::RectF(
                                       -101.0f, 100.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(2.0f + 2.0f * kEpsilon,
                  f.ManhattanInternalDistance(
                      gfx::RectF(-101.0f, -101.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(
      433.0f + 2.0f * kEpsilon,
      f.ManhattanInternalDistance(gfx::RectF(630.0f, 603.0f, 100.0f, 100.0f)));

  EXPECT_FLOAT_EQ(
      0.0f, f.ManhattanInternalDistance(gfx::RectF(-1.0f, 0.0f, 1.1f, 1.0f)));
  EXPECT_FLOAT_EQ(0.1f + kEpsilon, f.ManhattanInternalDistance(
                                       gfx::RectF(-1.5f, 0.0f, 1.4f, 1.0f)));
  EXPECT_FLOAT_EQ(kEpsilon, f.ManhattanInternalDistance(
                                gfx::RectF(-1.5f, 0.0f, 1.5f, 1.0f)));
}

TEST(RectFTest, Inset) {
  RectF r(10, 20, 30, 40);
  r.Inset(0);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
  r.Inset(1.5);
  EXPECT_RECTF_EQ(RectF(11.5, 21.5, 27, 37), r);
  r.Inset(-1.5);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Inset(1.5, 2.25);
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 27, 35.5), r);
  r.Inset(-1.5, -2.25);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // The parameters are left, top, right, bottom.
  r.Inset(1.5, 2.25, 3.75, 4);
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 24.75, 33.75), r);
  r.Inset(-1.5, -2.25, -3.75, -4);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // InsetsF parameters are top, right, bottom, left.
  r.Inset(InsetsF(1.5, 2.25, 3.75, 4));
  EXPECT_RECTF_EQ(RectF(12.25, 21.5, 23.75, 34.75), r);
  r.Inset(InsetsF(-1.5, -2.25, -3.75, -4));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
}

TEST(RectFTest, Outset) {
  RectF r(10, 20, 30, 40);
  r.Outset(0);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
  r.Outset(1.5);
  EXPECT_RECTF_EQ(RectF(8.5, 18.5, 33, 43), r);
  r.Outset(-1.5);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Outset(1.5, 2.25);
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 33, 44.5), r);
  r.Outset(-1.5, -2.25);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Outset(1.5, 2.25, 3.75, 4);
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 35.25, 46.25), r);
  r.Outset(-1.5, -2.25, -3.75, -4);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
}

TEST(RectFTest, InsetClamped) {
  RectF r(10, 20, 30, 40);
  r.Inset(18);
  EXPECT_RECTF_EQ(RectF(28, 38, 0, 4), r);
  r.Inset(-18);
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 40), r);

  r.Inset(15, 30);
  EXPECT_RECTF_EQ(RectF(25, 50, 6, 0), r);
  r.Inset(-15, -30);
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 60), r);

  r.Inset(20, 30, 40, 50);
  EXPECT_RECTF_EQ(RectF(30, 50, 0, 0), r);
  r.Inset(-20, -30, -40, -50);
  EXPECT_RECTF_EQ(RectF(10, 20, 60, 80), r);
}

TEST(RectFTest, InclusiveIntersect) {
  RectF rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(RectF(11, 12, 13, 14)));
  EXPECT_RECTF_EQ(RectF(11, 12, 0, 0), rect);

  rect = RectF(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(RectF(24, 8, 0, 7)));
  EXPECT_RECTF_EQ(RectF(24, 12, 0, 3), rect);

  rect = RectF(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(RectF(9, 15, 4, 0)));
  EXPECT_RECTF_EQ(RectF(11, 15, 2, 0), rect);

  rect = RectF(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(RectF(12, 13, 15, 16)));
  EXPECT_RECTF_EQ(RectF(), rect);
}

TEST(RectFTest, MaximumCoveredRect) {
  // X aligned and intersect: unite.
  EXPECT_EQ(RectF(10, 20, 30, 60),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(10, 30, 30, 50)));
  // X aligned and adjacent: unite.
  EXPECT_EQ(RectF(10, 20, 30, 90),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(10, 60, 30, 50)));
  // X aligned and separate: choose the bigger one.
  EXPECT_EQ(RectF(10, 61, 30, 50),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(10, 61, 30, 50)));
  // Y aligned and intersect: unite.
  EXPECT_EQ(RectF(10, 20, 60, 40),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(30, 20, 40, 40)));
  // Y aligned and adjacent: unite.
  EXPECT_EQ(RectF(10, 20, 70, 40),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(40, 20, 40, 40)));
  // Y aligned and separate: choose the bigger one.
  EXPECT_EQ(RectF(41, 20, 40, 40),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(41, 20, 40, 40)));
  // Get the biggest expanded intersection.
  EXPECT_EQ(RectF(0, 0, 9, 19),
            MaximumCoveredRect(RectF(0, 0, 10, 10), RectF(0, 9, 9, 10)));
  EXPECT_EQ(RectF(0, 0, 19, 9),
            MaximumCoveredRect(RectF(0, 0, 10, 10), RectF(9, 0, 10, 9)));
  // Otherwise choose the bigger one.
  EXPECT_EQ(RectF(20, 30, 40, 50),
            MaximumCoveredRect(RectF(10, 20, 30, 40), RectF(20, 30, 40, 50)));
  EXPECT_EQ(RectF(10, 20, 40, 50),
            MaximumCoveredRect(RectF(10, 20, 40, 50), RectF(20, 30, 30, 40)));
  EXPECT_EQ(RectF(10, 20, 40, 50),
            MaximumCoveredRect(RectF(10, 20, 40, 50), RectF(20, 30, 40, 50)));
}

}  // namespace gfx
