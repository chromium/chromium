// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

struct DisplayGeometryTestData {
  gfx::Rect bounds_px;
  float scale;
  gfx::Rect expected_bounds_dip;
};

namespace {

void ConvertDisplayBoundsToDipsHelper(
    std::vector<DisplayGeometryTestData> test_data,
    size_t primary_display_index) {
  const size_t n = test_data.size();
  ASSERT_LT(primary_display_index, n);
  std::vector<display::Display> displays;
  for (size_t i = 0; i < n; i++) {
    displays.emplace_back(i, test_data[i].bounds_px);
    displays[i].set_device_scale_factor(test_data[i].scale);
  }
  ConvertDisplayBoundsToDips(&displays, primary_display_index);
  for (size_t i = 0; i < n; i++) {
    EXPECT_EQ(displays[i].bounds(), test_data[i].expected_bounds_dip);
  }
}

}  // namespace

TEST(X11DisplayUtilTest, RangeDistance) {
  // Adjacent ranges.
  EXPECT_EQ(RangeDistance(10, 20, 20, 30), 0);
  EXPECT_EQ(RangeDistance(20, 10, 30, 20), 0);
  // Separated ranges.
  EXPECT_EQ(RangeDistance(10, 20, 30, 40), 10);
  EXPECT_EQ(RangeDistance(30, 40, 10, 20), 10);
  // Partially overlapping ranges.
  EXPECT_EQ(RangeDistance(10, 30, 20, 30), -10);
  EXPECT_EQ(RangeDistance(20, 30, 10, 30), -10);
  // Fully overlapping ranges.
  EXPECT_EQ(RangeDistance(10, 40, 20, 30), -20);
  EXPECT_EQ(RangeDistance(20, 30, 10, 40), -20);
}

TEST(X11DisplayUtilTest, RectDistance) {
  // Rects meeting at a corner have distance {0, 0}.
  EXPECT_EQ(RectDistance({10, 10, 10, 10}, {20, 20, 10, 10}),
            std::make_pair(0, 0));

  // Adjacent rects have a distance of {0, x}.
  EXPECT_EQ(RectDistance({10, 10, 10, 10}, {20, 10, 10, 10}),
            std::make_pair(0, -10));
  EXPECT_EQ(RectDistance({20, 10, 10, 10}, {10, 10, 10, 10}),
            std::make_pair(0, -10));
  EXPECT_EQ(RectDistance({10, 10, 10, 10}, {10, 20, 10, 10}),
            std::make_pair(0, -10));
  EXPECT_EQ(RectDistance({10, 20, 10, 10}, {10, 10, 10, 10}),
            std::make_pair(0, -10));

  // Identical rects have a distance of {-min(w, h), -max(w, h)}.
  EXPECT_EQ(RectDistance({0, 0, 10, 20}, {0, 0, 10, 20}),
            std::make_pair(-10, -20));
  EXPECT_EQ(RectDistance({0, 0, 20, 10}, {0, 0, 20, 10}),
            std::make_pair(-10, -20));

  // Separated rects have a positive distance.
  EXPECT_EQ(RectDistance({10, 10, 10, 10}, {30, 10, 10, 10}),
            std::make_pair(10, -10));
  EXPECT_EQ(RectDistance({30, 10, 10, 10}, {10, 10, 10, 10}),
            std::make_pair(10, -10));
}

TEST(X11DisplayUtilTest, ConvertDisplayBoundsToDips) {
  // Single display
  ConvertDisplayBoundsToDipsHelper(
      {{{0, 0, 1000, 1000}, 1.0f, {0, 0, 1000, 1000}}}, 0);
  ConvertDisplayBoundsToDipsHelper(
      {{{0, 0, 1000, 1000}, 2.0f, {0, 0, 500, 500}}}, 0);
  ConvertDisplayBoundsToDipsHelper(
      {{{0, 0, 1000, 1000}, 0.5f, {0, 0, 2000, 2000}}}, 0);
  // Displays should be positioned relative to the primary display.
  ConvertDisplayBoundsToDipsHelper(
      {{{1000, 1000, 1000, 1000}, 1.0f, {1000, 1000, 1000, 1000}}}, 0);
  ConvertDisplayBoundsToDipsHelper(
      {{{1000, 1000, 1000, 1000}, 2.0f, {500, 500, 500, 500}}}, 0);

  // If all displays have the same scale factor, each display's bounds should
  // be scaled directly...
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 1000}, 1.0f, {0, 0, 1000, 1000}},
          {{1000, 0, 1000, 1000}, 1.0f, {1000, 0, 1000, 1000}},
      },
      0);
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 1000}, 2.0f, {0, 0, 500, 500}},
          {{1000, 0, 1000, 1000}, 2.0f, {500, 0, 500, 500}},
      },
      0);
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 2000, 1000}, 0.5f, {0, 0, 4000, 2000}},
          {{2000, 1000, 3000, 2000}, 0.5f, {4000, 2000, 6000, 4000}},
          {{5000, 2000, 4000, 3000}, 0.5f, {10000, 4000, 8000, 6000}},
      },
      0);
  // ... even in odd cases like overlapping displays, duplicated displays, or
  // "floating island" displays.
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 1000}, 2.0f, {0, 0, 500, 500}},
          {{0, 0, 1000, 1000}, 2.0f, {0, 0, 500, 500}},
          {{1000000, 1000000, 1000, 1000}, 2.0f, {500000, 500000, 500, 500}},
          {{0, 500, 1000, 1000}, 2.0f, {0, 250, 500, 500}},
          {{-1000, -1000, 1000, 1000}, 2.0f, {-500, -500, 500, 500}},
      },
      0);

  // Finally, test mixed scale factors.  The mapping from pixels to DIPs should
  // be continuous as long as there are no inconsistencies (eg. overlapping
  // displays with different scale factors).
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 2000}, 2.0f, {0, 0, 500, 1000}},
          {{1000, 500, 2000, 1000}, 1.0f, {500, 0, 2000, 1000}},
          {{3000, 0, 1000, 2000}, 2.0f, {2500, 0, 500, 1000}},
      },
      0);
  ConvertDisplayBoundsToDipsHelper(
      {
          {{3000, 0, 1000, 2000}, 2.0f, {2500, 0, 500, 1000}},
          {{1000, 500, 2000, 1000}, 1.0f, {500, 0, 2000, 1000}},
          {{0, 0, 1000, 2000}, 2.0f, {0, 0, 500, 1000}},
      },
      2);
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 2000}, 2.0f, {-1000, 0, 500, 1000}},
          {{1000, 500, 2000, 1000}, 1.0f, {-500, 0, 2000, 1000}},
          {{3000, 0, 1000, 2000}, 2.0f, {1500, 0, 500, 1000}},
      },
      2);
  ConvertDisplayBoundsToDipsHelper(
      {
          {{0, 0, 1000, 1000}, 1.0f, {0, 0, 1000, 1000}},
          {{1000, 0, 100, 100}, 2.0f, {1000, 25, 50, 50}},
          {{0, 1000, 100, 100}, 2.0f, {25, 1000, 50, 50}},
      },
      0);
}

}  // namespace ui
