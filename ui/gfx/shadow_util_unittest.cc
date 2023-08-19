// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/shadow_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {

// Tests the ShadowDetailsKey works properly for shadow details cache.
TEST(ShadowUtilTest, ShadowDetailsKey) {
  // Make a cache for the generated details such that they will not be removed
  // from the shadow details cache.
  std::vector<ShadowDetails> details;
  // Add first shadow details.
  details.emplace_back(ShadowDetails::Get(/*elevation=*/4, /*radius=*/2));
  EXPECT_EQ(1u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add second shadow details with a different elevation.
  details.emplace_back(ShadowDetails::Get(/*elevation=*/5, /*radius=*/2));
  EXPECT_EQ(2u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add third shadow details with a different rounded corner radius.
  details.emplace_back(ShadowDetails::Get(/*elevation=*/5, /*radius=*/3));
  EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add a same shadow details will not increase the cache.
  details.emplace_back(ShadowDetails::Get(/*elevation=*/4, /*radius=*/2));
  EXPECT_EQ(3u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add fourth shadow details with a different key shadow blur than the first
  // details.
  const ShadowValues& values_1 = details[0].values;
  ShadowValues new_blur_values = {
      ShadowValue(values_1[0].offset(), /*blur=*/20, values_1[0].color()),
      values_1[1]};
  details.emplace_back(ShadowDetails::Get(/*radius=*/2, new_blur_values));
  EXPECT_EQ(4u, ShadowDetails::GetDetailsCacheSizeForTest());

  // Add fifth shadow details with a different ambient color than the second
  // details.
  const ShadowValues& values_2 = details[1].values;
  ShadowValues new_color_values = {
      ShadowValue(values_2[0].offset(), values_2[0].blur(), SK_ColorBLUE),
      values_2[1]};
  details.emplace_back(ShadowDetails::Get(/*radius=*/2, new_color_values));
  EXPECT_EQ(5u, ShadowDetails::GetDetailsCacheSizeForTest());
}

}  // namespace gfx
