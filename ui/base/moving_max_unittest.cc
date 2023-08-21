// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/moving_max.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

const std::vector<int> kTestValues{
    33, 1, 2, 7, 5, 2, 4, 45, 1000, 1, 100, 2, 200, 2,  2, 2, 300, 4, 1,
    2,  3, 4, 5, 6, 7, 8, 9,  10,   9, 8,   7, 6,   5,  4, 3, 2,   1, 1,
    2,  1, 4, 2, 1, 8, 1, 2,  1,    4, 1,   2, 1,   16, 1, 2, 1};

class MovingMaxTest : public testing::TestWithParam<unsigned int> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MovingMaxTest,
                         testing::ValuesIn({1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 17u, 20u, 100u}));

TEST_P(MovingMaxTest, BlanketTest) {
  const size_t window_size = GetParam();
  MovingMax window(window_size);
  for (size_t i = 0; i < kTestValues.size(); ++i) {
    window.Put(kTestValues[i]);
    int slow_max = kTestValues[i];
    for (size_t j = 1; j < window_size && j <= i; ++j) {
      slow_max = std::max(slow_max, kTestValues[i - j]);
    }
    EXPECT_EQ(window.Max(), slow_max);
  }
}

TEST(MovingMax, SingleElementWindow) {
  MovingMax window(1u);
  window.Put(100);
  EXPECT_EQ(window.Max(), 100);
  window.Put(1000);
  EXPECT_EQ(window.Max(), 1000);
  window.Put(1);
  EXPECT_EQ(window.Max(), 1);
  window.Put(3);
  EXPECT_EQ(window.Max(), 3);
  window.Put(4);
  EXPECT_EQ(window.Max(), 4);
}

TEST(MovingMax, VeryLargeWindow) {
  MovingMax window(100u);
  window.Put(100);
  EXPECT_EQ(window.Max(), 100);
  window.Put(1000);
  EXPECT_EQ(window.Max(), 1000);
  window.Put(1);
  EXPECT_EQ(window.Max(), 1000);
  window.Put(3);
  EXPECT_EQ(window.Max(), 1000);
  window.Put(4);
  EXPECT_EQ(window.Max(), 1000);
}

}  // namespace ui
