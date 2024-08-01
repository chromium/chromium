// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/rect.h"

#include <stddef.h>

#include <limits>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace gfx {

constexpr int kMaxInt = std::numeric_limits<int>::max();
constexpr int kMinInt = std::numeric_limits<int>::min();

TEST(RectTest, Contains) {
  static const struct ContainsCase {
    int rect_x;
    int rect_y;
    int rect_width;
    int rect_height;
    int point_x;
    int point_y;
    bool contained;
  } contains_cases[] = {
    {0, 0, 10, 10, 0, 0, true},
    {0, 0, 10, 10, 5, 5, true},
    {0, 0, 10, 10, 9, 9, true},
    {0, 0, 10, 10, 5, 10, false},
    {0, 0, 10, 10, 10, 5, false},
    {0, 0, 10, 10, -1, -1, false},
    {0, 0, 10, 10, 50, 50, false},
  #if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
    {0, 0, -10, -10, 0, 0, false},
  #endif
  };
  for (size_t i = 0; i < std::size(contains_cases); ++i) {
    const ContainsCase& value = contains_cases[i];
    Rect rect(value.rect_x, value.rect_y, value.rect_width, value.rect_height);
    EXPECT_EQ(value.contained, rect.Contains(value.point_x, value.point_y));
  }
}

TEST(RectTest, Intersects) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    bool intersects;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, false },
    { 0, 0, 0, 0, -10, -10, 20, 20, false },
    { -10, 0, 0, 20, 0, -10, 20, 0, false },
    { 0, 0, 10, 10, 0, 0, 10, 10, true },
    { 0, 0, 10, 10, 10, 10, 10, 10, false },
    { 10, 10, 10, 10, 0, 0, 10, 10, false },
    { 10, 10, 10, 10, 5, 5, 10, 10, true },
    { 10, 10, 10, 10, 15, 15, 10, 10, true },
    { 10, 10, 10, 10, 20, 15, 10, 10, false },
    { 10, 10, 10, 10, 21, 15, 10, 10, false }
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    EXPECT_EQ(tests[i].intersects, r1.Intersects(r2));
    EXPECT_EQ(tests[i].intersects, r2.Intersects(r1));
  }
}

TEST(RectTest, Intersect) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: the union of rects 1 and 2
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 0, 0,   // zeros
      0, 0, 0, 0,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // equal
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 0, 0, 4, 4,   // neighboring
      4, 4, 4, 4,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // overlapping corners
      2, 2, 4, 4,
      2, 2, 2, 2 },
    { 0, 0, 4, 4,   // T junction
      3, 1, 4, 2,
      3, 1, 1, 2 },
    { 3, 0, 2, 2,   // gap
      0, 0, 2, 2,
      0, 0, 0, 0 }
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    EXPECT_EQ(r3, IntersectRects(r1, r2));
  }
}

TEST(RectTest, InclusiveIntersect) {
  Rect rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(Rect(11, 12, 13, 14)));
  EXPECT_EQ(Rect(11, 12, 0, 0), rect);

  rect = Rect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(Rect(24, 8, 0, 7)));
  EXPECT_EQ(Rect(24, 12, 0, 3), rect);

  rect = Rect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(Rect(9, 15, 4, 0)));
  EXPECT_EQ(Rect(11, 15, 2, 0), rect);

  rect = Rect(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(Rect(12, 13, 15, 16)));
  EXPECT_EQ(Rect(), rect);
}

TEST(RectTest, Union) {
  EXPECT_EQ(Rect(), UnionRects(Rect(), Rect()));
  EXPECT_EQ(Rect(1, 2, 3, 4), UnionRects(Rect(1, 2, 3, 4), Rect(1, 2, 3, 4)));
  EXPECT_EQ(Rect(0, 0, 8, 10), UnionRects(Rect(0, 0, 3, 4), Rect(3, 4, 5, 6)));
  EXPECT_EQ(Rect(0, 0, 8, 10), UnionRects(Rect(3, 4, 5, 6), Rect(0, 0, 3, 4)));
  EXPECT_EQ(Rect(0, 1, 3, 8), UnionRects(Rect(0, 1, 3, 4), Rect(0, 5, 3, 4)));
  EXPECT_EQ(Rect(0, 1, 10, 11), UnionRects(Rect(0, 1, 3, 4), Rect(4, 5, 6, 7)));
  EXPECT_EQ(Rect(0, 1, 10, 11), UnionRects(Rect(4, 5, 6, 7), Rect(0, 1, 3, 4)));
  EXPECT_EQ(Rect(2, 3, 4, 5), UnionRects(Rect(8, 9, 0, 2), Rect(2, 3, 4, 5)));
  EXPECT_EQ(Rect(2, 3, 4, 5), UnionRects(Rect(2, 3, 4, 5), Rect(8, 9, 2, 0)));
}

