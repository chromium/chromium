// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"

namespace ui {
namespace {

void VerifyPoint(SkPoint a, SkPoint b) {
  EXPECT_EQ(a.x(), b.x());
  EXPECT_EQ(a.y(), b.y());
}

void VerifyTriangle(SkPath actualPath, SkPoint p0, SkPoint p1, SkPoint p2) {
  EXPECT_EQ(3, actualPath.countPoints());
  VerifyPoint(p0, actualPath.getPoint(0));
  VerifyPoint(p1, actualPath.getPoint(1));
  VerifyPoint(p2, actualPath.getPoint(2));
}

}  // namespace

class NativeThemeAuraTest : public testing::Test {
 protected:
  NativeThemeAuraTest() = default;

  SkPath PathForArrow(const gfx::Rect& rect,
                      NativeTheme::Part direction) const {
    return theme_.PathForArrow(rect, direction);
  }

  gfx::Rect BoundingRectForArrow(const gfx::Rect& rect) const {
    return theme_.BoundingRectForArrow(rect);
  }

 private:
  NativeThemeAura theme_{false, false};
};

TEST_F(NativeThemeAuraTest, VerticalArrows) {
  SkPath path;

  // Up arrow, sized for 1x.
  path =
      PathForArrow(gfx::Rect(100, 200, 17, 17), NativeTheme::kScrollbarUpArrow);
  VerifyTriangle(path, SkPoint::Make(105, 211), SkPoint::Make(112, 211),
                 SkPoint::Make(108.5, 207));

  // 1.25x, should be larger.
  path =
      PathForArrow(gfx::Rect(50, 70, 21, 21), NativeTheme::kScrollbarUpArrow);
  VerifyTriangle(path, SkPoint::Make(56, 84), SkPoint::Make(65, 84),
                 SkPoint::Make(60.5, 79));

  // Down arrow is just a flipped up arrow.
  path =
      PathForArrow(gfx::Rect(20, 80, 17, 17), NativeTheme::kScrollbarDownArrow);
  VerifyTriangle(path, SkPoint::Make(25, 86), SkPoint::Make(32, 86),
                 SkPoint::Make(28.5, 90));
}

TEST_F(NativeThemeAuraTest, HorizontalArrows) {
  SkPath path;

  // Right arrow, sized for 1x.
  path = PathForArrow(gfx::Rect(100, 200, 17, 17),
                      NativeTheme::kScrollbarRightArrow);
  VerifyTriangle(path, SkPoint::Make(107, 205), SkPoint::Make(107, 212),
                 SkPoint::Make(111, 208.5));

  // Button size for 1.25x, should be larger.
  path = PathForArrow(gfx::Rect(50, 70, 21, 21),
                      NativeTheme::kScrollbarRightArrow);
  VerifyTriangle(path, SkPoint::Make(58, 76), SkPoint::Make(58, 85),
                 SkPoint::Make(63, 80.5));

  // Left arrow is just a flipped right arrow.
  path =
      PathForArrow(gfx::Rect(20, 80, 17, 17), NativeTheme::kScrollbarLeftArrow);
  VerifyTriangle(path, SkPoint::Make(30, 85), SkPoint::Make(30, 92),
                 SkPoint::Make(26, 88.5));
}

TEST_F(NativeThemeAuraTest, ArrowForNonSquareButton) {
  SkPath path =
      PathForArrow(gfx::Rect(90, 80, 42, 37), NativeTheme::kScrollbarLeftArrow);
  VerifyTriangle(path, SkPoint::Make(116, 89), SkPoint::Make(116, 109),
                 SkPoint::Make(105, 99));
}

TEST_F(NativeThemeAuraTest, BoundingRectSquare) {
  gfx::Rect bounding_rect = BoundingRectForArrow(gfx::Rect(42, 61, 21, 21));
  EXPECT_EQ(48.f, bounding_rect.x());
  EXPECT_EQ(67.f, bounding_rect.y());
  EXPECT_EQ(9.f, bounding_rect.width());
  EXPECT_EQ(bounding_rect.width(), bounding_rect.height());
}

TEST_F(NativeThemeAuraTest, BoundingRectSlightlyRectangular) {
  // Stretched horzontally.
  gfx::Rect bounding_rect = BoundingRectForArrow(gfx::Rect(42, 61, 25, 20));
  EXPECT_EQ(49.f, bounding_rect.x());
  EXPECT_EQ(66.f, bounding_rect.y());
  EXPECT_EQ(11.f, bounding_rect.width());
  EXPECT_EQ(bounding_rect.width(), bounding_rect.height());

  // Stretched vertically.
  bounding_rect = BoundingRectForArrow(gfx::Rect(42, 61, 14, 10));
  EXPECT_EQ(46.f, bounding_rect.x());
  EXPECT_EQ(63.f, bounding_rect.y());
  EXPECT_EQ(6.f, bounding_rect.width());
  EXPECT_EQ(bounding_rect.width(), bounding_rect.height());
}

TEST_F(NativeThemeAuraTest, BoundingRectVeryRectangular) {
  // Stretched horzontally.
  gfx::Rect bounding_rect = BoundingRectForArrow(gfx::Rect(42, 61, 30, 8));
  EXPECT_EQ(53.f, bounding_rect.x());
  EXPECT_EQ(61.f, bounding_rect.y());
  EXPECT_EQ(8.f, bounding_rect.width());
  EXPECT_EQ(bounding_rect.width(), bounding_rect.height());

  // Stretched vertically.
  bounding_rect = BoundingRectForArrow(gfx::Rect(42, 61, 6, 44));
  EXPECT_EQ(42.f, bounding_rect.x());
  EXPECT_EQ(80.f, bounding_rect.y());
  EXPECT_EQ(6.f, bounding_rect.width());
  EXPECT_EQ(bounding_rect.width(), bounding_rect.height());
}

TEST_F(NativeThemeAuraTest, BoundingRectSnappedToWholePixels) {
  gfx::Rect bounding_rect = BoundingRectForArrow(gfx::Rect(0, 0, 9, 10));
  EXPECT_EQ(3.f, bounding_rect.x());

  bounding_rect = BoundingRectForArrow(gfx::Rect(0, 0, 10, 9));
  EXPECT_EQ(3.f, bounding_rect.y());
}

}  // namespace ui
