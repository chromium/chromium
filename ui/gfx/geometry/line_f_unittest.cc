// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/line_f.h"

#include <stddef.h>

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

TEST(LineFTest, Normal) {
  EXPECT_EQ(LineF({10, 10}, {100, 10}).Normal(), Vector2dF(0, 90));
  EXPECT_EQ(LineF({10, 10}, {10, 100}).Normal(), Vector2dF(-90, 0));
  EXPECT_EQ(LineF({20, 20}, {100, 100}).Normal(), Vector2dF(-80, 80));
  EXPECT_EQ(LineF({5, 5}, {5, 5}).Normal(), Vector2dF(0, 0));
  EXPECT_EQ(LineF({0, -15}, {15, -10}).Normal(), Vector2dF(-5, 15));
}

TEST(LineFTest, Intersection) {
  EXPECT_EQ(LineF({10, 10}, {100, 10}).IntersectionWith({{50, 0}, {50, 100}}),
            PointF(50, 10));
  EXPECT_EQ(LineF({10, 10}, {100, 10}).IntersectionWith({{50, 20}, {60, 20}}),
            std::nullopt);
  EXPECT_EQ(LineF({10, 10}, {10, 10}).IntersectionWith({{50, 30}, {60, 20}}),
            std::nullopt);
  EXPECT_EQ(LineF({0, 0}, {20, 20}).IntersectionWith({{0, 20}, {20, 0}}),
            PointF(10, 10));
  EXPECT_EQ(ToRoundedPoint(LineF({0, -10}, {-20, 20})
                               .IntersectionWith({{-10, 20}, {-100, -100}})
                               .value()),
            ToRoundedPoint(PointF(-15.2941, 12.9412)));
}

}  // namespace gfx