TEST(RectTest, UnionEvenIfEmpty) {
  EXPECT_EQ(Rect(), UnionRectsEvenIfEmpty(Rect(), Rect()));
  EXPECT_EQ(Rect(0, 0, 3, 4), UnionRectsEvenIfEmpty(Rect(), Rect(3, 4, 0, 0)));
  EXPECT_EQ(Rect(0, 0, 8, 10),
            UnionRectsEvenIfEmpty(Rect(0, 0, 3, 4), Rect(3, 4, 5, 6)));
  EXPECT_EQ(Rect(0, 0, 8, 10),
            UnionRectsEvenIfEmpty(Rect(3, 4, 5, 6), Rect(0, 0, 3, 4)));
  EXPECT_EQ(Rect(2, 3, 6, 8),
            UnionRectsEvenIfEmpty(Rect(8, 9, 0, 2), Rect(2, 3, 4, 5)));
  EXPECT_EQ(Rect(2, 3, 8, 6),
            UnionRectsEvenIfEmpty(Rect(2, 3, 4, 5), Rect(8, 9, 2, 0)));
}

TEST(RectTest, Equals) {
  ASSERT_TRUE(Rect(0, 0, 0, 0) == Rect(0, 0, 0, 0));
  ASSERT_TRUE(Rect(1, 2, 3, 4) == Rect(1, 2, 3, 4));
  ASSERT_FALSE(Rect(0, 0, 0, 0) == Rect(0, 0, 0, 1));
  ASSERT_FALSE(Rect(0, 0, 0, 0) == Rect(0, 0, 1, 0));
  ASSERT_FALSE(Rect(0, 0, 0, 0) == Rect(0, 1, 0, 0));
  ASSERT_FALSE(Rect(0, 0, 0, 0) == Rect(1, 0, 0, 0));
}

TEST(RectTest, AdjustToFit) {
  static const struct Test {
    int x1;  // source
    int y1;
    int w1;
    int h1;
    int x2;  // target
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: results of invoking AdjustToFit
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 2, 2,
      0, 0, 2, 2,
      0, 0, 2, 2 },
    { 2, 2, 3, 3,
      0, 0, 4, 4,
      1, 1, 3, 3 },
    { -1, -1, 5, 5,
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 2, 2, 4, 4,
      0, 0, 3, 3,
      0, 0, 3, 3 },
    { 2, 2, 1, 1,
      0, 0, 3, 3,
      2, 2, 1, 1 }
  };
  for (size_t i = 0; i < std::size(tests); ++i) {
    Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    Rect u = r1;
    u.AdjustToFit(r2);
    EXPECT_EQ(r3, u);
  }
}

TEST(RectTest, Subtract) {
  Rect result;

  // Matching
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(10, 10, 20, 20));
  EXPECT_EQ(Rect(0, 0, 0, 0), result);

  // Contains
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(5, 5, 30, 30));
  EXPECT_EQ(Rect(0, 0, 0, 0), result);

  // No intersection
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(30, 30, 30, 30));
  EXPECT_EQ(Rect(10, 10, 20, 20), result);

  // Not a complete intersection in either direction
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(15, 15, 20, 20));
  EXPECT_EQ(Rect(10, 10, 20, 20), result);

  // Complete intersection in the x-direction, top edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(10, 15, 20, 20));
  EXPECT_EQ(Rect(10, 10, 20, 5), result);

  // Complete intersection in the x-direction, top edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(5, 15, 30, 20));
  EXPECT_EQ(Rect(10, 10, 20, 5), result);

  // Complete intersection in the x-direction, bottom edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(5, 5, 30, 20));
  EXPECT_EQ(Rect(10, 25, 20, 5), result);

  // Complete intersection in the x-direction, none of the edges is fully
  // covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(5, 15, 30, 1));
  EXPECT_EQ(Rect(10, 10, 20, 20), result);

  // Complete intersection in the y-direction, left edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(10, 10, 10, 30));
  EXPECT_EQ(Rect(20, 10, 10, 20), result);

  // Complete intersection in the y-direction, left edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(5, 5, 20, 30));
  EXPECT_EQ(Rect(25, 10, 5, 20), result);

  // Complete intersection in the y-direction, right edge is fully covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(20, 5, 20, 30));
  EXPECT_EQ(Rect(10, 10, 10, 20), result);

  // Complete intersection in the y-direction, none of the edges is fully
  // covered.
  result = Rect(10, 10, 20, 20);
  result.Subtract(Rect(15, 5, 1, 30));
  EXPECT_EQ(Rect(10, 10, 20, 20), result);
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE(Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 0, 0).size().IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 10, 0).IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 10, 0).size().IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 0, 10).IsEmpty());
  EXPECT_TRUE(Rect(0, 0, 0, 10).size().IsEmpty());
  EXPECT_FALSE(Rect(0, 0, 10, 10).IsEmpty());
  EXPECT_FALSE(Rect(0, 0, 10, 10).size().IsEmpty());
}

TEST(RectTest, SplitVertically) {
  Rect left_half, right_half;

  // Splitting when origin is (0, 0).
  Rect(0, 0, 20, 20).SplitVertically(left_half, right_half);
  EXPECT_TRUE(left_half == Rect(0, 0, 10, 20));
  EXPECT_TRUE(right_half == Rect(10, 0, 10, 20));

  // Splitting when origin is arbitrary.
  Rect(10, 10, 20, 10).SplitVertically(left_half, right_half);
  EXPECT_TRUE(left_half == Rect(10, 10, 10, 10));
  EXPECT_TRUE(right_half == Rect(20, 10, 10, 10));

  // Splitting a rectangle of zero width.
  Rect(10, 10, 0, 10).SplitVertically(left_half, right_half);
  EXPECT_TRUE(left_half == Rect(10, 10, 0, 10));
  EXPECT_TRUE(right_half == Rect(10, 10, 0, 10));

  // Splitting a rectangle of odd width.
  Rect(10, 10, 5, 10).SplitVertically(left_half, right_half);
  EXPECT_TRUE(left_half == Rect(10, 10, 2, 10));
  EXPECT_TRUE(right_half == Rect(12, 10, 3, 10));
}

