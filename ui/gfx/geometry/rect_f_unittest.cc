// Copyright 2021 The Chromium Authors
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

TEST(RectFTest, ContainsPointF) {
  EXPECT_FALSE(RectF().Contains(PointF()));
  RectF r(10, 20, 30, 40);
  EXPECT_FALSE(r.Contains(PointF(0, 0)));
  EXPECT_FALSE(r.Contains(PointF(9.9999f, 20)));
  EXPECT_FALSE(r.Contains(PointF(10, 19.9999f)));
  EXPECT_TRUE(r.Contains(PointF(10, 20)));
  EXPECT_TRUE(r.Contains(PointF(39.9999f, 20)));
  EXPECT_FALSE(r.Contains(PointF(40, 20)));
  EXPECT_TRUE(r.Contains(PointF(10, 59.9999f)));
  EXPECT_FALSE(r.Contains(PointF(10, 60)));
  EXPECT_TRUE(r.Contains(PointF(39.9999f, 59.9999f)));
  EXPECT_FALSE(r.Contains(PointF(40, 60)));
  EXPECT_FALSE(r.Contains(PointF(100, 100)));
}

TEST(RectFTest, ContainsXY) {
  EXPECT_FALSE(RectF().Contains(0, 0));
  RectF r(10, 20, 30, 40);
  EXPECT_FALSE(r.Contains(0, 0));
  EXPECT_FALSE(r.Contains(9.9999f, 20));
  EXPECT_FALSE(r.Contains(10, 19.9999f));
  EXPECT_TRUE(r.Contains(10, 20));
  EXPECT_TRUE(r.Contains(39.9999f, 20));
  EXPECT_FALSE(r.Contains(40, 20));
  EXPECT_TRUE(r.Contains(10, 59.9999f));
  EXPECT_FALSE(r.Contains(10, 60));
  EXPECT_TRUE(r.Contains(39.9999f, 59.9999f));
  EXPECT_FALSE(r.Contains(40, 60));
  EXPECT_FALSE(r.Contains(100, 100));
}

TEST(RectFTest, InclusiveContainsPointF) {
  EXPECT_TRUE(RectF().InclusiveContains(PointF()));
  EXPECT_FALSE(RectF().InclusiveContains(PointF(0.0001f, 0)));
  RectF r(10, 20, 30, 40);
  EXPECT_FALSE(r.InclusiveContains(PointF(0, 0)));
  EXPECT_FALSE(r.InclusiveContains(PointF(9.9999f, 20)));
  EXPECT_FALSE(r.InclusiveContains(PointF(10, 19.9999f)));
  EXPECT_TRUE(r.InclusiveContains(PointF(10, 20)));
  EXPECT_TRUE(r.InclusiveContains(PointF(40, 20)));
  EXPECT_FALSE(r.InclusiveContains(PointF(40.0001f, 20)));
  EXPECT_TRUE(r.InclusiveContains(PointF(10, 60)));
  EXPECT_FALSE(r.InclusiveContains(PointF(10, 60.0001f)));
  EXPECT_TRUE(r.InclusiveContains(PointF(40, 60)));
  EXPECT_FALSE(r.InclusiveContains(PointF(100, 100)));
}

