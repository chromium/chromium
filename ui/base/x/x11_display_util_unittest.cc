// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_display_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/connection.h"

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

TEST(X11DisplayUtilTest, GetDisplayRefreshInterval) {
  // A 60Hz primary display next to a 144Hz display, both scaled to check that
  // pixel bounds are used.
  std::vector<display::Display> displays;
  displays.emplace_back(0, gfx::Rect(0, 0, 1920, 1080));
  displays.emplace_back(1, gfx::Rect(1920, 0, 2560, 1440));
  displays[0].set_display_frequency(60);
  displays[1].set_display_frequency(144);
  for (auto& display : displays) {
    display.set_device_scale_factor(2);
  }
  ConvertDisplayBoundsToDips(&displays, 0);

  const auto k60Hz = base::Seconds(1. / 60);
  const auto k144Hz = base::Seconds(1. / 144);

  // Unknown window bounds use the primary display.
  EXPECT_EQ(GetDisplayRefreshInterval(displays, 0, std::nullopt), k60Hz);
  EXPECT_EQ(GetDisplayRefreshInterval(displays, 1, std::nullopt), k144Hz);

  // Windows entirely on one display.
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 0, gfx::Rect(100, 100, 800, 600)),
      k60Hz);
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 0, gfx::Rect(2000, 100, 800, 600)),
      k144Hz);
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 1, gfx::Rect(100, 100, 800, 600)),
      k60Hz);

  // Windows spanning both displays use the one with the larger intersection.
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 0, gfx::Rect(1800, 100, 800, 600)),
      k144Hz);
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 0, gfx::Rect(1400, 100, 800, 600)),
      k60Hz);

  // Windows outside every display use the primary display.
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 1, gfx::Rect(0, 2000, 800, 600)),
      k144Hz);

  // Displays with an unknown refresh rate use 60Hz.
  displays[1].set_display_frequency(0);
  EXPECT_EQ(
      GetDisplayRefreshInterval(displays, 0, gfx::Rect(2000, 100, 800, 600)),
      k60Hz);
}

// Mirrored outputs (e.g. `xrandr --same-as`) have identical bounds and should
// be reported as a single display.
TEST(X11DisplayUtilTest, RemoveMirroredDisplays) {
  std::vector<display::Display> displays;
  displays.emplace_back(10, gfx::Rect(0, 0, 1920, 1080));
  displays.emplace_back(11, gfx::Rect(1920, 0, 2560, 1440));
  displays.emplace_back(12, gfx::Rect(0, 0, 1920, 1080));
  size_t primary_display_index = 1;
  RemoveMirroredDisplays(displays, primary_display_index);
  ASSERT_EQ(displays.size(), 2u);
  EXPECT_EQ(displays[0].id(), 10);
  EXPECT_EQ(displays[1].id(), 11);
  EXPECT_EQ(primary_display_index, 1u);

  // A primary display that mirrors an earlier display replaces it.
  displays.emplace_back(12, gfx::Rect(0, 0, 1920, 1080));
  primary_display_index = 2;
  RemoveMirroredDisplays(displays, primary_display_index);
  ASSERT_EQ(displays.size(), 2u);
  EXPECT_EQ(displays[0].id(), 12);
  EXPECT_EQ(displays[1].id(), 11);
  EXPECT_EQ(primary_display_index, 0u);
}

// GtkUi folds the font scale (Xft.dpi / 96) into the primary scale, so it can
// legitimately be less than 1. The fallback must accept such scales like the
// XRandR path does.
TEST(X11DisplayUtilTest, GetFallbackDisplayListScaleBelowOne) {
  constexpr float kScale = 0.921875f;
  size_t primary_display_index = 1;
  std::vector<display::Display> displays =
      GetFallbackDisplayList(kScale, &primary_display_index);
  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(primary_display_index, 0u);

  const auto& screen = x11::Connection::Get()->default_screen();
  const gfx::Rect bounds_px(screen.width_in_pixels, screen.height_in_pixels);
  EXPECT_EQ(displays[0].device_scale_factor(), kScale);
  EXPECT_EQ(displays[0].GetSizeInPixel(), bounds_px.size());
  EXPECT_EQ(displays[0].bounds(),
            gfx::ScaleToEnclosingRect(bounds_px, 1.0f / kScale));
}

}  // namespace ui
