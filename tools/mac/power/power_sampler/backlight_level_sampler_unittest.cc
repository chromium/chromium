// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/backlight_level_sampler.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {
namespace {

// Simulates a display with no brightness control.
int NoBrightness(CGDirectDisplayID id, float* brightness) {
  return -1;
}

// Simulates a display with brightness control.
int PointFiveBrightness(CGDirectDisplayID id, float* brightness) {
  *brightness = 0.5;
  return 0;
}

constexpr CGDirectDisplayID kDummyDisplay = 0xFABBEE;

}  // namespace

TEST(BacklightLevelSamplerTest, CreateFailsWhenNoBrightness) {
  EXPECT_EQ(nullptr, BacklightLevelSampler::CreateForTesting(kDummyDisplay,
                                                             NoBrightness));
}

TEST(BacklightLevelSamplerTest, NameAndGetDatumNameUnits) {
  std::unique_ptr<BacklightLevelSampler> sampler(
      BacklightLevelSampler::CreateForTesting(kDummyDisplay,
                                              PointFiveBrightness));
  ASSERT_NE(nullptr, sampler.get());

  EXPECT_EQ("BacklightLevel", sampler->GetName());

  auto datum_name_units = sampler->GetDatumNameUnits();
  ASSERT_EQ(1u, datum_name_units.size());
  EXPECT_EQ("%", datum_name_units["display_brightness"]);
}

TEST(BacklightLevelSamplerTest, ReturnsASample) {
  std::unique_ptr<BacklightLevelSampler> sampler(
      BacklightLevelSampler::CreateForTesting(kDummyDisplay,
                                              PointFiveBrightness));
  ASSERT_NE(nullptr, sampler.get());
  Sampler::Sample datums = sampler->GetSample(base::TimeTicks::Now());

  EXPECT_EQ(1u, datums.size());
  auto it = datums.find("display_brightness");
  ASSERT_TRUE(it != datums.end());
  // The level should be in the range of 0.0-100.0.
  EXPECT_EQ(50, it->second);
}

}  // namespace power_sampler