TEST(RectTest, SplitHorizontally) {
  Rect top_half, bottom_half;

  // Splitting when origin is (0, 0).
  Rect(0, 0, 10, 20).SplitHorizontally(top_half, bottom_half);
  EXPECT_EQ(Rect(0, 0, 10, 10), top_half);
  EXPECT_EQ(Rect(0, 10, 10, 10), bottom_half);

  // Splitting when origin is arbitrary.
  Rect(10, 10, 10, 20).SplitHorizontally(top_half, bottom_half);
  EXPECT_EQ(Rect(10, 10, 10, 10), top_half);
  EXPECT_EQ(Rect(10, 20, 10, 10), bottom_half);

  // Splitting a rectangle of zero height.
  Rect(10, 10, 10, 0).SplitHorizontally(top_half, bottom_half);
  EXPECT_EQ(Rect(10, 10, 10, 0), top_half);
  EXPECT_EQ(Rect(10, 10, 10, 0), bottom_half);

  // Splitting a rectangle of odd height.
  Rect(10, 10, 10, 5).SplitHorizontally(top_half, bottom_half);
  EXPECT_EQ(Rect(10, 10, 10, 2), top_half);
  EXPECT_EQ(Rect(10, 12, 10, 3), bottom_half);
}

TEST(RectTest, CenterPoint) {
  Point center;

  // When origin is (0, 0).
  center = Rect(0, 0, 20, 20).CenterPoint();
  EXPECT_TRUE(center == Point(10, 10));

  // When origin is even.
  center = Rect(10, 10, 20, 20).CenterPoint();
  EXPECT_TRUE(center == Point(20, 20));

  // When origin is odd.
  center = Rect(11, 11, 20, 20).CenterPoint();
  EXPECT_TRUE(center == Point(21, 21));

  // When 0 width or height.
  center = Rect(10, 10, 0, 20).CenterPoint();
  EXPECT_TRUE(center == Point(10, 20));
  center = Rect(10, 10, 20, 0).CenterPoint();
  EXPECT_TRUE(center == Point(20, 10));

  // When an odd size.
  center = Rect(10, 10, 21, 21).CenterPoint();
  EXPECT_TRUE(center == Point(20, 20));

  // When an odd size and position.
  center = Rect(11, 11, 21, 21).CenterPoint();
  EXPECT_TRUE(center == Point(21, 21));
}

TEST(RectTest, SharesEdgeWith) {
  Rect r(2, 3, 4, 5);

  // Must be non-overlapping
  EXPECT_FALSE(r.SharesEdgeWith(r));

  Rect just_above(2, 1, 4, 2);
  Rect just_below(2, 8, 4, 2);
  Rect just_left(0, 3, 2, 5);
  Rect just_right(6, 3, 2, 5);

  EXPECT_TRUE(r.SharesEdgeWith(just_above));
  EXPECT_TRUE(r.SharesEdgeWith(just_below));
  EXPECT_TRUE(r.SharesEdgeWith(just_left));
  EXPECT_TRUE(r.SharesEdgeWith(just_right));

  // Wrong placement
  Rect same_height_no_edge(0, 0, 1, 5);
  Rect same_width_no_edge(0, 0, 4, 1);

  EXPECT_FALSE(r.SharesEdgeWith(same_height_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(same_width_no_edge));

  Rect just_above_no_edge(2, 1, 5, 2);  // too wide
  Rect just_below_no_edge(2, 8, 3, 2);  // too narrow
  Rect just_left_no_edge(0, 3, 2, 6);   // too tall
  Rect just_right_no_edge(6, 3, 2, 4);  // too short

  EXPECT_FALSE(r.SharesEdgeWith(just_above_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_below_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_left_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_right_no_edge));
}

static void TestScaleRectOverflowClamp(Rect (*function)(const Rect&,
                                                        float,
                                                        float)) {
  // The whole rect is scaled out of kMinInt.
  Rect xy_underflow1(-100000, -123456, 10, 20);
  EXPECT_EQ(Rect(kMinInt, kMinInt, 0, 0),
            function(xy_underflow1, 100000, 100000));

  // This rect's right/bottom is 0. The origin overflows, and is clamped to
  // -kMaxInt (instead of kMinInt) to keep width/height not overflowing.
  Rect xy_underflow2(-100000, -123456, 100000, 123456);
  EXPECT_EQ(Rect(-kMaxInt, -kMaxInt, kMaxInt, kMaxInt),
            function(xy_underflow2, 100000, 100000));

  // A location overflow means that width/right and bottom/top also
  // overflow so need to be clamped.
  Rect xy_overflow(100000, 123456, 10, 20);
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            function(xy_overflow, 100000, 100000));

  // In practice all rects are clamped to 0 width / 0 height so
  // negative sizes don't matter, but try this for the sake of testing.
  Rect size_underflow(-1, -2, 100000, 100000);
  EXPECT_EQ(Rect(100000, 200000, 0, 0),
            function(size_underflow, -100000, -100000));

  Rect size_overflow(-1, -2, 123456, 234567);
  EXPECT_EQ(Rect(-100000, -200000, kMaxInt, kMaxInt),
            function(size_overflow, 100000, 100000));
  // Verify width/right gets clamped properly too if x/y positive.
  Rect size_overflow2(1, 2, 123456, 234567);
  EXPECT_EQ(Rect(100000, 200000, kMaxInt - 100000, kMaxInt - 200000),
            function(size_overflow2, 100000, 100000));

  constexpr float kMaxIntAsFloat = static_cast<float>(kMaxInt);
  Rect max_origin_rect(kMaxInt, kMaxInt, kMaxInt, kMaxInt);
  // width/height of max_origin_rect has already been clamped to 0.
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0), max_origin_rect);
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            function(max_origin_rect, kMaxIntAsFloat, kMaxIntAsFloat));

  Rect max_size_rect1(0, 0, kMaxInt, kMaxInt);
  // Max sized rect can't be scaled up any further in any dimension.
  EXPECT_EQ(max_size_rect1, function(max_size_rect1, 2, 3.5));
  EXPECT_EQ(max_size_rect1,
            function(max_size_rect1, kMaxIntAsFloat, kMaxIntAsFloat));
  // Max sized ret scaled by negative scale is an empty rect.
  EXPECT_EQ(Rect(), function(max_size_rect1, kMinInt, kMinInt));

  Rect max_size_rect2(-kMaxInt, -kMaxInt, kMaxInt, kMaxInt);
  EXPECT_EQ(max_size_rect2, function(max_size_rect2, 2, 3.5));
  EXPECT_EQ(max_size_rect2,
            function(max_size_rect2, kMaxIntAsFloat, kMaxIntAsFloat));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            function(max_size_rect2, kMinInt, kMinInt));
}

