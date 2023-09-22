// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_types_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace display {

TEST(DisplayTypesUtilTest, CompareTwoFloatsWithinEpsilon) {
  EXPECT_TRUE(IsWithinEpsilon(60.0f, 59.99999f));
  EXPECT_FALSE(IsWithinEpsilon(30.0f, 29.9f));
}

}  // namespace display
