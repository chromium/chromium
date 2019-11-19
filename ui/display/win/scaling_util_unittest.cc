// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/scaling_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/win/test/screen_util_win.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace win {
namespace test {
namespace {

const wchar_t kFakeDisplayName[] = L"Fake Display";

DisplayInfo CreateDisplayInfo(int x, int y, int width, int height,
                              float scale_factor) {
  MONITORINFOEX monitor_info = CreateMonitorInfo(gfx::Rect(x, y, width, height),
                                                 gfx::Rect(x, y, width, height),
                                                 kFakeDisplayName);
  return DisplayInfo(monitor_info, scale_factor, 1.0f, Display::ROTATE_0, 60,
                     gfx::Vector2dF());
}

::testing::AssertionResult AssertOffsetsEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const DisplayPlacement& lhs,
    const DisplayPlacement& rhs) {
  if (lhs.position == rhs.position &&
      lhs.offset == rhs.offset &&
      lhs.offset_reference == rhs.offset_reference) {
    return ::testing::AssertionSuccess();
  }

  return ::testing::AssertionFailure() <<
      "Value of: " << rhs_expr << "\n  Actual: " << rhs.ToString() <<
      "\nExpected: " << lhs_expr << "\nWhich is: " << lhs.ToString();
}

#define EXPECT_OFFSET_EQ(a, b) \
  EXPECT_PRED_FORMAT2(AssertOffsetsEqual, a, b);

TEST(ScalingUtilTest, DisplayInfosTouchBottom) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(1, 2, 1, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(0, 2, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(1, 2, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(2, 2, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(3, 2, 2, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchLeft) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(0, 1, 1, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(0, 0, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(0, 1, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(0, 2, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(0, 3, 1, 2, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchTop) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(1, 0, 1, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(0, 0, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(1, 0, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(2, 0, 2, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 3, 1, 1.0f),
                                CreateDisplayInfo(3, 0, 2, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchRight) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(2, 1, 1, 1, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(2, 0, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(2, 1, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(2, 2, 1, 2, 1.0f)));
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 3, 1.0f),
                                CreateDisplayInfo(2, 3, 1, 2, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchBottomRightCorner) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(2, 2, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchBottomLeftCorner) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(0, 2, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchUpperLeftCorner) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(0, 0, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchUpperRightCorner) {
  EXPECT_TRUE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                CreateDisplayInfo(2, 0, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchNone) {
  EXPECT_FALSE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                 CreateDisplayInfo(3, 1, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchNoneShareXAxis) {
  EXPECT_FALSE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                 CreateDisplayInfo(5, 2, 1, 1, 1.0f)));

  EXPECT_FALSE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                 CreateDisplayInfo(-2, 1, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, DisplayInfosTouchNoneShareYAxis) {
  EXPECT_FALSE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                 CreateDisplayInfo(2, 3, 1, 1, 1.0f)));

  EXPECT_FALSE(DisplayInfosTouch(CreateDisplayInfo(1, 1, 1, 1, 1.0f),
                                 CreateDisplayInfo(0, -1, 1, 1, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacementNoScaleRight) {
  // Top edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT, 0, DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(800, 0, 1024, 768, 1.0f)));

  // Bottom edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::BOTTOM_RIGHT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(800, -168, 1024, 768, 1.0f)));

  // Offset to the top
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       -10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(800, -10, 1024, 768, 1.0f)));

  // Offset to the bottom.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(800, 10, 1024, 768, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacementNoScaleLeft) {
  // Top edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(-1024, 0, 1024, 768, 1.0f)));

  // Bottom edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::BOTTOM_RIGHT),
      CalculateDisplayPlacement(
          CreateDisplayInfo(0, 0, 800, 600, 1.0f),
          CreateDisplayInfo(-1024, -168, 1024, 768, 1.0f)));

  // Offset to the top.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       -10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(
          CreateDisplayInfo(0, 0, 800, 600, 1.0f),
          CreateDisplayInfo(-1024, -10, 1024, 768, 1.0f)));

  // Offset to the bottom.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(-1024, 10, 1024, 768, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacementNoScaleTop) {
  // Left edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(0, -768, 1024, 768, 1.0f)));

  // Right edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       0,
                       DisplayPlacement::BOTTOM_RIGHT),
      CalculateDisplayPlacement(
          CreateDisplayInfo(0, 0, 800, 600, 1.0f),
          CreateDisplayInfo(-224, -768, 1024, 768, 1.0f)));

  // Offset to the right.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(10, -768, 1024, 768, 1.0f)));

  // Offset to the left.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       -10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(-10, -768, 1024, 768, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacementNoScaleBottom) {
  // Left edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(0, 600, 1024, 768, 1.0f)));

  // Right edge aligned.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       0,
                       DisplayPlacement::BOTTOM_RIGHT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(-224, 600, 1024, 768, 1.0f)));

  // Offset to the right
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(10, 600, 1024, 768, 1.0f)));

  // Offset to the left
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       -10,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(0, 0, 800, 600, 1.0f),
                                CreateDisplayInfo(-10, 600, 1024, 768, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacementNoScaleOddDimensions) {
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       5,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(35, 72, 24, 24, 1.0f),
                                CreateDisplayInfo(59, 77, 7, 9, 1.0f)));

  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       2,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(1, 7, 30, 40, 1.0f),
                                CreateDisplayInfo(-701, 9, 702, 2, 1.0f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacement1_5xScale) {
  // Side by side to the right.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(900, 50, 1000, 700, 1.5f)));

  // Side-by-side to the left.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(-900, 50, 1000, 700, 1.5f)));

  // Side-by-side to the top.
  // Note that -33 would be the normal enclosing rect offset.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       -34,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(50, -650, 1000, 700, 1.5f)));

  // Side-by-side on the bottom.
  // Note that -33 would be the normal enclosing rect offset.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       -34,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(50, 650, 1000, 700, 1.5f)));

  // Side by side to the right.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(900, 50, 1000, 700, 1.5f)));

  // Side-by-side to the left.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(-900, 50, 1000, 700, 1.5f)));

  // Side-by-side to the top.
  // Note that -33 would be the normal enclosing rect offset.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       -34,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(50, -650, 1000, 700, 1.5f)));


  // Side-by-side to the bottom.
  // Note that -33 would be the normal enclosing rect offset.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       -34,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(50, 650, 1000, 700, 1.5f)));
}

TEST(ScalingUtilTest, CalculateDisplayPlacement2xScale) {
  // Side by side to the right.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(900, 50, 1000, 700, 2.0f)));

  // Side-by-side to the left.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(-900, 50, 1000, 700, 2.0f)));

  // Side-by-side to the top.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       -25,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(50, -650, 1000, 700, 2.0f)));

  // Side-by-side on the bottom.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       -25,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 1.0f),
                                CreateDisplayInfo(50, 650, 1000, 700, 2.0f)));

  // Side by side to the right.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::RIGHT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(900, 50, 1000, 700, 2.0f)));

  // Side-by-side to the left.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::LEFT,
                       0,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(-900, 50, 1000, 700, 2.0f)));

  // Side-by-side to the top.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::TOP,
                       -25,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(50, -650, 1000, 700, 2.0f)));


  // Side-by-side to the bottom.
  EXPECT_OFFSET_EQ(
      DisplayPlacement(DisplayPlacement::BOTTOM,
                       -25,
                       DisplayPlacement::TOP_LEFT),
      CalculateDisplayPlacement(CreateDisplayInfo(100, 50, 800, 600, 2.0f),
                                CreateDisplayInfo(50, 650, 1000, 700, 2.0f)));
}

