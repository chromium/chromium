// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/rect_based_targeting_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace views {

TEST(RectBasedTargetingUtils, UsePointBasedTargeting) {
  gfx::Rect rect_1(gfx::Point(-22, 30), gfx::Size(1, 1));
  gfx::Rect rect_2(gfx::Point(0, 0), gfx::Size(34, 55));
  gfx::Rect rect_3(gfx::Point(12, 12), gfx::Size(1, 0));
  gfx::Rect rect_4(gfx::Point(12, 120), gfx::Size(0, 0));

  EXPECT_TRUE(UsePointBasedTargeting(rect_1));
  EXPECT_FALSE(UsePointBasedTargeting(rect_2));
  EXPECT_FALSE(UsePointBasedTargeting(rect_3));
  EXPECT_FALSE(UsePointBasedTargeting(rect_4));
}

TEST(RectBasedTargetingUtils, PercentCoveredBy) {
  gfx::Rect rect_1(gfx::Point(0, 0), gfx::Size(300, 120));
  gfx::Rect rect_2(gfx::Point(20, 10), gfx::Size(30, 90));
  gfx::Rect rect_3(gfx::Point(160, 50), gfx::Size(150, 85));
  gfx::Rect rect_4(gfx::Point(20, 55), gfx::Size(0, 15));
  float error = 0.001f;

  // Passing in identical rectangles.
  EXPECT_FLOAT_EQ(1.0f, PercentCoveredBy(rect_1, rect_1));

  // |rect_1| completely contains |rect_2|.
  EXPECT_FLOAT_EQ(0.075f, PercentCoveredBy(rect_1, rect_2));
  EXPECT_FLOAT_EQ(1.0f, PercentCoveredBy(rect_2, rect_1));

  // |rect_2| and |rect_3| do not intersect.
  EXPECT_FLOAT_EQ(0.0f, PercentCoveredBy(rect_2, rect_3));

  // |rect_1| and |rect_3| have a nonzero area of intersection which
  // is smaller than the area of either rectangle.
  EXPECT_NEAR(0.272f, PercentCoveredBy(rect_1, rect_3), error);
  EXPECT_NEAR(0.768f, PercentCoveredBy(rect_3, rect_1), error);

  // |rect_4| has no area.
  EXPECT_FLOAT_EQ(0.0f, PercentCoveredBy(rect_2, rect_4));
  EXPECT_FLOAT_EQ(0.0f, PercentCoveredBy(rect_4, rect_2));
}

TEST(RectBasedTargetingUtils, DistanceSquaredFromCenterToPoint) {
  gfx::Rect rect_1(gfx::Point(0, 0), gfx::Size(10, 10));
  gfx::Rect rect_2(gfx::Point(20, 0), gfx::Size(80, 10));
  gfx::Rect rect_3(gfx::Point(0, 20), gfx::Size(10, 20));

  gfx::Point point_1(5, 5);
  gfx::Point point_2(25, 5);
  gfx::Point point_3(11, 15);
  gfx::Point point_4(33, 44);

  EXPECT_EQ(0, DistanceSquaredFromCenterToPoint(point_1, rect_1));
  EXPECT_EQ(1225, DistanceSquaredFromCenterToPoint(point_2, rect_2));
  EXPECT_EQ(3025, DistanceSquaredFromCenterToPoint(point_1, rect_2));
  EXPECT_EQ(1025, DistanceSquaredFromCenterToPoint(point_2, rect_3));
  EXPECT_EQ(2501, DistanceSquaredFromCenterToPoint(point_3, rect_2));
  EXPECT_EQ(136, DistanceSquaredFromCenterToPoint(point_3, rect_1));
  EXPECT_EQ(980, DistanceSquaredFromCenterToPoint(point_4, rect_3));
}

}  // namespace views
