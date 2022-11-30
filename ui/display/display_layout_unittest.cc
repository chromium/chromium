// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"

#include <tuple>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"

namespace display {

using Position = DisplayPlacement::Position;

TEST(DisplayLayoutTest, Empty) {
  Displays display_list;
  std::vector<int64_t> updated_ids;

  DisplayLayout display_layout;
  display_layout.ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  EXPECT_EQ(0u, display_list.size());
}

TEST(DisplayLayoutTest, SingleDisplayNoPlacements) {
  Displays display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 800, 600));
  std::vector<int64_t> updated_ids;

  DisplayLayout display_layout;
  display_layout.ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  ASSERT_EQ(1u, display_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), display_list[0].bounds());
}

TEST(DisplayLayoutTest, SingleDisplayNonRelevantPlacement) {
  Displays display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 800, 600));
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(20, 40, DisplayPlacement::Position::LEFT, 150);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(&display_list, &updated_ids, 0);

  EXPECT_EQ(0u, updated_ids.size());
  ASSERT_EQ(1u, display_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), display_list[0].bounds());
}

TEST(DisplayLayoutTest, SwapPrimaryDisplay) {
  std::unique_ptr<DisplayLayout> layout =
      DisplayLayoutBuilder(123)
          .AddDisplayPlacement(456, 123, Position::LEFT, 150)
          .Build();

  // Initial layout will be 123 <-- 456
  EXPECT_EQ(123, layout->primary_id);
  EXPECT_EQ(456, layout->placement_list[0].display_id);
  EXPECT_EQ(123, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::LEFT, layout->placement_list[0].position);
  EXPECT_EQ(150, layout->placement_list[0].offset);

  // Swap layout to 123 --> 456.
  layout->SwapPrimaryDisplay(456);
  EXPECT_EQ(456, layout->primary_id);
  EXPECT_EQ(123, layout->placement_list[0].display_id);
  EXPECT_EQ(456, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::RIGHT, layout->placement_list[0].position);
  EXPECT_EQ(-150, layout->placement_list[0].offset);

  // Swap layout back to 123 <-- 456.
  layout->SwapPrimaryDisplay(123);
  EXPECT_EQ(123, layout->primary_id);
  EXPECT_EQ(456, layout->placement_list[0].display_id);
  EXPECT_EQ(123, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::LEFT, layout->placement_list[0].position);
  EXPECT_EQ(150, layout->placement_list[0].offset);
}

TEST(DisplayLayoutTest, SwapPrimaryDisplayThreeDisplays) {
  std::unique_ptr<DisplayLayout> layout =
      DisplayLayoutBuilder(456)
          .AddDisplayPlacement(123, 456, Position::LEFT, 0)
          .AddDisplayPlacement(789, 456, Position::RIGHT, 0)
          .Build();

  // Note: Placement order is determined by least significant 8 bits of IDs.
  // Initial layout will be 123 (0x7B) --> 456 (0x1C8) <-- 789 (0x315).
  EXPECT_EQ(456, layout->primary_id);
  EXPECT_EQ(789, layout->placement_list[0].display_id);
  EXPECT_EQ(456, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::RIGHT, layout->placement_list[0].position);
  EXPECT_EQ(123, layout->placement_list[1].display_id);
  EXPECT_EQ(456, layout->placement_list[1].parent_display_id);
  EXPECT_EQ(Position::LEFT, layout->placement_list[1].position);

  // Swap layout to 123 --> 456 --> 789.
  layout->SwapPrimaryDisplay(789);
  EXPECT_EQ(789, layout->primary_id);
  EXPECT_EQ(123, layout->placement_list[0].display_id);
  EXPECT_EQ(456, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::LEFT, layout->placement_list[0].position);
  EXPECT_EQ(456, layout->placement_list[1].display_id);
  EXPECT_EQ(789, layout->placement_list[1].parent_display_id);
  EXPECT_EQ(Position::LEFT, layout->placement_list[1].position);

  // Swap layout to 123 <-- 456 <-- 789.
  layout->SwapPrimaryDisplay(123);
  EXPECT_EQ(123, layout->primary_id);
  EXPECT_EQ(789, layout->placement_list[0].display_id);
  EXPECT_EQ(456, layout->placement_list[0].parent_display_id);
  EXPECT_EQ(Position::RIGHT, layout->placement_list[0].position);
  EXPECT_EQ(456, layout->placement_list[1].display_id);
  EXPECT_EQ(123, layout->placement_list[1].parent_display_id);
  EXPECT_EQ(Position::RIGHT, layout->placement_list[1].position);
}

