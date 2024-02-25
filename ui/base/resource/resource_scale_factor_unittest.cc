// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_scale_factor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(ResourceScaleFactorTest, GetResourceScaleFactorScale) {
  EXPECT_FLOAT_EQ(1.0f, GetScaleForResourceScaleFactor(k100Percent));
  EXPECT_FLOAT_EQ(2.0f, GetScaleForResourceScaleFactor(k200Percent));
  EXPECT_FLOAT_EQ(3.0f, GetScaleForResourceScaleFactor(k300Percent));
}

TEST(ResourceScaleFactorTest, GetScaleFactorFromScalePartlySupported) {
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {k100Percent, k200Percent});
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(0.1f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(0.9f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(1.0f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(1.41f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(1.6f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(2.0f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(999.0f));
}

TEST(ResourceScaleFactorTest, GetScaleFactorFromScaleAllSupported) {
  std::vector<ResourceScaleFactor> supported_factors;
  for (int factor = k100Percent; factor < NUM_SCALE_FACTORS; ++factor) {
    supported_factors.push_back(static_cast<ResourceScaleFactor>(factor));
  }
  test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      supported_factors);

  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(0.1f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(0.9f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(1.0f));
  EXPECT_EQ(k100Percent, GetSupportedResourceScaleFactor(1.49f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(1.51f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(2.0f));
  EXPECT_EQ(k200Percent, GetSupportedResourceScaleFactor(2.49f));
  EXPECT_EQ(k300Percent, GetSupportedResourceScaleFactor(2.51f));
  EXPECT_EQ(k300Percent, GetSupportedResourceScaleFactor(3.0f));
  EXPECT_EQ(k300Percent, GetSupportedResourceScaleFactor(3.1f));
  EXPECT_EQ(k300Percent, GetSupportedResourceScaleFactor(999.0f));
}

}  // namespace ui