TEST(RectFTest, InclusiveContainsXY) {
  EXPECT_TRUE(RectF().InclusiveContains(0, 0));
  EXPECT_FALSE(RectF().InclusiveContains(0.0001f, 0));
  RectF r(10, 20, 30, 40);
  EXPECT_FALSE(r.InclusiveContains(0, 0));
  EXPECT_FALSE(r.InclusiveContains(9.9999f, 20));
  EXPECT_FALSE(r.InclusiveContains(10, 19.9999f));
  EXPECT_TRUE(r.InclusiveContains(10, 20));
  EXPECT_TRUE(r.InclusiveContains(40, 20));
  EXPECT_FALSE(r.InclusiveContains(40.0001f, 20));
  EXPECT_TRUE(r.InclusiveContains(10, 60));
  EXPECT_FALSE(r.InclusiveContains(10, 60.0001f));
  EXPECT_TRUE(r.InclusiveContains(40, 60));
  EXPECT_FALSE(r.InclusiveContains(100, 100));
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
  // If Point A and point B are far apart such that the exact result of
  // A.x - B.x cannot be presented by float, then the width or height increases
  // such that both A and B are included in the bounding rect.
  RectF boundingRect1(
      BoundingRect(PointF(20.0f, -1000000000.0f), PointF(20.0f, 10.0f)));
  EXPECT_TRUE(boundingRect1.InclusiveContains(PointF(20.0f, -1000000000.0f)));
  EXPECT_TRUE(boundingRect1.InclusiveContains(PointF(20.0f, 10.0f)));
  RectF boundingRect2(
      BoundingRect(PointF(20.0f, 20.0f), PointF(20.0f, 1000000000.0f)));
  EXPECT_TRUE(boundingRect2.InclusiveContains(PointF(20.0f, 20.0f)));
  EXPECT_TRUE(boundingRect2.InclusiveContains(PointF(20.0f, 1000000000.0f)));
  RectF boundingRect3(
      BoundingRect(PointF(-1000000000.0f, 20.0f), PointF(20.0f, 20.0f)));
  EXPECT_TRUE(boundingRect3.InclusiveContains(PointF(-1000000000.0f, 20.0f)));
  EXPECT_TRUE(boundingRect3.InclusiveContains(PointF(20.0f, 20.0f)));
  RectF boundingRect4(
      BoundingRect(PointF(20.0f, 20.0f), PointF(1000000000.0f, 20.0f)));
  EXPECT_TRUE(boundingRect4.InclusiveContains(PointF(20.0f, 20.0f)));
  EXPECT_TRUE(boundingRect4.InclusiveContains(PointF(1000000000.0f, 20.0f)));
}

TEST(RectFTest, Union) {
  EXPECT_RECTF_EQ(RectF(), UnionRects(RectF(), RectF()));
  EXPECT_RECTF_EQ(
      RectF(1.1f, 2.2f, 3.3f, 4.4f),
      UnionRects(RectF(1.1f, 2.2f, 3.3f, 4.4f), RectF(1.1f, 2.2f, 3.3f, 4.4f)));
  EXPECT_RECTF_EQ(
      RectF(0, 0, 8.8f, 11.0f),
      UnionRects(RectF(0, 0, 3.3f, 4.4f), RectF(3.3f, 4.4f, 5.5f, 6.6f)));
  EXPECT_RECTF_EQ(
      RectF(0, 0, 8.8f, 11.0f),
      UnionRects(RectF(3.3f, 4.4f, 5.5f, 6.6f), RectF(0, 0, 3.3f, 4.4f)));
  EXPECT_RECTF_EQ(
      RectF(0, 1.1f, 3.3f, 8.8f),
      UnionRects(RectF(0, 1.1f, 3.3f, 4.4f), RectF(0, 5.5f, 3.3f, 4.4f)));
  EXPECT_RECTF_EQ(
      RectF(0, 1.1f, 11.0f, 12.1f),
      UnionRects(RectF(0, 1.1f, 3.3f, 4.4f), RectF(4.4f, 5.5f, 6.6f, 7.7f)));
  EXPECT_RECTF_EQ(
      RectF(0, 1.1f, 11.0f, 12.1f),
      UnionRects(RectF(4.4f, 5.5f, 6.6f, 7.7f), RectF(0, 1.1f, 3.3f, 4.4f)));
  EXPECT_RECTF_EQ(
      RectF(2.2f, 3.3f, 4.4f, 5.5f),
      UnionRects(RectF(8.8f, 9.9f, 0, 2.2f), RectF(2.2f, 3.3f, 4.4f, 5.5f)));
  EXPECT_RECTF_EQ(
      RectF(2.2f, 3.3f, 4.4f, 5.5f),
      UnionRects(RectF(2.2f, 3.3f, 4.4f, 5.5f), RectF(8.8f, 9.9f, 2.2f, 0)));
}