TEST(RectTest, ScaleToEnclosedRect) {
  EXPECT_EQ(Rect(), ScaleToEnclosedRect(Rect(), 5.f));
  EXPECT_EQ(Rect(5, 5, 5, 5), ScaleToEnclosedRect(Rect(1, 1, 1, 1), 5.f));
  EXPECT_EQ(Rect(-5, -5, 0, 0), ScaleToEnclosedRect(Rect(-1, -1, 0, 0), 5.f));
  EXPECT_EQ(Rect(5, -5, 0, 5), ScaleToEnclosedRect(Rect(1, -1, 0, 1), 5.f));
  EXPECT_EQ(Rect(-5, 5, 5, 0), ScaleToEnclosedRect(Rect(-1, 1, 1, 0), 5.f));
  EXPECT_EQ(Rect(2, 3, 4, 6), ScaleToEnclosedRect(Rect(1, 2, 3, 4), 1.5f));
  EXPECT_EQ(Rect(-1, -3, 0, 0), ScaleToEnclosedRect(Rect(-1, -2, 0, 0), 1.5f));
  EXPECT_EQ(Rect(1, 2, 2, 1), ScaleToEnclosedRect(Rect(2, 4, 9, 8), 0.3f));
  TestScaleRectOverflowClamp(ScaleToEnclosedRect);
}

TEST(RectTest, ScaleToEnclosingRect) {
  EXPECT_EQ(Rect(), ScaleToEnclosingRect(Rect(), 5.f));
  EXPECT_EQ(Rect(5, 5, 5, 5), ScaleToEnclosingRect(Rect(1, 1, 1, 1), 5.f));
  EXPECT_EQ(Rect(-5, -5, 0, 0), ScaleToEnclosingRect(Rect(-1, -1, 0, 0), 5.f));
  EXPECT_EQ(Rect(5, -5, 0, 5), ScaleToEnclosingRect(Rect(1, -1, 0, 1), 5.f));
  EXPECT_EQ(Rect(-5, 5, 5, 0), ScaleToEnclosingRect(Rect(-1, 1, 1, 0), 5.f));
  EXPECT_EQ(Rect(1, 3, 5, 6), ScaleToEnclosingRect(Rect(1, 2, 3, 4), 1.5f));
  EXPECT_EQ(Rect(-2, -3, 0, 0), ScaleToEnclosingRect(Rect(-1, -2, 0, 0), 1.5f));
  EXPECT_EQ(Rect(0, 1, 4, 3), ScaleToEnclosingRect(Rect(2, 4, 9, 8), 0.3f));
  TestScaleRectOverflowClamp(ScaleToEnclosingRect);
}

TEST(RectTest, ScaleToRoundedRect) {
  EXPECT_EQ(Rect(), ScaleToRoundedRect(Rect(), 5.f));
  EXPECT_EQ(Rect(5, 5, 5, 5), ScaleToRoundedRect(Rect(1, 1, 1, 1), 5.f));
  EXPECT_EQ(Rect(-5, -5, 0, 0), ScaleToRoundedRect(Rect(-1, -1, 0, 0), 5.f));
  EXPECT_EQ(Rect(5, -5, 0, 5), ScaleToRoundedRect(Rect(1, -1, 0, 1), 5.f));
  EXPECT_EQ(Rect(-5, 5, 5, 0), ScaleToRoundedRect(Rect(-1, 1, 1, 0), 5.f));
  EXPECT_EQ(Rect(2, 3, 4, 6), ScaleToRoundedRect(Rect(1, 2, 3, 4), 1.5f));
  EXPECT_EQ(Rect(-2, -3, 0, 0), ScaleToRoundedRect(Rect(-1, -2, 0, 0), 1.5f));
  EXPECT_EQ(Rect(1, 1, 2, 3), ScaleToRoundedRect(Rect(2, 4, 9, 8), 0.3f));
  TestScaleRectOverflowClamp(ScaleToRoundedRect);
}

