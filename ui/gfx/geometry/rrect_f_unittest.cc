// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rrect_f.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f_builder.h"

namespace gfx {

TEST(RRectFTest, IsEmpty) {
  EXPECT_TRUE(RRectF().IsEmpty());
  EXPECT_TRUE(RRectF(0, 0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(RRectF(0, 0, 10, 0, 0).IsEmpty());
  EXPECT_TRUE(RRectF(0, 0, 0, 10, 0).IsEmpty());
  EXPECT_TRUE(RRectF(0, 0, 0, 10, 10).IsEmpty());
  EXPECT_FALSE(RRectF(0, 0, 10, 10, 0).IsEmpty());
}

TEST(RRectFTest, Equals) {
  EXPECT_EQ(RRectF(0, 0, 0, 0, 0, 0), RRectF(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(RRectF(1, 2, 3, 4, 5, 6), RRectF(1, 2, 3, 4, 5, 6));
  EXPECT_EQ(RRectF(1, 2, 3, 4, 5, 5), RRectF(1, 2, 3, 4, 5));
  EXPECT_EQ(RRectF(0, 0, 2, 3, 0, 0), RRectF(0, 0, 2, 3, 0, 1));
  EXPECT_EQ(RRectF(0, 0, 2, 3, 0, 0), RRectF(0, 0, 2, 3, 1, 0));
  EXPECT_EQ(RRectF(1, 2, 3, 0, 5, 6), RRectF(0, 0, 0, 0, 0, 0));
  EXPECT_EQ(RRectF(0, 0, 0, 0, 5, 6), RRectF(0, 0, 0, 0, 0, 0));

  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(1, 20, 30, 40, 7, 8));
  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(10, 2, 30, 40, 7, 8));
  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(10, 20, 3, 40, 7, 8));
  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(10, 20, 30, 4, 7, 8));
  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(10, 20, 30, 40, 5, 8));
  EXPECT_NE(RRectF(10, 20, 30, 40, 7, 8), RRectF(10, 20, 30, 40, 7, 6));
}

TEST(RRectFTest, PlusMinusOffset) {
  const RRectF a(40, 50, 60, 70, 5);
  gfx::Vector2d offset(23, 34);
  RRectF correct(63, 84, 60, 70, 5);
  RRectF b = a + offset;
  ASSERT_EQ(b, correct);
  b = a;
  b.Offset(offset);
  ASSERT_EQ(b, correct);

  correct = RRectF(17, 16, 60, 70, 5);
  b = a - offset;
  ASSERT_EQ(b, correct);
  b = a;
  b.Offset(-offset);
  ASSERT_EQ(b, correct);
}

TEST(RRectFTest, RRectTypes) {
  RRectF a(40, 50, 0, 70, 0);
  EXPECT_EQ(a.GetType(), RRectF::Type::kEmpty);
  EXPECT_TRUE(a.IsEmpty());
  a = RRectF(40, 50, 60, 70, 0);
  EXPECT_EQ(a.GetType(), RRectF::Type::kRect);
  a = RRectF(40, 50, 60, 70, 5);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSingle);
  a = RRectF(40, 50, 60, 70, 5, 5);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSingle);
  a = RRectF(40, 50, 60, 60, 30, 30);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSingle);
  a = RRectF(40, 50, 60, 70, 6, 3);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSimple);
  a = RRectF(40, 50, 60, 70, 30, 3);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSimple);
  a = RRectF(40, 50, 60, 70, 30, 35);
  EXPECT_EQ(a.GetType(), RRectF::Type::kOval);
  a.SetCornerRadii(RRectF::Corner::kLowerRight, gfx::Vector2dF(7, 8));
  EXPECT_EQ(a.GetType(), RRectF::Type::kComplex);

  // When one radius is larger than half its dimension, both radii are scaled
  // down proportionately.
  a = RRectF(40, 50, 60, 70, 30, 70);
  EXPECT_EQ(a.GetType(), RRectF::Type::kSimple);
  EXPECT_EQ(a, RRectF(40, 50, 60, 70, 15, 35));
  // If they stay equal to half the radius, it stays oval.
  a = RRectF(40, 50, 60, 70, 120, 140);
  EXPECT_EQ(a.GetType(), RRectF::Type::kOval);
}

void CheckRadii(RRectF val,
                float ulx,
                float uly,
                float urx,
                float ury,
                float lrx,
                float lry,
                float llx,
                float lly) {
  EXPECT_EQ(val.GetCornerRadii(RRectF::Corner::kUpperLeft),
            gfx::Vector2dF(ulx, uly));
  EXPECT_EQ(val.GetCornerRadii(RRectF::Corner::kUpperRight),
            gfx::Vector2dF(urx, ury));
  EXPECT_EQ(val.GetCornerRadii(RRectF::Corner::kLowerRight),
            gfx::Vector2dF(lrx, lry));
  EXPECT_EQ(val.GetCornerRadii(RRectF::Corner::kLowerLeft),
            gfx::Vector2dF(llx, lly));
}

