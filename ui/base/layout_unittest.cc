// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#endif

namespace ui {

TEST(LayoutTest, GetScaleFactorFromScalePartlySupported) {
  std::vector<ScaleFactor> supported_factors;
  supported_factors.push_back(SCALE_FACTOR_100P);
  supported_factors.push_back(SCALE_FACTOR_180P);
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.1f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.9f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.0f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.39f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(1.41f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(1.8f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(2.0f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(999.0f));
}

TEST(LayoutTest, GetScaleFactorFromScaleAllSupported) {
  std::vector<ScaleFactor> supported_factors;
  for (int factor = SCALE_FACTOR_100P; factor < NUM_SCALE_FACTORS; ++factor) {
    supported_factors.push_back(static_cast<ScaleFactor>(factor));
  }
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);

  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.1f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.9f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.0f));
  EXPECT_EQ(SCALE_FACTOR_125P, GetSupportedScaleFactor(1.19f));
  EXPECT_EQ(SCALE_FACTOR_125P, GetSupportedScaleFactor(1.21f));
  EXPECT_EQ(SCALE_FACTOR_133P, GetSupportedScaleFactor(1.291f));
  EXPECT_EQ(SCALE_FACTOR_133P, GetSupportedScaleFactor(1.3f));
  EXPECT_EQ(SCALE_FACTOR_140P, GetSupportedScaleFactor(1.4f));
  EXPECT_EQ(SCALE_FACTOR_150P, GetSupportedScaleFactor(1.59f));
  EXPECT_EQ(SCALE_FACTOR_150P, GetSupportedScaleFactor(1.61f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(1.7f));
  EXPECT_EQ(SCALE_FACTOR_180P, GetSupportedScaleFactor(1.89f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(1.91f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(2.0f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(2.1f));
  EXPECT_EQ(SCALE_FACTOR_250P, GetSupportedScaleFactor(2.3f));
  EXPECT_EQ(SCALE_FACTOR_250P, GetSupportedScaleFactor(2.5f));
  EXPECT_EQ(SCALE_FACTOR_250P, GetSupportedScaleFactor(2.6f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(2.9f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(3.0f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(3.1f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(999.0f));
}

}  // namespace ui