#if BUILDFLAG(IS_WIN)
TEST(RectTest, ConstructAndAssign) {
  const RECT rect_1 = { 0, 0, 10, 10 };
  const RECT rect_2 = { 0, 0, -10, -10 };
  Rect test1(rect_1);
  Rect test2(rect_2);
}
#endif

TEST(RectTest, BoundingRect) {
  struct {
    Point a;
    Point b;
    Rect expected;
  } int_tests[] = {
    // If point B dominates A, then A should be the origin.
    { Point(4, 6), Point(4, 6), Rect(4, 6, 0, 0) },
    { Point(4, 6), Point(8, 6), Rect(4, 6, 4, 0) },
    { Point(4, 6), Point(4, 9), Rect(4, 6, 0, 3) },
    { Point(4, 6), Point(8, 9), Rect(4, 6, 4, 3) },
    // If point A dominates B, then B should be the origin.
    { Point(4, 6), Point(4, 6), Rect(4, 6, 0, 0) },
    { Point(8, 6), Point(4, 6), Rect(4, 6, 4, 0) },
    { Point(4, 9), Point(4, 6), Rect(4, 6, 0, 3) },
    { Point(8, 9), Point(4, 6), Rect(4, 6, 4, 3) },
    // If neither point dominates, then the origin is a combination of the two.
    { Point(4, 6), Point(6, 4), Rect(4, 4, 2, 2) },
    { Point(-4, -6), Point(-6, -4), Rect(-6, -6, 2, 2) },
    { Point(-4, 6), Point(6, -4), Rect(-4, -4, 10, 10) },
  };

  for (size_t i = 0; i < std::size(int_tests); ++i) {
    Rect actual = BoundingRect(int_tests[i].a, int_tests[i].b);
    EXPECT_EQ(int_tests[i].expected, actual);
  }
}

TEST(RectTest, Offset) {
  Rect i(1, 2, 3, 4);

  EXPECT_EQ(Rect(2, 1, 3, 4), (i + Vector2d(1, -1)));
  EXPECT_EQ(Rect(2, 1, 3, 4), (Vector2d(1, -1) + i));
  i += Vector2d(1, -1);
  EXPECT_EQ(Rect(2, 1, 3, 4), i);
  EXPECT_EQ(Rect(1, 2, 3, 4), (i - Vector2d(1, -1)));
  i -= Vector2d(1, -1);
  EXPECT_EQ(Rect(1, 2, 3, 4), i);

  i.Offset(2, -2);
  EXPECT_EQ(Rect(3, 0, 3, 4), i);

  EXPECT_EQ(Rect(kMaxInt - 2, kMaxInt - 2, 2, 2),
            (Rect(0, 0, kMaxInt - 2, kMaxInt - 2) +
             Vector2d(kMaxInt - 2, kMaxInt - 2)));
  EXPECT_EQ(Rect(kMaxInt - 2, kMaxInt - 2, 2, 2),
            (Rect(0, 0, kMaxInt - 2, kMaxInt - 2) -
             Vector2d(2 - kMaxInt, 2 - kMaxInt)));
}

TEST(RectTest, Corners) {
  Rect i(1, 2, 3, 4);
  EXPECT_EQ(Point(1, 2), i.origin());
  EXPECT_EQ(Point(4, 2), i.top_right());
  EXPECT_EQ(Point(1, 6), i.bottom_left());
  EXPECT_EQ(Point(4, 6), i.bottom_right());
}

TEST(RectTest, Centers) {
  Rect i(10, 20, 30, 40);
  EXPECT_EQ(Point(10, 40), i.left_center());
  EXPECT_EQ(Point(25, 20), i.top_center());
  EXPECT_EQ(Point(40, 40), i.right_center());
  EXPECT_EQ(Point(25, 60), i.bottom_center());
}

TEST(RectTest, Transpose) {
  Rect i(10, 20, 30, 40);
  i.Transpose();
  EXPECT_EQ(Rect(20, 10, 40, 30), i);
}

TEST(RectTest, ManhattanDistanceToPoint) {
  Rect i(1, 2, 3, 4);
  EXPECT_EQ(0, i.ManhattanDistanceToPoint(Point(1, 2)));
  EXPECT_EQ(0, i.ManhattanDistanceToPoint(Point(4, 6)));
  EXPECT_EQ(0, i.ManhattanDistanceToPoint(Point(2, 4)));
  EXPECT_EQ(3, i.ManhattanDistanceToPoint(Point(0, 0)));
  EXPECT_EQ(2, i.ManhattanDistanceToPoint(Point(2, 0)));
  EXPECT_EQ(3, i.ManhattanDistanceToPoint(Point(5, 0)));
  EXPECT_EQ(1, i.ManhattanDistanceToPoint(Point(5, 4)));
  EXPECT_EQ(3, i.ManhattanDistanceToPoint(Point(5, 8)));
  EXPECT_EQ(2, i.ManhattanDistanceToPoint(Point(3, 8)));
  EXPECT_EQ(2, i.ManhattanDistanceToPoint(Point(0, 7)));
  EXPECT_EQ(1, i.ManhattanDistanceToPoint(Point(0, 3)));
}

