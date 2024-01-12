// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/main_display_sampler.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {
namespace {

using testing::UnorderedElementsAre;

constexpr CGDirectDisplayID kDummyDisplay = 0xFABBEE;

class TestingMainDisplaySampler : public MainDisplaySampler {
 public:
  TestingMainDisplaySampler(std::optional<float> brightness, bool sleeping)
      : MainDisplaySampler(kDummyDisplay),
        brightness_(brightness),
        sleeping_(sleeping) {}

  static std::unique_ptr<TestingMainDisplaySampler> Create(
      std::optional<float> brightness,
      bool sleeping);

 private:
  bool GetIsDisplaySleeping() override { return sleeping_; }
  std::optional<float> GetDisplayBrightness() override { return brightness_; }

  const std::optional<float> brightness_;
  const bool sleeping_;
};

// static
std::unique_ptr<TestingMainDisplaySampler> TestingMainDisplaySampler::Create(
    std::optional<float> brightness,
    bool sleeping) {
  return std::make_unique<TestingMainDisplaySampler>(brightness, sleeping);
}

}  // namespace

TEST(MainDisplaySamplerTest, NameAndGetDatumNameUnits) {
  std::unique_ptr<MainDisplaySampler> sampler(
      TestingMainDisplaySampler::Create(0.5, true));
  ASSERT_NE(nullptr, sampler.get());

  EXPECT_EQ("main_display", sampler->GetName());

  auto datum_name_units = sampler->GetDatumNameUnits();
  ASSERT_EQ(2u, datum_name_units.size());
  EXPECT_EQ("%", datum_name_units["brightness"]);
  EXPECT_EQ("bool", datum_name_units["sleeping"]);
}

TEST(MainDisplaySamplerTest, SamplesBrightnessAndSleeping) {
  std::unique_ptr<MainDisplaySampler> sampler(
      TestingMainDisplaySampler::Create(0.5, false));
  ASSERT_NE(nullptr, sampler.get());
  Sampler::Sample datums = sampler->GetSample(base::TimeTicks::Now());

  EXPECT_THAT(datums, UnorderedElementsAre(std::make_pair("brightness", 50.0),
                                           std::make_pair("sleeping", 0)));

  // Validate that the sleeping datum can go both ways.
  sampler = TestingMainDisplaySampler::Create(0.875, true);
  datums = sampler->GetSample(base::TimeTicks::Now());
  ASSERT_NE(nullptr, sampler.get());
  EXPECT_THAT(datums, UnorderedElementsAre(std::make_pair("brightness", 87.5),
                                           std::make_pair("sleeping", 1.0)));
}

TEST(MainDisplaySamplerTest, ReturnsSampleWhenNoBrightness) {
  std::unique_ptr<MainDisplaySampler> sampler(
      TestingMainDisplaySampler::Create(std::nullopt, false));
  ASSERT_NE(nullptr, sampler.get());
  Sampler::Sample datums = sampler->GetSample(base::TimeTicks::Now());
  EXPECT_THAT(datums, UnorderedElementsAre(std::make_pair("sleeping", 0.0)));
}

}  // namespace power_sampler
