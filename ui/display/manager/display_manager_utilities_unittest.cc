// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_utilities.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/test/display_test_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"

namespace display {

TEST(DisplayUtilitiesTest, GenerateDisplayIdList) {
  DisplayIdList list;
  {
    int64_t ids[] = {10, 1};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 5, 1};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(5, list[1]);
    EXPECT_EQ(10, list[2]);
  }
  {
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayIds set_internal(100);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayIds set_internal(10);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
}

TEST(DisplayUtilitiesTest, DisplayIdListToString) {
  {
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ("1,10,16", DisplayIdListToString(list));
  }
  {
    ScopedSetInternalDisplayIds set_internal(16);
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ("16,1,10", DisplayIdListToString(list));
  }
}

TEST(DisplayUtilitiesTest, ComputeBoundary) {
  // Two displays with their top and bottom align but share no edges.
  // +----+
  // |    |
  // +----+  +----+
  //         |    |
  //         +----+
  Display display_1(1, gfx::Rect(0, 0, 500, 300));
  Display display_2(2, gfx::Rect(759, 300, 133, 182));
  gfx::Rect edge_1;
  gfx::Rect edge_2;
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Two displays with their left and right align but share no edges.
  // +----+
  // |    |
  // +----+
  //
  //      +----+
  //      |    |
  //      +----+
  display_1.set_bounds(gfx::Rect(0, 0, 500, 300));
  display_2.set_bounds(gfx::Rect(500, 500, 240, 300));
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Special case: all edges align but no edges are shared.
  // +----+
  // |    |
  // +----+----+
  //      |    |
  //      +----+
  display_1.set_bounds(gfx::Rect(0, 0, 500, 300));
  display_2.set_bounds(gfx::Rect(500, 300, 500, 300));
  EXPECT_FALSE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));

  // Test normal cases.
  display_1.set_bounds(gfx::Rect(740, 0, 150, 300));
  display_2.set_bounds(gfx::Rect(759, 300, 133, 182));
  EXPECT_TRUE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));
  EXPECT_EQ(gfx::Rect(759, 299, 131, 1), edge_1);
  EXPECT_EQ(gfx::Rect(759, 300, 131, 1), edge_2);

  display_1.set_bounds(gfx::Rect(0, 0, 400, 400));
  display_2.set_bounds(gfx::Rect(400, 150, 400, 400));
  EXPECT_TRUE(ComputeBoundary(display_1, display_2, &edge_1, &edge_2));
  EXPECT_EQ(gfx::Rect(399, 150, 1, 250), edge_1);
  EXPECT_EQ(gfx::Rect(400, 150, 1, 250), edge_2);
}

}  // namespace display
