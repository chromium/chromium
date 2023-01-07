// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/user_idle_level_sampler.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_sampler {

TEST(UserIdleLevelSamplerTest, NameAndGetDatumNameUnits) {
  std::unique_ptr<UserIdleLevelSampler> sampler(UserIdleLevelSampler::Create());
  EXPECT_NE(nullptr, sampler.get());

  EXPECT_EQ("user_idle_level", sampler->GetName());

  auto datum_name_units = sampler->GetDatumNameUnits();
  ASSERT_EQ(1u, datum_name_units.size());
  EXPECT_EQ("int", datum_name_units["user_idle_level"]);
}

TEST(UserIdleLevelSamplerTest, ReturnsASample) {
  std::unique_ptr<UserIdleLevelSampler> sampler(UserIdleLevelSampler::Create());
  ASSERT_NE(nullptr, sampler.get());

  Sampler::Sample sample = sampler->GetSample(base::TimeTicks::Now());

  EXPECT_EQ(1u, sample.size());
  auto it = sample.find("user_idle_level");
  ASSERT_TRUE(it != sample.end());
  // These are the values seen so far.
  EXPECT_TRUE(it->second == 0.0 || it->second == 128.0);
}

}  // namespace power_sampler