// Makes sure that only the least significant 8 bits of the display IDs in the
// placement lists are used to validate their sort order.
TEST(DisplayLayoutTest, PlacementSortOrder) {
  // Sorted placement lists by full IDs, but not sorted by the least significant
  // 8 bits of the IDs.
  std::unique_ptr<DisplayLayout> layout(new DisplayLayout);
  layout->primary_id = 456;
  layout->placement_list.emplace_back(0x0405, 456, Position::LEFT, 0,
                                      DisplayPlacement::TOP_LEFT);
  layout->placement_list.emplace_back(0x0506, 0x0405, Position::RIGHT, 0,
                                      DisplayPlacement::TOP_LEFT);
  layout->placement_list.emplace_back(0x0604, 0x0506, Position::RIGHT, 0,
                                      DisplayPlacement::TOP_LEFT);
  EXPECT_FALSE(DisplayLayout::Validate({456, 0x0405, 0x0506, 0x0604}, *layout));

  // Full IDs not sorted, but least significant 8 bits of the IDs are sorted.
  layout->placement_list.clear();
  layout->placement_list.emplace_back(0x0504, 456, Position::LEFT, 0,
                                      DisplayPlacement::TOP_LEFT);
  layout->placement_list.emplace_back(0x0605, 0x0504, Position::RIGHT, 0,
                                      DisplayPlacement::TOP_LEFT);
  layout->placement_list.emplace_back(0x0406, 0x0605, Position::RIGHT, 0,
                                      DisplayPlacement::TOP_LEFT);
  EXPECT_TRUE(DisplayLayout::Validate({456, 0x0504, 0x0605, 0x0406}, *layout));
}

namespace {

class TwoDisplays
    : public testing::TestWithParam<std::tuple<
          // Primary Display Bounds
          gfx::Rect,
          // Secondary Display Bounds
          gfx::Rect,
          // Secondary Layout Position
          DisplayPlacement::Position,
          // Secondary Layout Offset
          int,
          // Minimum Offset Overlap
          int,
          // Expected Primary Display Bounds
          gfx::Rect,
          // Expected Secondary Display Bounds
          gfx::Rect>> {
 public:
  TwoDisplays() = default;

  TwoDisplays(const TwoDisplays&) = delete;
  TwoDisplays& operator=(const TwoDisplays&) = delete;
};

}  // namespace

TEST_P(TwoDisplays, Placement) {
  gfx::Rect primary_display_bounds = std::get<0>(GetParam());
  gfx::Rect secondary_display_bounds = std::get<1>(GetParam());
  DisplayPlacement::Position position = std::get<2>(GetParam());
  int offset = std::get<3>(GetParam());
  int minimum_offset_overlap = std::get<4>(GetParam());
  gfx::Rect expected_primary_display_bounds = std::get<5>(GetParam());
  gfx::Rect expected_secondary_display_bounds = std::get<6>(GetParam());

  Displays display_list;
  display_list.emplace_back(0, primary_display_bounds);
  display_list.emplace_back(1, secondary_display_bounds);
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  builder.AddDisplayPlacement(1, 0, position, offset);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(
      &display_list, &updated_ids, minimum_offset_overlap);

  ASSERT_EQ(1u, updated_ids.size());
  EXPECT_EQ(1u, updated_ids[0]);
  ASSERT_EQ(2u, display_list.size());
  EXPECT_EQ(expected_primary_display_bounds, display_list[0].bounds());
  EXPECT_EQ(expected_secondary_display_bounds, display_list[1].bounds());
}

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestZero,
    TwoDisplays,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, 0, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, 0, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 600, 1024, 768))));

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestOffset,
    TwoDisplays,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    37,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, 37, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    37,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(37, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    37,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, 37, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    37,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(37, 600, 1024, 768))));

