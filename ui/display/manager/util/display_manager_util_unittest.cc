// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/util/display_manager_util.h"

#include <array>
#include <vector>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_test_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/size.h"

namespace display::test {

namespace {

constexpr std::size_t kNumOfZoomFactors = 9;
using ZoomListBucket = std::pair<int, std::array<float, kNumOfZoomFactors>>;

constexpr std::array<ZoomListBucket, 4> kTestData{{
    {240, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
    {720, {0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f}},
    {1024, {0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f}},
    {2400, {1.f, 1.10f, 1.15f, 1.20f, 1.30f, 1.40f, 1.50f, 1.75f, 2.00f}},
}};

bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

}  // namespace

using DisplayManagerUtilTest = testing::Test;

TEST_F(DisplayManagerUtilTest, DisplayZooms) {
  // The expected zoom list for the width given by |first| of the pair should be
  // equal to the |second| of the pair.
  for (const auto& data : kTestData) {
    {
      SCOPED_TRACE("Landscape");
      ManagedDisplayMode mode(gfx::Size(data.first, data.first / 2), 60, false,
                              true, 1.f);
      const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);
      for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
        EXPECT_FLOAT_EQ(zoom_values[j], data.second[j]);
      }
    }
    {
      SCOPED_TRACE("Portrait");
      ManagedDisplayMode mode(gfx::Size(data.first / 2, data.first), 60, false,
                              true, 1.f);
      const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);
      for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
        EXPECT_FLOAT_EQ(zoom_values[j], data.second[j]);
      }
    }
  }
}

TEST_F(DisplayManagerUtilTest, DisplayZoomsByDisplayWidth) {
  // The expected zoom list for the width given by |first| of the pair should be
  // equal to the |second| of the pair.
  for (const auto& data : kTestData) {
    const std::vector<float> zoom_values =
        GetDisplayZoomFactorsByDisplayWidth(data.first);
    for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
      EXPECT_FLOAT_EQ(zoom_values[j], data.second[j]);
    }
  }
}

TEST_F(DisplayManagerUtilTest, DisplayZoomsWithInternal) {
  std::vector<float> kDpis = {160, 200, 225, 240, 280, 320};
  float previous_dsf = 0.f;
  for (const auto& dpi : kDpis) {
    SCOPED_TRACE(base::StringPrintf("dpi=%f", dpi));
    float dsf = DisplayChangeObserver::FindDeviceScaleFactor(dpi, gfx::Size());
    // Make sure each dpis is mapped to different dsf.
    EXPECT_NE(previous_dsf, dsf);
    previous_dsf = dsf;
    const std::vector<float> zoom_values = GetDisplayZoomFactorForDsf(dsf);
    const float inverse_dsf = 1.f / dsf;
    uint8_t checks = 0;
    for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
      if (WithinEpsilon(zoom_values[j], inverse_dsf)) {
        checks |= 0x01;
      }
      if (WithinEpsilon(zoom_values[j], 1.f)) {
        checks |= 0x02;
      }
      EXPECT_LT(0.0f, zoom_values[j]);
    }
    EXPECT_TRUE(checks & 0x01) << "Inverse of " << dsf << " not on the list.";
    EXPECT_TRUE(checks & 0x02) << "Zoom level of unity is not on the list.";
  }
}

TEST(DisplayManagerUtilTest, GenerateDisplayIdList) {
  DisplayIdList list;
  {
    int64_t ids[] = {10, 1};
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 5, 1};
    list = GenerateDisplayIdList(three_ids);
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(5, list[1]);
    EXPECT_EQ(10, list[2]);
  }
  {
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(three_ids);
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayIds set_internal(100);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(three_ids);
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayIds set_internal(10);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(ids);
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(three_ids);
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
}

TEST(DisplayManagerUtilTest, DisplayIdListToString) {
  {
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(ids);
    EXPECT_EQ("1,10,16", DisplayIdListToString(list));
  }
  {
    ScopedSetInternalDisplayIds set_internal(16);
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(ids);
    EXPECT_EQ("16,1,10", DisplayIdListToString(list));
  }
}

TEST(DisplayManagerUtilTest, ComputeBoundary) {
  // Two displays with their top and bottom align but share no edges.
  // +----+
  // |    |
  // +----+  +----+
  //         |    |
  //         +----+
  Display display_1(1, gfx::Rect(0, 0, 500, 300));
  Display display_2(2, gfx::Rect(759, 300, 133, 182));
  gfx::Rect edge_1;
  gfx::Rect edge_2;
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Two displays with their left and right align but share no edges.
  // +----+
  // |    |
  // +----+
  //
  //      +----+
  //      |    |
  //      +----+
  display_1.set_bounds(gfx::Rect(0, 0, 500, 300));
  display_2.set_bounds(gfx::Rect(500, 500, 240, 300));
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Special case: all edges align but no edges are shared.
  // +----+
  // |    |
  // +----+----+
  //      |    |
  //      +----+
  display_1.set_bounds(gfx::Rect(0, 0, 500, 300));
  display_2.set_bounds(gfx::Rect(500, 300, 500, 300));
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Test normal cases.
  display_1.set_bounds(gfx::Rect(740, 0, 150, 300));
  display_2.set_bounds(gfx::Rect(759, 300, 133, 182));
  EXPECT_TRUE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));
  EXPECT_EQ(gfx::Rect(759, 299, 131, 1), edge_1);
  EXPECT_EQ(gfx::Rect(759, 300, 131, 1), edge_2);

  display_1.set_bounds(gfx::Rect(0, 0, 400, 400));
  display_2.set_bounds(gfx::Rect(400, 150, 400, 400));
  EXPECT_TRUE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));
  EXPECT_EQ(gfx::Rect(399, 150, 1, 250), edge_1);
  EXPECT_EQ(gfx::Rect(400, 150, 1, 250), edge_2);
}

}  // namespace display::test