TEST(ScalingUtilTest, SquaredDistanceBetweenRectsFullyIntersecting) {
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(5, 5, 10, 10);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(rect1, rect2));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(rect2, rect1));
}

TEST(ScalingUtilTest, SquaredDistanceBetweenRectsPartiallyIntersecting) {
  gfx::Rect rect1(0, 0, 10, 10);
  gfx::Rect rect2(5, 5, 10, 10);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(rect1, rect2));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(rect2, rect1));
}

TEST(ScalingUtilTest, SquaredDistanceBetweenRectsTouching) {
  gfx::Rect ref(2, 2, 2, 2);

  gfx::Rect top_left(0, 0, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top_left));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top_left, ref));
  gfx::Rect top_left_partial_top(1, 0, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top_left_partial_top));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top_left_partial_top, ref));
  gfx::Rect top(2, 0, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top, ref));
  gfx::Rect top_right_partial_top(3, 0, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top_right_partial_top));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top_right_partial_top, ref));
  gfx::Rect top_right(4, 0, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top_right));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top_right, ref));

  gfx::Rect top_left_partial_left(0, 1, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, top_left_partial_left));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(top_left_partial_left, ref));
  gfx::Rect left(0, 2, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, left));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(left, ref));
  gfx::Rect bottom_left_partial(0, 3, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, bottom_left_partial));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(bottom_left_partial, ref));
  gfx::Rect bottom_left(0, 4, 2, 2);
  EXPECT_EQ(0, SquaredDistanceBetweenRects(ref, bottom_left));
  EXPECT_EQ(0, SquaredDistanceBetweenRects(bottom_left, ref));
}

TEST(ScalingUtilTest, SquaredDistanceBetweenRectsOverlapping) {
  gfx::Rect ref(5, 5, 2, 2);

  gfx::Rect top_left_partial_top(4, 0, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, top_left_partial_top));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(top_left_partial_top, ref));
  gfx::Rect top(5, 0, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, top));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(top, ref));
  gfx::Rect top_right_partial(6, 0, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, top_right_partial));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(top_right_partial, ref));

  gfx::Rect top_left_partial_left(0, 4, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, top_left_partial_left));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(top_left_partial_left, ref));
  gfx::Rect left(0, 5, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, left));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(left, ref));
  gfx::Rect bottom_left_partial(0, 6, 2, 2);
  EXPECT_EQ(9, SquaredDistanceBetweenRects(ref, bottom_left_partial));
  EXPECT_EQ(9, SquaredDistanceBetweenRects(bottom_left_partial, ref));
}

TEST(ScalingUtilTest, SquaredDistanceBetweenRectsDiagonals) {
  gfx::Rect ref(5, 5, 2, 2);

  gfx::Rect top_left(0, 0, 2, 2);
  EXPECT_EQ(18, SquaredDistanceBetweenRects(ref, top_left));
  EXPECT_EQ(18, SquaredDistanceBetweenRects(top_left, ref));

  gfx::Rect top_right(10, 0, 2, 2);
  EXPECT_EQ(18, SquaredDistanceBetweenRects(ref, top_right));
  EXPECT_EQ(18, SquaredDistanceBetweenRects(top_right, ref));
}

}  // namespace
}  // namespace test
}  // namespace win
}  // namespace display