TEST(RectTest, ManhattanInternalDistance) {
  Rect i(0, 0, 400, 400);
  EXPECT_EQ(0, i.ManhattanInternalDistance(Rect(-1, 0, 2, 1)));
  EXPECT_EQ(1, i.ManhattanInternalDistance(Rect(400, 0, 1, 400)));
  EXPECT_EQ(2, i.ManhattanInternalDistance(Rect(-100, -100, 100, 100)));
  EXPECT_EQ(2, i.ManhattanInternalDistance(Rect(-101, 100, 100, 100)));
  EXPECT_EQ(4, i.ManhattanInternalDistance(Rect(-101, -101, 100, 100)));
  EXPECT_EQ(435, i.ManhattanInternalDistance(Rect(630, 603, 100, 100)));
}

TEST(RectTest, IntegerOverflow) {
  int limit = std::numeric_limits<int>::max();
  int min_limit = std::numeric_limits<int>::min();
  int expected_thickness = 10;
  int large_number = limit - expected_thickness;

  Rect height_overflow(0, large_number, 100, 100);
  EXPECT_EQ(large_number, height_overflow.y());
  EXPECT_EQ(expected_thickness, height_overflow.height());

  Rect width_overflow(large_number, 0, 100, 100);
  EXPECT_EQ(large_number, width_overflow.x());
  EXPECT_EQ(expected_thickness, width_overflow.width());

  Rect size_height_overflow(Point(0, large_number), Size(100, 100));
  EXPECT_EQ(large_number, size_height_overflow.y());
  EXPECT_EQ(expected_thickness, size_height_overflow.height());

  Rect size_width_overflow(Point(large_number, 0), Size(100, 100));
  EXPECT_EQ(large_number, size_width_overflow.x());
  EXPECT_EQ(expected_thickness, size_width_overflow.width());

  Rect set_height_overflow(0, large_number, 100, 5);
  EXPECT_EQ(5, set_height_overflow.height());
  set_height_overflow.set_height(100);
  EXPECT_EQ(expected_thickness, set_height_overflow.height());

  Rect set_y_overflow(100, 100, 100, 100);
  EXPECT_EQ(100, set_y_overflow.height());
  set_y_overflow.set_y(large_number);
  EXPECT_EQ(expected_thickness, set_y_overflow.height());

  Rect set_width_overflow(large_number, 0, 5, 100);
  EXPECT_EQ(5, set_width_overflow.width());
  set_width_overflow.set_width(100);
  EXPECT_EQ(expected_thickness, set_width_overflow.width());

  Rect set_x_overflow(100, 100, 100, 100);
  EXPECT_EQ(100, set_x_overflow.width());
  set_x_overflow.set_x(large_number);
  EXPECT_EQ(expected_thickness, set_x_overflow.width());

  Point large_offset(large_number, large_number);
  Size size(100, 100);
  Size expected_size(10, 10);

  Rect set_origin_overflow(100, 100, 100, 100);
  EXPECT_EQ(size, set_origin_overflow.size());
  set_origin_overflow.set_origin(large_offset);
  EXPECT_EQ(large_offset, set_origin_overflow.origin());
  EXPECT_EQ(expected_size, set_origin_overflow.size());

  Rect set_size_overflow(large_number, large_number, 5, 5);
  EXPECT_EQ(Size(5, 5), set_size_overflow.size());
  set_size_overflow.set_size(size);
  EXPECT_EQ(large_offset, set_size_overflow.origin());
  EXPECT_EQ(expected_size, set_size_overflow.size());

  Rect set_rect_overflow;
  set_rect_overflow.SetRect(large_number, large_number, 100, 100);
  EXPECT_EQ(large_offset, set_rect_overflow.origin());
  EXPECT_EQ(expected_size, set_rect_overflow.size());

  // Insetting an empty rect, but the total inset (left + right) could overflow.
  Rect inset_overflow;
  inset_overflow.Inset(Insets::TLBR(large_number, large_number, 100, 100));
  EXPECT_EQ(large_offset, inset_overflow.origin());
  EXPECT_EQ(Size(), inset_overflow.size());

  // Insetting where the total inset (width - left - right) could overflow.
  // Also, this insetting by the min limit in all directions cannot
  // represent width() without overflow, so that will also clamp.
  Rect inset_overflow2;
  inset_overflow2.Inset(min_limit);
  EXPECT_EQ(inset_overflow2, Rect(min_limit, min_limit, limit, limit));

  // Insetting where the width shouldn't change, but if the insets operations
  // clamped in the wrong order, e.g. ((width - left) - right) vs (width - (left
  // + right)) then this will not work properly.  This is the proper order,
  // as if left + right overflows, the width cannot be decreased by more than
  // max int anyway.  Additionally, if left + right underflows, it cannot be
  // increased by more then max int.
  Rect inset_overflow3(0, 0, limit, limit);
  inset_overflow3.Inset(Insets::TLBR(-100, -100, 100, 100));
  EXPECT_EQ(inset_overflow3, Rect(-100, -100, limit, limit));

  Rect inset_overflow4(-1000, -1000, limit, limit);
  inset_overflow4.Inset(Insets::TLBR(100, 100, -100, -100));
  EXPECT_EQ(inset_overflow4, Rect(-900, -900, limit, limit));

  Rect offset_overflow(0, 0, 100, 100);
  offset_overflow.Offset(large_number, large_number);
  EXPECT_EQ(large_offset, offset_overflow.origin());
  EXPECT_EQ(expected_size, offset_overflow.size());

  Rect operator_overflow(0, 0, 100, 100);
  operator_overflow += Vector2d(large_number, large_number);
  EXPECT_EQ(large_offset, operator_overflow.origin());
  EXPECT_EQ(expected_size, operator_overflow.size());

  Rect origin_maxint(limit, limit, limit, limit);
  EXPECT_EQ(origin_maxint, Rect(Point(limit, limit), Size()));

  // Expect a rect at the origin and a rect whose right/bottom is maxint
  // create a rect that extends from 0..maxint in both extents.
  {
    Rect origin_small(0, 0, 100, 100);
    Rect big_clamped(50, 50, limit, limit);
    EXPECT_EQ(big_clamped.right(), limit);

    Rect unioned = UnionRects(origin_small, big_clamped);
    Rect rect_limit(0, 0, limit, limit);
    EXPECT_EQ(unioned, rect_limit);
  }

  // Expect a rect that would overflow width (but not right) to be clamped
  // and to have maxint extents after unioning.
  {
    Rect small(-500, -400, 100, 100);
    Rect big(-400, -500, limit, limit);
    // Technically, this should be limit + 100 width, but will clamp to maxint.
    EXPECT_EQ(UnionRects(small, big), Rect(-500, -500, limit, limit));
  }

  // Expect a rect that would overflow right *and* width to be clamped.
  {
    Rect clamped(500, 500, limit, limit);
    Rect positive_origin(100, 100, 500, 500);

    // Ideally, this should be (100, 100, limit + 400, limit + 400).
    // However, width overflows and would be clamped to limit, but right
    // overflows too and so will be clamped to limit - 100.
    Rect expected_rect(100, 100, limit - 100, limit - 100);
    EXPECT_EQ(UnionRects(clamped, positive_origin), expected_rect);
  }

  // Unioning a left=minint rect with a right=maxint rect.
  // We can't represent both ends of the spectrum in the same rect.
  // Make sure we keep the most useful area.
  {
    int part_limit = min_limit / 3;
    Rect left_minint(min_limit, min_limit, 1, 1);
    Rect right_maxint(limit - 1, limit - 1, limit, limit);
    Rect expected_rect(part_limit, part_limit, 2 * part_limit, 2 * part_limit);
    Rect result = UnionRects(left_minint, right_maxint);

    // The result should be maximally big.
    EXPECT_EQ(limit, result.height());
    EXPECT_EQ(limit, result.width());

    // The result should include the area near the origin.
    EXPECT_GT(-part_limit, result.x());
    EXPECT_LT(part_limit, result.right());
    EXPECT_GT(-part_limit, result.y());
    EXPECT_LT(part_limit, result.bottom());

    // More succinctly, but harder to read in the results.
    EXPECT_TRUE(UnionRects(left_minint, right_maxint).Contains(expected_rect));
  }
}

