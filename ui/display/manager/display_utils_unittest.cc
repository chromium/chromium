// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_util.h"

#include <array>
#include <vector>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace test {

namespace {
constexpr std::size_t kNumOfZoomFactors = 9;
using ZoomListBucket = std::pair<int, std::array<float, kNumOfZoomFactors>>;

bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

}  // namespace
using DisplayUtilTest = testing::Test;

TEST_F(DisplayUtilTest, DisplayZooms) {
  // The expected zoom list for the width given by |first| of the pair should be
  //  equal to the |second| of the pair.
  constexpr std::array<ZoomListBucket, 4> kTestData{{
      {240, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
      {720, {0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f, 1.10f}},
      {1024, {0.90f, 0.95f, 1.f, 1.05f, 1.10f, 1.15f, 1.20f, 1.25f, 1.30f}},
      {2400, {1.f, 1.10f, 1.15f, 1.20f, 1.30f, 1.40f, 1.50f, 1.75f, 2.00f}},
  }};
  for (const auto& data : kTestData) {
    {
      SCOPED_TRACE("Landscape");
      ManagedDisplayMode mode(gfx::Size(data.first, data.first / 2), 60, false,
                              true, 1.f);
      const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);
      for (std::size_t j = 0; j < kNumOfZoomFactors; j++)
        EXPECT_FLOAT_EQ(zoom_values[j], data.second[j]);
    }
    {
      SCOPED_TRACE("Portrait");
      ManagedDisplayMode mode(gfx::Size(data.first / 2, data.first), 60, false,
                              true, 1.f);
      const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);
      for (std::size_t j = 0; j < kNumOfZoomFactors; j++)
        EXPECT_FLOAT_EQ(zoom_values[j], data.second[j]);
    }
  }
}

TEST_F(DisplayUtilTest, DisplayZoomsWithInternal) {
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
      if (WithinEpsilon(zoom_values[j], inverse_dsf))
        checks |= 0x01;
      if (WithinEpsilon(zoom_values[j], 1.f))
        checks |= 0x02;
      EXPECT_LT(0.0f, zoom_values[j]);
    }
    EXPECT_TRUE(checks & 0x01) << "Inverse of " << dsf << " not on the list.";
    EXPECT_TRUE(checks & 0x02) << "Zoom level of unity is not on the list.";
  }
}

TEST_F(DisplayUtilTest, InsertDsfIntoListLessThanUnity) {
  // list[0] -> actual
  // list[1] -> expected
  std::vector<float> list[2];
  float dsf;

  dsf = 0.6f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {dsf, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.6f;
  list[0] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, 1.05f};
  list[1] = {dsf, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, 1.05f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.67f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, dsf, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.9f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, dsf, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.99f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, dsf, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.99f;
  list[0] = {0.8f, 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.2f, 2.4f};
  list[1] = {dsf, 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.2f, 2.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.85f;
  list[0] = {1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  list[1] = {dsf, 1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);
}

TEST_F(DisplayUtilTest, InsertDsfIntoListGreaterThanUnity) {
  // list[0] -> actual
  // list[1] -> expected
  std::vector<float> list[2];
  float dsf;

  dsf = 1.f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, 1.05f};
  list[1] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, dsf};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, dsf};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.01f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.13f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.17f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, dsf, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  list[1] = {1.f, dsf, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);
}

}  // namespace test
}  // namespace display
