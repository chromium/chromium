// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_scale_factor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(ScaleFactorTest, GetResourceScaleFactorScale) {
  EXPECT_FLOAT_EQ(1.0f, GetScaleForResourceScaleFactor(k100Percent));
  EXPECT_FLOAT_EQ(2.0f, GetScaleForResourceScaleFactor(k200Percent));
  EXPECT_FLOAT_EQ(3.0f, GetScaleForResourceScaleFactor(k300Percent));
}

}  // namespace ui