TEST(RectTest, Inset) {
  Rect r(10, 20, 30, 40);
  r.Inset(0);
  EXPECT_EQ(Rect(10, 20, 30, 40), r);
  r.Inset(1);
  EXPECT_EQ(Rect(11, 21, 28, 38), r);
  r.Inset(-1);
  EXPECT_EQ(Rect(10, 20, 30, 40), r);

  r.Inset(Insets::VH(2, 1));
  EXPECT_EQ(Rect(11, 22, 28, 36), r);
  r.Inset(Insets::VH(-2, -1));
  EXPECT_EQ(Rect(10, 20, 30, 40), r);

  // The parameters are left, top, right, bottom.
  r.Inset(Insets::TLBR(2, 1, 4, 3));
  EXPECT_EQ(Rect(11, 22, 26, 34), r);
  r.Inset(Insets::TLBR(-2, -1, -4, -3));
  EXPECT_EQ(Rect(10, 20, 30, 40), r);

  r.Inset(Insets::TLBR(1, 2, 3, 4));
  EXPECT_EQ(Rect(12, 21, 24, 36), r);
  r.Inset(Insets::TLBR(-1, -2, -3, -4));
  EXPECT_EQ(Rect(10, 20, 30, 40), r);
}

TEST(RectTest, Outset) {
  Rect r(10, 20, 30, 40);
  r.Outset(0);
  EXPECT_EQ(Rect(10, 20, 30, 40), r);
  r.Outset(1);
  EXPECT_EQ(Rect(9, 19, 32, 42), r);
  r.Outset(-1);
  EXPECT_EQ(Rect(10, 20, 30, 40), r);

  r.Outset(Outsets::VH(2, 1));
  EXPECT_EQ(Rect(9, 18, 32, 44), r);
  r.Outset(Outsets::VH(-2, -1));
  EXPECT_EQ(Rect(10, 20, 30, 40), r);

  r.Outset(Outsets::TLBR(2, 1, 4, 3));
  EXPECT_EQ(Rect(9, 18, 34, 46), r);
  r.Outset(Outsets::TLBR(-2, -1, -4, -3));
  EXPECT_EQ(Rect(10, 20, 30, 40), r);
}