TEST(RectFTest, UnionEvenIfEmpty) {
  EXPECT_RECTF_EQ(RectF(), UnionRectsEvenIfEmpty(RectF(), RectF()));
  EXPECT_RECTF_EQ(RectF(0, 0, 3.3f, 4.4f),
                  UnionRectsEvenIfEmpty(RectF(), RectF(3.3f, 4.4f, 0, 0)));
  EXPECT_RECTF_EQ(RectF(0, 0, 8.8f, 11.0f),
                  UnionRectsEvenIfEmpty(RectF(0, 0, 3.3f, 4.4f),
                                        RectF(3.3f, 4.4f, 5.5f, 6.6f)));
  EXPECT_RECTF_EQ(RectF(0, 0, 8.8f, 11.0f),
                  UnionRectsEvenIfEmpty(RectF(3.3f, 4.4f, 5.5f, 6.6f),
                                        RectF(0, 0, 3.3f, 4.4f)));
  EXPECT_RECTF_EQ(RectF(2.2f, 3.3f, 6.6f, 8.8f),
                  UnionRectsEvenIfEmpty(RectF(8.8f, 9.9f, 0, 2.2f),
                                        RectF(2.2f, 3.3f, 4.4f, 5.5f)));
  EXPECT_RECTF_EQ(RectF(2.2f, 3.3f, 8.8f, 6.6f),
                  UnionRectsEvenIfEmpty(RectF(2.2f, 3.3f, 4.4f, 5.5f),
                                        RectF(8.8f, 9.9f, 2.2f, 0)));
}

TEST(RectFTest, UnionEnsuresContainWithFloatingError) {
  for (float f = 0.1f; f < 5; f += 0.1f) {
    RectF r1(1, 2, 3, 4);
    r1.Scale(f, f + 0.05f);
    RectF r2 = r1 + Vector2dF(10.f + f, f - 10.f);
    RectF r3 = UnionRects(r1, r2);
    EXPECT_TRUE(r3.Contains(r1));
    EXPECT_TRUE(r3.Contains(r2));
  }
}

TEST(RectFTest, UnionIfEmptyResultTinySize) {
  RectF r1(1e-15f, 0, 0, 0);
  RectF r2(0, 1e-15f, 0, 0);
  RectF r3 = UnionRectsEvenIfEmpty(r1, r2);
  EXPECT_FALSE(r3.IsEmpty());
  EXPECT_TRUE(r3.Contains(r1));
  EXPECT_TRUE(r3.Contains(r2));
}