TEST(RRectFTest, RRectRadii) {
  RRectF a(40, 50, 60, 70, 0);
  CheckRadii(a, 0, 0, 0, 0, 0, 0, 0, 0);

  a.SetCornerRadii(RRectF::Corner::kUpperLeft, 1, 2);
  CheckRadii(a, 1, 2, 0, 0, 0, 0, 0, 0);

  a.SetCornerRadii(RRectF::Corner::kUpperRight, 3, 4);
  CheckRadii(a, 1, 2, 3, 4, 0, 0, 0, 0);

  a.SetCornerRadii(RRectF::Corner::kLowerRight, 5, 6);
  CheckRadii(a, 1, 2, 3, 4, 5, 6, 0, 0);

  a.SetCornerRadii(RRectF::Corner::kLowerLeft, 7, 8);
  CheckRadii(a, 1, 2, 3, 4, 5, 6, 7, 8);

  RRectF b(40, 50, 60, 70, 1, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(a, b);
}

TEST(RRectFTest, FromRectF) {
  // Check that explicit conversion from float rect works.
  RectF a(40, 50, 60, 70);
  RRectF b(40, 50, 60, 70, 0);
  RRectF c = RRectF(a);
  EXPECT_EQ(b, c);
}

TEST(RRectFTest, FromSkRRect) {
  // Check that explicit conversion from SkRRect works.
  SkRRect a = SkRRect::MakeRectXY(SkRect::MakeXYWH(40, 50, 60, 70), 15, 25);
  RRectF b(40, 50, 60, 70, 15, 25);
  RRectF c = RRectF(a);
  EXPECT_EQ(b, c);

  // Try with single radius constructor.
  a = SkRRect::MakeRectXY(SkRect::MakeXYWH(40, 50, 60, 70), 15, 15);
  b = RRectF(40, 50, 60, 70, 15);
  c = RRectF(a);
  EXPECT_EQ(b, c);
}

TEST(RRectFTest, FromRoundedCornersF) {
  constexpr RectF kRect(50.0f, 40.0f);
  constexpr RoundedCornersF kCorners(1.5f, 2.5f, 3.5f, 4.5f);
  const RRectF rrect_f(kRect, kCorners);

  const auto upper_left = rrect_f.GetCornerRadii(RRectF::Corner::kUpperLeft);
  EXPECT_EQ(kCorners.upper_left(), upper_left.x());
  EXPECT_EQ(kCorners.upper_left(), upper_left.y());
  const auto upper_right = rrect_f.GetCornerRadii(RRectF::Corner::kUpperRight);
  EXPECT_EQ(kCorners.upper_right(), upper_right.x());
  EXPECT_EQ(kCorners.upper_right(), upper_right.y());
  const auto lower_right = rrect_f.GetCornerRadii(RRectF::Corner::kLowerRight);
  EXPECT_EQ(kCorners.lower_right(), lower_right.x());
  EXPECT_EQ(kCorners.lower_right(), lower_right.y());
  const auto lower_left = rrect_f.GetCornerRadii(RRectF::Corner::kLowerLeft);
  EXPECT_EQ(kCorners.lower_left(), lower_left.x());
  EXPECT_EQ(kCorners.lower_left(), lower_left.y());
}

TEST(RRectFTest, ToString) {
  RRectF a(40, 50, 60, 70, 0);
  EXPECT_EQ(a.ToString(), "40.000,50.000 60.000x70.000, rectangular");
  a = RRectF(40, 50, 60, 70, 15);
  EXPECT_EQ(a.ToString(), "40.000,50.000 60.000x70.000, radius 15.000");
  a = RRectF(40, 50, 60, 70, 15, 25);
  EXPECT_EQ(a.ToString(),
            "40.000,50.000 60.000x70.000, x_rad 15.000, y_rad 25.000");
  a.SetCornerRadii(RRectF::Corner::kLowerRight, gfx::Vector2dF(7, 8));
  EXPECT_EQ(a.ToString(),
            "40.000,50.000 60.000x70.000, [15.000 25.000] "
            "[15.000 25.000] [7.000 8.000] [15.000 25.000]");
}

TEST(RRectFTest, Sizes) {
  RRectF a(40, 50, 60, 70, 5, 6);
  EXPECT_EQ(a.rect().x(), 40);
  EXPECT_EQ(a.rect().y(), 50);
  EXPECT_EQ(a.rect().width(), 60);
  EXPECT_EQ(a.rect().height(), 70);
  EXPECT_EQ(a.GetSimpleRadii().x(), 5);
  EXPECT_EQ(a.GetSimpleRadii().y(), 6);
  a = RRectF(40, 50, 60, 70, 5, 5);
  EXPECT_EQ(a.GetSimpleRadius(), 5);
  a.Clear();
  EXPECT_TRUE(a.IsEmpty());
  // Make sure ovals can still get simple radii
  a = RRectF(40, 50, 60, 70, 30, 35);
  EXPECT_EQ(a.GetType(), RRectF::Type::kOval);
  EXPECT_EQ(a.GetSimpleRadii().x(), 30);
  EXPECT_EQ(a.GetSimpleRadii().y(), 35);
}

TEST(RRectFTest, Contains) {
  RRectF a(40, 50, 60, 70, 5, 6);
  RectF b(50, 60, 5, 6);
  EXPECT_TRUE(a.Contains(b));
  b = RectF(40, 50, 5, 6);  // Right on the border
  EXPECT_FALSE(a.Contains(b));
  b = RectF(95, 114, 5, 6);  // Right on the border
  EXPECT_FALSE(a.Contains(b));
  b = RectF(40, 50, 60, 70);
  EXPECT_FALSE(a.Contains(b));
}

TEST(RRectFTest, HasRoundedCorners) {
  RRectF a;
  EXPECT_FALSE(a.HasRoundedCorners());
  a = gfx::RRectF(gfx::RectF(10, 10));
  EXPECT_FALSE(a.HasRoundedCorners());
  a = gfx::RRectF(gfx::RectF(), gfx::RoundedCornersF(1.0f));
  EXPECT_FALSE(a.HasRoundedCorners());
  a = gfx::RRectF(gfx::RectF(10, 10), gfx::RoundedCornersF(1.0f));
  EXPECT_TRUE(a.HasRoundedCorners());
  a = gfx::RRectF(gfx::RectF(10, 10), gfx::RoundedCornersF(1, 0, 0, 0));
  EXPECT_TRUE(a.HasRoundedCorners());
}

TEST(RRectFTest, Scale) {
  static const struct Test {
    float x1;  // source
    float y1;
    float w1;
    float h1;
    float x_rad1;
    float y_rad1;

    float x_scale;
    float y_scale;
    float x2;  // target
    float y2;
    float w2;
    float h2;
    float x_rad2;
    float y_rad2;
  } tests[] = {
      {3.0f, 4.0f, 5.0f, 6.0f, 0.0f, 0.0f, 1.5f, 1.5f, 4.5f, 6.0f, 7.5f, 9.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 1.5f, 1.5f, 4.5f, 6.0f, 7.5f, 9.0f,
       1.5f, 1.5f},
      {3.0f, 4.0f, 5.0f, 6.0f, 0.0f, 0.0f, 1.5f, 3.0f, 4.5f, 12.0f, 7.5f, 18.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 1.5f, 3.0f, 4.5f, 12.0f, 7.5f, 18.0f,
       1.5f, 3.0f},
      {3.0f, 4.0f, 0.0f, 6.0f, 1.0f, 1.0f, 1.5f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, std::numeric_limits<float>::max(),
       1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 1.0f,
       std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, std::numeric_limits<float>::max(),
       std::numeric_limits<float>::max(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f,
       std::numeric_limits<double>::quiet_NaN(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f, 1.0f,
       std::numeric_limits<double>::quiet_NaN(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f},
      {3.0f, 4.0f, 5.0f, 6.0f, 1.0f, 1.0f,
       std::numeric_limits<double>::quiet_NaN(),
       std::numeric_limits<double>::quiet_NaN(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       0.0f},
  };

  for (auto& test : tests) {
    RRectF r1(test.x1, test.y1, test.w1, test.h1, test.x_rad1, test.y_rad1);
    RRectF r2(test.x2, test.y2, test.w2, test.h2, test.x_rad2, test.y_rad2);

    r1.Scale(test.x_scale, test.y_scale);
    ASSERT_TRUE(r1.GetType() <= RRectF::Type::kSimple);
    EXPECT_EQ(r1.rect().x(), r2.rect().x());
    EXPECT_EQ(r1.rect().y(), r2.rect().y());
    EXPECT_EQ(r1.rect().width(), r2.rect().width());
    EXPECT_EQ(r1.rect().height(), r2.rect().height());
    EXPECT_EQ(r1.GetSimpleRadii(), r2.GetSimpleRadii());
  }
}

TEST(RRectFTest, InsetOutset) {
  RRectF a(40, 50, 60, 70, 5);
  RRectF b = a;
  b.Inset(3);
  ASSERT_EQ(b, RRectF(43, 53, 54, 64, 2));
  b = a;
  b.Outset(3);
  ASSERT_EQ(b, RRectF(37, 47, 66, 76, 8));
}

// The following tests(started with "Build*") are for RRectFBuilder. All
// different tests are to make sure that existing RRectF definitions can be
// implemented with RRectFBuilder.
TEST(RRectFTest, BuildFromRectF) {
  RectF a = RectF();
  RRectF b(a);
  RRectF c = RRectFBuilder().set_rect(a).Build();
  EXPECT_EQ(b, c);

  a = RectF(60, 70);
  b = RRectF(a);
  c = RRectFBuilder().set_rect(a).Build();
  EXPECT_EQ(b, c);

  a = RectF(40, 50, 60, 70);
  b = RRectF(a);
  c = RRectFBuilder().set_rect(a).Build();
  EXPECT_EQ(b, c);
}

TEST(RRectFTest, BuildFromRadius) {
  RRectF a(40, 50, 60, 70, 15);
  RRectF b = RRectFBuilder()
                 .set_origin(40, 50)
                 .set_size(60, 70)
                 .set_radius(15)
                 .Build();
  EXPECT_EQ(a, b);

  a = RRectF(40, 50, 60, 70, 15, 25);
  b = RRectFBuilder()
          .set_origin(40, 50)
          .set_size(60, 70)
          .set_radius(15, 25)
          .Build();
  EXPECT_EQ(a, b);

  const PointF p(40, 50);
  const SizeF s(60, 70);
  b = RRectFBuilder().set_origin(p).set_size(s).set_radius(15, 25).Build();
  EXPECT_EQ(a, b);
}

TEST(RRectFTest, BuildFromRectFWithRadius) {
  RectF a(40, 50, 60, 70);
  RRectF b(a, 15);
  RRectF c = RRectFBuilder().set_rect(a).set_radius(15).Build();
  EXPECT_EQ(b, c);

  b = RRectF(a, 15, 25);
  c = RRectFBuilder().set_rect(a).set_radius(15, 25).Build();
  EXPECT_EQ(b, c);
}

TEST(RRectFTest, BuildFromCorners) {
  RRectF a(40, 50, 60, 70, 1, 2, 3, 4, 5, 6, 7, 8);
  RRectF b = RRectFBuilder()
                 .set_origin(40, 50)
                 .set_size(60, 70)
                 .set_upper_left(1, 2)
                 .set_upper_right(3, 4)
                 .set_lower_right(5, 6)
                 .set_lower_left(7, 8)
                 .Build();
  EXPECT_EQ(a, b);
}

TEST(RRectFTest, BuildFromRectFWithCorners) {
  RectF a(40, 50, 60, 70);
  RRectF b(a, 1, 2, 3, 4, 5, 6, 7, 8);
  RRectF c = RRectFBuilder()
                 .set_rect(a)
                 .set_upper_left(1, 2)
                 .set_upper_right(3, 4)
                 .set_lower_right(5, 6)
                 .set_lower_left(7, 8)
                 .Build();
  EXPECT_EQ(b, c);
}

TEST(RRectFTest, BuildFromRoundedCornersF) {
  RectF a(40, 50, 60, 70);
  RoundedCornersF corners(1.5f, 2.5f, 3.5f, 4.5f);
  RRectF b(a, corners);
  RRectF c = RRectFBuilder().set_rect(a).set_corners(corners).Build();
  EXPECT_EQ(b, c);
}

// In the following tests(*CornersHigherThanSize), we test whether the corner
// radii gets truncated in case of being greater than the width/height.
TEST(RRectFTest, BuildFromCornersHigherThanSize) {
  RRectF a(0, 0, 20, 10, 12, 2, 8, 4, 14, 6, 6, 8);
  RRectF b = RRectFBuilder()
                 .set_origin(0, 0)
                 .set_size(20, 10)
                 .set_upper_left(48, 8)
                 .set_upper_right(32, 16)
                 .set_lower_right(56, 24)
                 .set_lower_left(24, 32)
                 .Build();
  EXPECT_EQ(a, b);
}

TEST(RRectFTest, BuildFromRectFWithCornersHigherThanSize) {
  RectF a(0, 0, 20, 10);
  RRectF b(a, 12, 2, 8, 4, 14, 6, 6, 8);
  RRectF c = RRectFBuilder()
                 .set_rect(a)
                 .set_upper_left(48, 8)
                 .set_upper_right(32, 16)
                 .set_lower_right(56, 24)
                 .set_lower_left(24, 32)
                 .Build();
  EXPECT_EQ(b, c);
}

// In this test, we set the radius first but then change the value of the
// corners.
TEST(RRectFTest, BuildFromRadiusAndCorners) {
  RRectF a(40, 50, 60, 70, 1, 2, 3, 4, 15, 25, 15, 25);
  RRectF b = RRectFBuilder()
                 .set_origin(40, 50)
                 .set_size(60, 70)
                 .set_radius(15, 25)
                 .set_upper_left(1, 2)
                 .set_upper_right(3, 4)
                 .Build();
  EXPECT_EQ(a, b);
}

}  // namespace gfx
