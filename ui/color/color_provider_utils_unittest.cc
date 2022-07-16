// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_utils.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"

using ColorProviderUtilsTest = ::testing::Test;

TEST_F(ColorProviderUtilsTest, ConvertColorProviderColorIdToCSSColorId) {
  EXPECT_EQ(std::string("--color-primary-background"),
            ui::ConvertColorProviderColorIdToCSSColorId(
                std::string(ui::ColorIdName(ui::kColorPrimaryBackground))));
}

TEST_F(ColorProviderUtilsTest, ConvertSkColorToCSSColor) {
  SkColor test_color = SkColorSetRGB(0xF2, 0x99, 0x00);
  // This will fail if we don't make sure to show two hex digits per color.
  EXPECT_EQ(std::string("#f29900ff"), ui::ConvertSkColorToCSSColor(test_color));
  SkColor test_color_alpha = SkColorSetA(test_color, 0x25);
  EXPECT_EQ(std::string("#f2990025"),
            ui::ConvertSkColorToCSSColor(test_color_alpha));
}