INSTANTIATE_TEST_SUITE_P(DisplayLayoutTestCorner,
                         TwoDisplays,
                         testing::Values(
                             // Top-Left
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::LEFT,
                                             -60,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, -60, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::TOP,
                                             -30,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, -60, 30, 60)),
                             // Top-Right
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::RIGHT,
                                             -60,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, -60, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::TOP,
                                             20,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, -60, 30, 60)),
                             // Bottom-Right
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::RIGHT,
                                             40,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, 40, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::BOTTOM,
                                             20,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, 40, 30, 60)),
                             // Bottom-Left
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::LEFT,
                                             40,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, 40, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::BOTTOM,
                                             -30,
                                             0,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, 40, 30, 60))));

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestZeroMinimumOverlap,
    TwoDisplays,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    0,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, 0, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    0,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    0,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, 0, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    0,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 600, 1024, 768))));

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestOffsetMinimumOverlap,
    TwoDisplays,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    37,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, 37, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    37,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(37, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    37,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, 37, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    37,
                                    14,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(37, 600, 1024, 768))));

INSTANTIATE_TEST_SUITE_P(DisplayLayoutTestMinimumOverlap,
                         TwoDisplays,
                         testing::Values(
                             // Top-Left
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::LEFT,
                                             -60,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, -46, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::TOP,
                                             -30,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-16, -60, 30, 60)),
                             // Top-Right
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::RIGHT,
                                             -60,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, -46, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::TOP,
                                             20,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(6, -60, 30, 60)),
                             // Bottom-Right
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::RIGHT,
                                             40,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(20, 26, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::BOTTOM,
                                             20,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(6, 40, 30, 60)),
                             // Bottom-Left
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::LEFT,
                                             40,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-30, 26, 30, 60)),
                             std::make_tuple(gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(0, 0, 30, 60),
                                             DisplayPlacement::Position::BOTTOM,
                                             -30,
                                             14,
                                             gfx::Rect(0, 0, 20, 40),
                                             gfx::Rect(-16, 40, 30, 60))));

