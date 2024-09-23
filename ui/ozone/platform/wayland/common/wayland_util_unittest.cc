// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_util.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"

namespace ui {

class WaylandUtilTest : public testing::Test {};

TEST_F(WaylandUtilTest, TestCreateRectsFromSkPath) {
  constexpr int width = 100;
  constexpr int height = 100;

  // Test1 consists of 2 rectangles from SkPath.
  std::vector<gfx::Rect> expectedFor2Rects = {gfx::Rect(1, 0, 99, 1),
                                              gfx::Rect(0, 1, 100, 99)};
  SkPath pathFor2Rects;
  const SkRect rect = SkRect::MakeIWH(width, height);
  constexpr SkScalar corner_radius_scalar = 2.0;
  constexpr SkScalar radii[8] = {corner_radius_scalar,
                                 corner_radius_scalar,  // top-left
                                 corner_radius_scalar,
                                 corner_radius_scalar,  // top-right
                                 0,
                                 0,  // bottom-right
                                 0,
                                 0};  // bottom-left
  pathFor2Rects.addRoundRect(rect, radii, SkPathDirection::kCW);
  EXPECT_EQ(expectedFor2Rects, wl::CreateRectsFromSkPath(pathFor2Rects));

  // Test2 consists of 5 rectangles from SkPath.
  std::vector<gfx::Rect> expectedFor5Rects = {
      gfx::Rect(3, 0, 94, 1), gfx::Rect(1, 1, 98, 2), gfx::Rect(0, 3, 100, 94),
      gfx::Rect(1, 97, 98, 2), gfx::Rect(3, 99, 94, 1)};
  SkPath pathFor5Rects;
  pathFor5Rects.moveTo(0, 3);
  pathFor5Rects.lineTo(1, 3);
  pathFor5Rects.lineTo(1, 1);
  pathFor5Rects.lineTo(3, 1);
  pathFor5Rects.lineTo(3, 0);

  pathFor5Rects.lineTo(width - 3, 0);
  pathFor5Rects.lineTo(width - 3, 1);
  pathFor5Rects.lineTo(width - 1, 1);
  pathFor5Rects.lineTo(width - 1, 3);
  pathFor5Rects.lineTo(width, 3);

  pathFor5Rects.lineTo(width, height - 3);
  pathFor5Rects.lineTo(width - 1, height - 3);
  pathFor5Rects.lineTo(width - 1, height - 1);
  pathFor5Rects.lineTo(width - 3, height - 1);
  pathFor5Rects.lineTo(width - 3, height);

  pathFor5Rects.lineTo(3, height);
  pathFor5Rects.lineTo(3, height - 1);
  pathFor5Rects.lineTo(1, height - 1);
  pathFor5Rects.lineTo(1, height - 3);
  pathFor5Rects.lineTo(0, height - 3);
  pathFor5Rects.close();

  EXPECT_EQ(expectedFor5Rects, wl::CreateRectsFromSkPath(pathFor5Rects));
}

}  // namespace ui
