// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/normalized_geometry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace views {

TEST(NormalizedRectTest, Inset_NormalizedInsets) {
  NormalizedRect rect(1, 2, 10, 11);
  constexpr NormalizedInsets kInsets(1, 2, 3, 4);
  rect.Inset(kInsets);
  EXPECT_EQ(2, rect.origin_main());
  EXPECT_EQ(4, rect.origin_cross());
  EXPECT_EQ(6, rect.size_main());
  EXPECT_EQ(5, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_FourValue) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(1, 2, 3, 4);
  EXPECT_EQ(2, rect.origin_main());
  EXPECT_EQ(4, rect.origin_cross());
  EXPECT_EQ(6, rect.size_main());
  EXPECT_EQ(5, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_TwoValue) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(3, 4);
  EXPECT_EQ(4, rect.origin_main());
  EXPECT_EQ(6, rect.origin_cross());
  EXPECT_EQ(4, rect.size_main());
  EXPECT_EQ(3, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_Negative) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(-1, -2, -3, -4);
  EXPECT_EQ(0, rect.origin_main());
  EXPECT_EQ(0, rect.origin_cross());
  EXPECT_EQ(14, rect.size_main());
  EXPECT_EQ(17, rect.size_cross());
}

}  // namespace views