TEST(RectFTest, UnionMaxRects) {
  constexpr float kMaxFloat = std::numeric_limits<float>::max();
  constexpr float kMinFloat = std::numeric_limits<float>::min();
  gfx::RectF r1(kMinFloat, 0, kMaxFloat, kMaxFloat);
  gfx::RectF r2(0, kMinFloat, kMaxFloat, kMaxFloat);
  // This should not trigger DCHECK failure.
  r1.Union(r2);
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
  constexpr RectF input(3, 3, 3, 3);
  constexpr SizeF size_f(2.0f, 3.0f);
  constexpr Size size(3, 2);
  EXPECT_RECTF_EQ(RectF(4.5f, 4.5f, 4.5f, 4.5f), ScaleRect(input, 1.5f));
  EXPECT_RECTF_EQ(RectF(6.0f, 9.0f, 6.0f, 9.0f), ScaleRect(input, size_f));
  EXPECT_RECTF_EQ(RectF(9.0f, 6.0f, 9.0f, 6.0f), ScaleRect(input, size));
  EXPECT_RECTF_EQ(RectF(0, 0, 0, 0), ScaleRect(input, 0));

  constexpr float kMaxFloat = std::numeric_limits<float>::max();
  constexpr int kMaxInt = std::numeric_limits<int>::max();
  EXPECT_RECTF_EQ(RectF(kMaxFloat, kMaxFloat, kMaxFloat, kMaxFloat),
                  ScaleRect(input, kMaxFloat));
  EXPECT_RECTF_EQ(RectF(input.x() * static_cast<float>(kMaxInt),
                        input.y() * static_cast<float>(kMaxInt),
                        input.width() * static_cast<float>(kMaxInt),
                        input.height() * static_cast<float>(kMaxInt)),
                  ScaleRect(input, Size(kMaxInt, kMaxInt)));

  RectF nan_rect = ScaleRect(input, std::numeric_limits<float>::quiet_NaN());
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

  EXPECT_FLOAT_EQ(0.0f,
                  f.ManhattanInternalDistance(RectF(-1.0f, 0.0f, 2.0f, 1.0f)));
  EXPECT_FLOAT_EQ(
      kEpsilon, f.ManhattanInternalDistance(RectF(400.0f, 0.0f, 1.0f, 400.0f)));
  EXPECT_FLOAT_EQ(2.0f * kEpsilon, f.ManhattanInternalDistance(RectF(
                                       -100.0f, -100.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(1.0f + kEpsilon, f.ManhattanInternalDistance(
                                       RectF(-101.0f, 100.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(
      2.0f + 2.0f * kEpsilon,
      f.ManhattanInternalDistance(RectF(-101.0f, -101.0f, 100.0f, 100.0f)));
  EXPECT_FLOAT_EQ(
      433.0f + 2.0f * kEpsilon,
      f.ManhattanInternalDistance(RectF(630.0f, 603.0f, 100.0f, 100.0f)));

  EXPECT_FLOAT_EQ(0.0f,
                  f.ManhattanInternalDistance(RectF(-1.0f, 0.0f, 1.1f, 1.0f)));
  EXPECT_FLOAT_EQ(0.1f + kEpsilon,
                  f.ManhattanInternalDistance(RectF(-1.5f, 0.0f, 1.4f, 1.0f)));
  EXPECT_FLOAT_EQ(kEpsilon,
                  f.ManhattanInternalDistance(RectF(-1.5f, 0.0f, 1.5f, 1.0f)));
}

TEST(RectFTest, Inset) {
  RectF r(10, 20, 30, 40);
  r.Inset(0);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
  r.Inset(1.5);
  EXPECT_RECTF_EQ(RectF(11.5, 21.5, 27, 37), r);
  r.Inset(-1.5);
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Inset(InsetsF::VH(2.25, 1.5));
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 27, 35.5), r);
  r.Inset(InsetsF::VH(-2.25, -1.5));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // The parameters are left, top, right, bottom.
  r.Inset(InsetsF::TLBR(2.25, 1.5, 4, 3.75));
  EXPECT_RECTF_EQ(RectF(11.5, 22.25, 24.75, 33.75), r);
  r.Inset(InsetsF::TLBR(-2.25, -1.5, -4, -3.75));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  // InsetsF parameters are top, right, bottom, left.
  r.Inset(InsetsF::TLBR(1.5, 2.25, 3.75, 4));
  EXPECT_RECTF_EQ(RectF(12.25, 21.5, 23.75, 34.75), r);
  r.Inset(InsetsF::TLBR(-1.5, -2.25, -3.75, -4));
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

  r.Outset(OutsetsF::VH(2.25, 1.5));
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 33, 44.5), r);
  r.Outset(OutsetsF::VH(-2.25, -1.5));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);

  r.Outset(OutsetsF::TLBR(2.25, 1.5, 4, 3.75));
  EXPECT_RECTF_EQ(RectF(8.5, 17.75, 35.25, 46.25), r);
  r.Outset(OutsetsF::TLBR(-2.25, -1.5, -4, -3.75));
  EXPECT_RECTF_EQ(RectF(10, 20, 30, 40), r);
}

TEST(RectFTest, InsetClamped) {
  RectF r(10, 20, 30, 40);
  r.Inset(18);
  EXPECT_RECTF_EQ(RectF(28, 38, 0, 4), r);
  r.Inset(-18);
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 40), r);

  r.Inset(InsetsF::VH(30, 15));
  EXPECT_RECTF_EQ(RectF(25, 50, 6, 0), r);
  r.Inset(InsetsF::VH(-30, -15));
  EXPECT_RECTF_EQ(RectF(10, 20, 36, 60), r);

  r.Inset(InsetsF::TLBR(30, 20, 50, 40));
  EXPECT_RECTF_EQ(RectF(30, 50, 0, 0), r);
  r.Inset(InsetsF::TLBR(-30, -20, -50, -40));
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

