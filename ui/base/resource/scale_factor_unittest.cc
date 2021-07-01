// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/scale_factor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(ScaleFactorTest, GetResourceScaleFactorScale) {
  EXPECT_FLOAT_EQ(1.0f, GetScaleForResourceScaleFactor(SCALE_FACTOR_100P));
  EXPECT_FLOAT_EQ(2.0f, GetScaleForResourceScaleFactor(SCALE_FACTOR_200P));
  EXPECT_FLOAT_EQ(3.0f, GetScaleForResourceScaleFactor(SCALE_FACTOR_300P));
}

}  // namespace ui
