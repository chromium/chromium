// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/triangle_f.h"

namespace gfx {

namespace {
constexpr PointF kPointA(1, 1);
constexpr PointF kPointB(10, 1);
constexpr PointF kPointC(1, 10);
}  // namespace

TEST(TriangleTest, PointIsInTriangleInside) {
  PointF p(2, 2);

  EXPECT_TRUE(PointIsInTriangle(p, kPointA, kPointB, kPointC));
}

TEST(TriangleTest, PointIsInTriangleOutside) {
  PointF o(0, 0);

  EXPECT_FALSE(PointIsInTriangle(o, kPointA, kPointB, kPointC));
}

TEST(TriangleTest, PointIsInTriangleEdge) {
  PointF e(1, 3);

  EXPECT_TRUE(PointIsInTriangle(e, kPointA, kPointB, kPointC));
}

TEST(TriangleTest, PointIsInTriangleVertex) {
  EXPECT_TRUE(PointIsInTriangle(kPointA, kPointA, kPointB, kPointC));
}

}  // namespace gfx