TEST(RectFTest, ClosestPoint) {
  //         r.x()=50   r.right()=350
  //            |          |
  //        1   |    2     |  3
  //      ------+----------+--------r.y()=100
  //        4   |    5(in) |  6
  //      ------+----------+--------r.bottom()=250
  //        7   |    8     |  9

  RectF r(50, 100, 300, 150);
  // 1
  EXPECT_EQ(PointF(50, 100), r.ClosestPoint(PointF(10, 20)));
  // 2
  EXPECT_EQ(PointF(110, 100), r.ClosestPoint(PointF(110, 80)));
  // 3
  EXPECT_EQ(PointF(350, 100), r.ClosestPoint(PointF(400, 80)));
  // 4
  EXPECT_EQ(PointF(50, 110), r.ClosestPoint(PointF(10, 110)));
  // 5
  EXPECT_EQ(PointF(50, 100), r.ClosestPoint(PointF(50, 100)));
  EXPECT_EQ(PointF(150, 100), r.ClosestPoint(PointF(150, 100)));
  EXPECT_EQ(PointF(350, 100), r.ClosestPoint(PointF(350, 100)));
  EXPECT_EQ(PointF(350, 150), r.ClosestPoint(PointF(350, 150)));
  EXPECT_EQ(PointF(350, 250), r.ClosestPoint(PointF(350, 250)));
  EXPECT_EQ(PointF(150, 250), r.ClosestPoint(PointF(150, 250)));
  EXPECT_EQ(PointF(50, 250), r.ClosestPoint(PointF(50, 250)));
  EXPECT_EQ(PointF(50, 150), r.ClosestPoint(PointF(50, 150)));
  EXPECT_EQ(PointF(150, 150), r.ClosestPoint(PointF(150, 150)));
  // 6
  EXPECT_EQ(PointF(350, 150), r.ClosestPoint(PointF(380, 150)));
  // 7
  EXPECT_EQ(PointF(50, 250), r.ClosestPoint(PointF(10, 280)));
  // 8
  EXPECT_EQ(PointF(180, 250), r.ClosestPoint(PointF(180, 300)));
  // 9
  EXPECT_EQ(PointF(350, 250), r.ClosestPoint(PointF(450, 450)));
}

TEST(RectFTest, MapRect) {
  EXPECT_RECTF_EQ(RectF(), MapRect(RectF(), RectF(), RectF()));
  EXPECT_RECTF_EQ(RectF(),
                  MapRect(RectF(1, 2, 3, 4), RectF(), RectF(5, 6, 7, 8)));
  EXPECT_RECTF_EQ(
      RectF(1, 2, 3, 4),
      MapRect(RectF(1, 2, 3, 4), RectF(5, 6, 7, 8), RectF(5, 6, 7, 8)));
  EXPECT_RECTF_EQ(
      RectF(5, 6, 7, 8),
      MapRect(RectF(1, 2, 3, 4), RectF(1, 2, 3, 4), RectF(5, 6, 7, 8)));
  EXPECT_RECTF_EQ(
      RectF(200, 300, 300, 400),
      MapRect(RectF(1, 2, 3, 4), RectF(0, 1, 6, 8), RectF(100, 200, 600, 800)));
  EXPECT_RECTF_EQ(RectF(1, 2, 3, 4),
                  MapRect(RectF(200, 300, 300, 400), RectF(100, 200, 600, 800),
                          RectF(0, 1, 6, 8)));
}

