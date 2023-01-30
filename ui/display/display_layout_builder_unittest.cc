// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_layout_builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace display {

TEST(DisplayLayoutBuilderTest, SecondaryPlacement) {
  DisplayLayoutBuilder builder(1);
  builder.SetSecondaryPlacement(2, DisplayPlacement::LEFT, 30);
  std::unique_ptr<DisplayLayout> layout(builder.Build());
  ASSERT_EQ(1u, layout->placement_list.size());

  EXPECT_EQ(2, layout->placement_list[0].display_id);
  EXPECT_EQ(1, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(30, layout->placement_list[0].offset);
  EXPECT_EQ(DisplayPlacement::LEFT, layout->placement_list[0].position);
}

TEST(DisplayLayoutBuilderTest, MultiplePlacement) {
  DisplayLayoutBuilder builder(1);
  builder.AddDisplayPlacement(5, 1, DisplayPlacement::TOP, 30);
  builder.AddDisplayPlacement(3, 5, DisplayPlacement::LEFT, 20);
  builder.AddDisplayPlacement(4, 5, DisplayPlacement::RIGHT, 10);
  std::unique_ptr<DisplayLayout> layout(builder.Build());

  ASSERT_EQ(3u, layout->placement_list.size());

  // placements are sorted by display_id.
  EXPECT_EQ(3, layout->placement_list[0].display_id);
  EXPECT_EQ(5, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(20, layout->placement_list[0].offset);
  EXPECT_EQ(DisplayPlacement::LEFT, layout->placement_list[0].position);

  EXPECT_EQ(4, layout->placement_list[1].display_id);
  EXPECT_EQ(5, layout->placement_list[1].parent_display_id);
  EXPECT_EQ(10, layout->placement_list[1].offset);
  EXPECT_EQ(DisplayPlacement::RIGHT, layout->placement_list[1].position);

  EXPECT_EQ(5, layout->placement_list[2].display_id);
  EXPECT_EQ(1, layout->placement_list[2].parent_display_id);
  EXPECT_EQ(30, layout->placement_list[2].offset);
  EXPECT_EQ(DisplayPlacement::TOP, layout->placement_list[2].position);
}

}  // namespace display
