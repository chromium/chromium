// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_resize_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace views {

namespace {
// Aspect ratio is defined by width / height.
constexpr float kAspectRatioSquare = 1.0f;
constexpr float kAspectRatioHorizontal = 2.0f;
constexpr float kAspectRatioVertical = 0.5f;

const gfx::Size kMinSizeSquare = gfx::Size(10, 10);
const gfx::Size kMaxSizeSquare = gfx::Size(50, 50);

const gfx::Size kMinSizeHorizontal = gfx::Size(20, 10);
const gfx::Size kMaxSizeHorizontal = gfx::Size(50, 25);

const gfx::Size kMinSizeVertical = gfx::Size(10, 20);
const gfx::Size kMaxSizeVertical = gfx::Size(25, 50);
}  // namespace

// Tests resizing of window with a 1:1 aspect ratio. This test also tests the
// 'pivot points' when resizing, i.e. the opposite side or corner of the
// window.
TEST(WindowResizeUtilsTest, SizeToSquareAspectRatio) {
  // Size from the top of the window.
  // |window_rect| within the bounds of kMinSizeSquare and kMaxSizeSquare.
  gfx::Rect window_rect(100, 100, 15, 15);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kTop, kAspectRatioSquare,
                                           kMinSizeSquare, kMaxSizeSquare,
                                           &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, 15, 15));

  // Size from the bottom right corner of the window.
  // |window_rect| smaller than kMinSizeSquare.
  window_rect.SetRect(100, 100, 5, 5);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kBottomRight,
                                           kAspectRatioSquare, kMinSizeSquare,
                                           kMaxSizeSquare, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMinSizeSquare.width(),
                                   kMinSizeSquare.height()));

  // Size from the top of the window.
  // |window_rect| larger than kMaxSizeSquare.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kTop, kAspectRatioSquare,
                                           kMinSizeSquare, kMaxSizeSquare,
                                           &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 150, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the bottom of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kBottom, kAspectRatioSquare,
                                           kMinSizeSquare, kMaxSizeSquare,
                                           &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the left of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kLeft, kAspectRatioSquare,
                                           kMinSizeSquare, kMaxSizeSquare,
                                           &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(150, 150, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the right of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kRight, kAspectRatioSquare,
                                           kMinSizeSquare, kMaxSizeSquare,
                                           &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the top left corner of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kTopLeft,
                                           kAspectRatioSquare, kMinSizeSquare,
                                           kMaxSizeSquare, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(150, 150, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the top right corner of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kTopRight,
                                           kAspectRatioSquare, kMinSizeSquare,
                                           kMaxSizeSquare, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 150, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));

  // Size from the bottom left corner of the window.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(HitTest::kBottomLeft,
                                           kAspectRatioSquare, kMinSizeSquare,
                                           kMaxSizeSquare, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(150, 100, kMaxSizeSquare.width(),
                                   kMaxSizeSquare.height()));
}

// Tests the aspect ratio of the gfx::Rect adheres to the horizontal aspect
// ratio.
TEST(WindowResizeUtilsTest, SizeToHorizontalAspectRatio) {
  // |window_rect| within bounds of kMinSizeHorizontal and kMaxSizeHorizontal.
  gfx::Rect window_rect(100, 100, 20, 10);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kTop, kAspectRatioHorizontal, kMinSizeHorizontal,
      kMaxSizeHorizontal, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, 20, 10));

  // |window_rect| smaller than kMinSizeHorizontal.
  window_rect.SetRect(100, 100, 5, 5);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kBottomRight, kAspectRatioHorizontal, kMinSizeHorizontal,
      kMaxSizeHorizontal, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMinSizeHorizontal.width(),
                                   kMinSizeHorizontal.height()));

  // |window_rect| greater than kMaxSizeHorizontal.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kTop, kAspectRatioHorizontal, kMinSizeHorizontal,
      kMaxSizeHorizontal, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 175, kMaxSizeHorizontal.width(),
                                   kMaxSizeHorizontal.height()));
}

// Tests the aspect ratio of the gfx::Rect adheres to the vertical aspect ratio.
TEST(WindowResizeUtilsTest, SizeToVerticalAspectRatio) {
  // |window_rect| within bounds of kMinSizeVertical and kMaxSizeVertical.
  gfx::Rect window_rect(100, 100, 10, 20);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kBottomRight, kAspectRatioVertical, kMinSizeVertical,
      kMaxSizeVertical, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, 10, 20));

  // |window_rect| smaller than kMinSizeVertical.
  window_rect.SetRect(100, 100, 5, 5);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kBottomRight, kAspectRatioVertical, kMinSizeVertical,
      kMaxSizeVertical, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMinSizeVertical.width(),
                                   kMinSizeVertical.height()));

  // |window_rect| greater than kMaxSizeVertical.
  window_rect.SetRect(100, 100, 100, 100);
  WindowResizeUtils::SizeRectToAspectRatio(
      HitTest::kBottomRight, kAspectRatioVertical, kMinSizeVertical,
      kMaxSizeVertical, &window_rect);
  EXPECT_EQ(window_rect, gfx::Rect(100, 100, kMaxSizeVertical.width(),
                                   kMaxSizeVertical.height()));
}

}  // namespace views
