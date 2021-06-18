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
  supported_factors.push_back(SCALE_FACTOR_200P);
  test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.1f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(0.9f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.0f));
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.41f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(1.6f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(2.0f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(999.0f));
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
  EXPECT_EQ(SCALE_FACTOR_100P, GetSupportedScaleFactor(1.49f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(1.51f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(2.0f));
  EXPECT_EQ(SCALE_FACTOR_200P, GetSupportedScaleFactor(2.49f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(2.51f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(3.0f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(3.1f));
  EXPECT_EQ(SCALE_FACTOR_300P, GetSupportedScaleFactor(999.0f));
}

}  // namespace ui