// Display Layout
//     [1]  [4]
//    [0][3]   [6]
// [2]  [5]
TEST(DisplayLayoutTest, MultipleDisplays) {
  Displays display_list;
  display_list.emplace_back(0, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(1, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(2, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(3, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(4, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(5, gfx::Rect(0, 0, 100, 100));
  display_list.emplace_back(6, gfx::Rect(0, 0, 100, 100));
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  builder.AddDisplayPlacement(1, 0, DisplayPlacement::Position::TOP, 50);
  builder.AddDisplayPlacement(2, 0, DisplayPlacement::Position::LEFT, 100);
  builder.AddDisplayPlacement(3, 0, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(4, 3, DisplayPlacement::Position::RIGHT, -100);
  builder.AddDisplayPlacement(5, 3, DisplayPlacement::Position::BOTTOM, -50);
  builder.AddDisplayPlacement(6, 4, DisplayPlacement::Position::BOTTOM, 100);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(&display_list, &updated_ids, 0);

  ASSERT_EQ(6u, updated_ids.size());
  std::sort(updated_ids.begin(), updated_ids.end());
  EXPECT_EQ(1u, updated_ids[0]);
  EXPECT_EQ(2u, updated_ids[1]);
  EXPECT_EQ(3u, updated_ids[2]);
  EXPECT_EQ(4u, updated_ids[3]);
  EXPECT_EQ(5u, updated_ids[4]);
  EXPECT_EQ(6u, updated_ids[5]);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), display_list[0].bounds());
  EXPECT_EQ(gfx::Rect(50, -100, 100, 100), display_list[1].bounds());
  EXPECT_EQ(gfx::Rect(-100, 100, 100, 100), display_list[2].bounds());
  EXPECT_EQ(gfx::Rect(100, 0, 100, 100), display_list[3].bounds());
  EXPECT_EQ(gfx::Rect(200, -100, 100, 100), display_list[4].bounds());
  EXPECT_EQ(gfx::Rect(50, 100, 100, 100), display_list[5].bounds());
  EXPECT_EQ(gfx::Rect(300, 0, 100, 100), display_list[6].bounds());
}

namespace {

class TwoDisplaysBottomRightReference
  : public testing::TestWithParam<std::tuple<
        // Primary Display Bounds
        gfx::Rect,
        // Secondary Display Bounds
        gfx::Rect,
        // Secondary Layout Position
        DisplayPlacement::Position,
        // Secondary Layout Offset
        int,
        // Minimum Offset Overlap
        int,
        // Expected Primary Display Bounds
        gfx::Rect,
        // Expected Secondary Display Bounds
        gfx::Rect>> {
 public:
  TwoDisplaysBottomRightReference() = default;

  TwoDisplaysBottomRightReference(const TwoDisplaysBottomRightReference&) =
      delete;
  TwoDisplaysBottomRightReference& operator=(
      const TwoDisplaysBottomRightReference&) = delete;
};

}  // namespace

TEST_P(TwoDisplaysBottomRightReference, Placement) {
  gfx::Rect primary_display_bounds = std::get<0>(GetParam());
  gfx::Rect secondary_display_bounds = std::get<1>(GetParam());
  DisplayPlacement::Position position = std::get<2>(GetParam());
  int offset = std::get<3>(GetParam());
  int minimum_offset_overlap = std::get<4>(GetParam());
  gfx::Rect expected_primary_display_bounds = std::get<5>(GetParam());
  gfx::Rect expected_secondary_display_bounds = std::get<6>(GetParam());

  Displays display_list;
  display_list.emplace_back(0, primary_display_bounds);
  display_list.emplace_back(1, secondary_display_bounds);
  std::vector<int64_t> updated_ids;

  DisplayLayoutBuilder builder(0);
  DisplayPlacement placement;
  placement.display_id = 1;
  placement.parent_display_id = 0;
  placement.position = position;
  placement.offset = offset;
  placement.offset_reference = DisplayPlacement::OffsetReference::BOTTOM_RIGHT;
  builder.AddDisplayPlacement(placement);
  std::unique_ptr<DisplayLayout> display_layout(builder.Build());
  display_layout->ApplyToDisplayList(
      &display_list, &updated_ids, minimum_offset_overlap);

  ASSERT_EQ(1u, updated_ids.size());
  EXPECT_EQ(1u, updated_ids[0]);
  ASSERT_EQ(2u, display_list.size());
  EXPECT_EQ(expected_primary_display_bounds, display_list[0].bounds());
  EXPECT_EQ(expected_secondary_display_bounds, display_list[1].bounds());
}

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestZero,
    TwoDisplaysBottomRightReference,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, -168, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-224, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, -168, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    0,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-224, 600, 1024, 768))));

INSTANTIATE_TEST_SUITE_P(
    DisplayLayoutTestOffset,
    TwoDisplaysBottomRightReference,
    testing::Values(std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::LEFT,
                                    7,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-1024, -175, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::TOP,
                                    7,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-231, -768, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::RIGHT,
                                    7,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(800, -175, 1024, 768)),
                    std::make_tuple(gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(0, 0, 1024, 768),
                                    DisplayPlacement::Position::BOTTOM,
                                    7,
                                    0,
                                    gfx::Rect(0, 0, 800, 600),
                                    gfx::Rect(-231, 600, 1024, 768))));

}  // namespace display