TEST(RectFTest, ApproximatelyEqual) {
  RectF rect1(10, 20, 1920, 1080);
  RectF rect2(10, 20, 1920, 1080);
  EXPECT_TRUE(rect1.ApproximatelyEqual(rect2, 0.0f, 0.0f));
  EXPECT_TRUE(rect1.ApproximatelyEqual(rect2, 0.001f, 0.001f));
  EXPECT_TRUE(rect1.ApproximatelyEqual(rect2, 0.001f, 0.00001f));
  EXPECT_TRUE(rect1.ApproximatelyEqual(rect2, 0.00001f, 0.001f));
  EXPECT_TRUE(rect1.ApproximatelyEqual(rect2, 0.00001f, 0.00001f));

  RectF rect3(9.999965732, 20, 1920, 1080);
  RectF rect4(10, 20, 1920, 1080);
  EXPECT_FALSE(rect3.ApproximatelyEqual(rect4, 0.0f, 0.0f));
  EXPECT_TRUE(rect3.ApproximatelyEqual(rect4, 0.001f, 0.001f));
  EXPECT_TRUE(rect3.ApproximatelyEqual(rect4, 0.001f, 0.000001f));
  EXPECT_FALSE(rect3.ApproximatelyEqual(rect4, 0.000001f, 0.001f));
  EXPECT_FALSE(rect3.ApproximatelyEqual(rect4, 0.000001f, 0.000001f));

  RectF rect5(10, 20.000001, 1920, 1080);
  RectF rect6(10, 20, 1920, 1080);
  EXPECT_FALSE(rect5.ApproximatelyEqual(rect6, 0.0f, 0.0f));
  EXPECT_TRUE(rect5.ApproximatelyEqual(rect6, 0.001f, 0.001f));
  EXPECT_FALSE(rect5.ApproximatelyEqual(rect6, 0.001f, 0.0000001f));
  EXPECT_TRUE(rect5.ApproximatelyEqual(rect6, 0.0000001f, 0.001f));
  EXPECT_FALSE(rect5.ApproximatelyEqual(rect6, 0.0000001f, 0.0000001f));

  RectF rect7(10, 20, 1919.99987792969, 1080);
  RectF rect8(10, 20, 1920, 1080);
  EXPECT_FALSE(rect7.ApproximatelyEqual(rect8, 0.0f, 0.0f));
  EXPECT_TRUE(rect7.ApproximatelyEqual(rect8, 0.001f, 0.001f));
  EXPECT_TRUE(rect7.ApproximatelyEqual(rect8, 0.001f, 0.00001f));
  EXPECT_FALSE(rect7.ApproximatelyEqual(rect8, 0.00001f, 0.001f));
  EXPECT_FALSE(rect7.ApproximatelyEqual(rect8, 0.00001f, 0.00001f));

  RectF rect9(10, 20, 1920, 1080.0001);
  RectF rect10(10, 20, 1920, 1080);
  EXPECT_FALSE(rect9.ApproximatelyEqual(rect10, 0.0f, 0.0f));
  EXPECT_TRUE(rect9.ApproximatelyEqual(rect10, 0.001f, 0.001f));
  EXPECT_FALSE(rect9.ApproximatelyEqual(rect10, 0.001f, 0.000001f));
  EXPECT_TRUE(rect9.ApproximatelyEqual(rect10, 0.000001f, 0.001f));
  EXPECT_FALSE(rect9.ApproximatelyEqual(rect10, 0.000001f, 0.000001f));
}

}  // namespace gfx