TEST(RectTest, InsetOutsetClamped) {
  Rect r(10, 20, 30, 40);
  r.Inset(18);
  EXPECT_EQ(Rect(28, 38, 0, 4), r);
  r.Inset(-18);
  EXPECT_EQ(Rect(10, 20, 36, 40), r);

  r.Inset(Insets::VH(30, 15));
  EXPECT_EQ(Rect(25, 50, 6, 0), r);
  r.Inset(Insets::VH(-30, -15));
  EXPECT_EQ(Rect(10, 20, 36, 60), r);

  r.Inset(Insets::TLBR(30, 20, 50, 40));
  EXPECT_EQ(Rect(30, 50, 0, 0), r);
  r.Inset(Insets::TLBR(-30, -20, -50, -40));
  EXPECT_EQ(Rect(10, 20, 60, 80), r);

  r.Outset(kMaxInt);
  EXPECT_EQ(Rect(10 - kMaxInt, 20 - kMaxInt, kMaxInt, kMaxInt), r);
  r.Outset(Outsets().set_top_bottom(kMaxInt, kMaxInt));
  EXPECT_EQ(Rect(10 - kMaxInt, kMinInt, kMaxInt, kMaxInt), r);
  r.Outset(Outsets().set_right(kMaxInt).set_top(kMaxInt));
  EXPECT_EQ(Rect(10 - kMaxInt, kMinInt, kMaxInt, kMaxInt), r);
  r.Outset(Outsets().set_left_right(kMaxInt, kMaxInt));
  EXPECT_EQ(Rect(kMinInt, kMinInt, kMaxInt, kMaxInt), r);
}

TEST(RectTest, SetByBounds) {
  Rect r;
  r.SetByBounds(1, 2, 30, 40);
  EXPECT_EQ(Rect(1, 2, 29, 38), r);
  r.SetByBounds(30, 40, 1, 2);
  EXPECT_EQ(Rect(30, 40, 0, 0), r);

  r.SetByBounds(0, 0, kMaxInt, kMaxInt);
  EXPECT_EQ(Rect(0, 0, kMaxInt, kMaxInt), r);
  r.SetByBounds(-1, -1, kMaxInt, kMaxInt);
  EXPECT_EQ(Rect(-1, -1, kMaxInt, kMaxInt), r);
  r.SetByBounds(1, 1, kMaxInt, kMaxInt);
  EXPECT_EQ(Rect(1, 1, kMaxInt - 1, kMaxInt - 1), r);
  r.SetByBounds(kMinInt, kMinInt, 0, 0);
  EXPECT_EQ(Rect(kMinInt + 1, kMinInt + 1, kMaxInt, kMaxInt), r);
  r.SetByBounds(kMinInt, kMinInt, 1, 1);
  EXPECT_EQ(Rect(kMinInt + 2, kMinInt + 2, kMaxInt, kMaxInt), r);
  r.SetByBounds(kMinInt, kMinInt, -1, -1);
  EXPECT_EQ(Rect(kMinInt, kMinInt, kMaxInt, kMaxInt), r);
  r.SetByBounds(kMinInt, kMinInt, kMaxInt, kMaxInt);
  EXPECT_EQ(Rect(kMinInt / 2 - 1, kMinInt / 2 - 1, kMaxInt, kMaxInt), r);
}

TEST(RectTest, MaximumCoveredRect) {
  // X aligned and intersect: unite.
  EXPECT_EQ(Rect(10, 20, 30, 60),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(10, 30, 30, 50)));
  // X aligned and adjacent: unite.
  EXPECT_EQ(Rect(10, 20, 30, 90),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(10, 60, 30, 50)));
  // X aligned and separate: choose the bigger one.
  EXPECT_EQ(Rect(10, 61, 30, 50),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(10, 61, 30, 50)));
  // Y aligned and intersect: unite.
  EXPECT_EQ(Rect(10, 20, 60, 40),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(30, 20, 40, 40)));
  // Y aligned and adjacent: unite.
  EXPECT_EQ(Rect(10, 20, 70, 40),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(40, 20, 40, 40)));
  // Y aligned and separate: choose the bigger one.
  EXPECT_EQ(Rect(41, 20, 40, 40),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(41, 20, 40, 40)));
  // Get the biggest expanded intersection.
  EXPECT_EQ(Rect(0, 0, 9, 19),
            MaximumCoveredRect(Rect(0, 0, 10, 10), Rect(0, 9, 9, 10)));
  EXPECT_EQ(Rect(0, 0, 19, 9),
            MaximumCoveredRect(Rect(0, 0, 10, 10), Rect(9, 0, 10, 9)));
  // Otherwise choose the bigger one.
  EXPECT_EQ(Rect(20, 30, 40, 50),
            MaximumCoveredRect(Rect(10, 20, 30, 40), Rect(20, 30, 40, 50)));
  EXPECT_EQ(Rect(10, 20, 40, 50),
            MaximumCoveredRect(Rect(10, 20, 40, 50), Rect(20, 30, 30, 40)));
  EXPECT_EQ(Rect(10, 20, 40, 50),
            MaximumCoveredRect(Rect(10, 20, 40, 50), Rect(20, 30, 40, 50)));
}

}  // namespace gfx
