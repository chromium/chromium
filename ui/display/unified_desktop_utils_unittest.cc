// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/unified_desktop_utils.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/types/display_constants.h"

namespace display {

TEST(UnifiedDesktopLayoutTests, ValidateMatrix) {
  UnifiedDesktopLayoutMatrix matrix;

  // Empty matrix.
  EXPECT_FALSE(ValidateMatrix(matrix));

  // Matrix with unequal row sizes.
  matrix.resize(2);
  matrix[0].emplace_back(1);
  matrix[0].emplace_back(2);
  matrix[1].emplace_back(3);
  EXPECT_FALSE(ValidateMatrix(matrix));

  // Matrix with a hole.
  matrix[1].emplace_back(display::kInvalidDisplayId);
  EXPECT_FALSE(ValidateMatrix(matrix));
}

TEST(UnifiedDesktopLayoutTests, PrimaryIdNotInList) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(BuildUnifiedDesktopMatrix({30, 40, 50}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, ExtraPlacement) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(BuildUnifiedDesktopMatrix({20, 30, 40}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, MissingPlacement) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(BuildUnifiedDesktopMatrix({20, 30, 40, 50}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, PrimaryIsNotRoot) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 60, DisplayPlacement::Position::LEFT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 40, DisplayPlacement::Position::LEFT, 0);
  builder.AddDisplayPlacement(60, 40, DisplayPlacement::Position::BOTTOM, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(
      BuildUnifiedDesktopMatrix({20, 30, 40, 50, 60}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, CycleThroughPrimary) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(20, 40, DisplayPlacement::Position::BOTTOM, 0);
  builder.AddDisplayPlacement(40, 50, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(50, 30, DisplayPlacement::Position::TOP, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(BuildUnifiedDesktopMatrix({20, 30, 40, 50}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, NoPlacementOffsets) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 20);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(BuildUnifiedDesktopMatrix({20, 30, 40, 50}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, TwoChildrenOnOneSide) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(60, 30, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(70, 30, DisplayPlacement::Position::RIGHT, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(
      BuildUnifiedDesktopMatrix({20, 30, 40, 50, 60, 70}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, EmptyHoles) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(60, 30, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(70, 50, DisplayPlacement::Position::LEFT, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_FALSE(
      BuildUnifiedDesktopMatrix({20, 30, 40, 50, 60, 70}, *layout, &matrix));
  EXPECT_TRUE(matrix.empty());
}

TEST(UnifiedDesktopLayoutTests, ValidHorizontalMatrix) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(50, 40, DisplayPlacement::Position::RIGHT, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_TRUE(BuildUnifiedDesktopMatrix({20, 30, 40, 50}, *layout, &matrix));
  EXPECT_FALSE(matrix.empty());
  // 1 x 4 matrix.
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(4u, matrix[0].size());

  // [[20, 30, 40, 50]].
  EXPECT_EQ(20, matrix[0][0]);
  EXPECT_EQ(30, matrix[0][1]);
  EXPECT_EQ(40, matrix[0][2]);
  EXPECT_EQ(50, matrix[0][3]);
}

TEST(UnifiedDesktopLayoutTests, ValidHorizontalMatrixReverse) {
  DisplayLayoutBuilder builder(60);
  builder.AddDisplayPlacement(50, 60, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 50, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(30, 40, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(20, 30, DisplayPlacement::Position::RIGHT, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_TRUE(
      BuildUnifiedDesktopMatrix({20, 30, 40, 50, 60}, *layout, &matrix));
  EXPECT_FALSE(matrix.empty());
  // 1 x 4 matrix.
  EXPECT_EQ(1u, matrix.size());
  EXPECT_EQ(5u, matrix[0].size());

  // [[60, 50, 40, 30, 20]].
  EXPECT_EQ(60, matrix[0][0]);
  EXPECT_EQ(50, matrix[0][1]);
  EXPECT_EQ(40, matrix[0][2]);
  EXPECT_EQ(30, matrix[0][3]);
  EXPECT_EQ(20, matrix[0][4]);
}

TEST(UnifiedDesktopLayoutTests, ValidVerticalMatrix) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::BOTTOM, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::BOTTOM, 0);
  builder.AddDisplayPlacement(50, 40, DisplayPlacement::Position::BOTTOM, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_TRUE(BuildUnifiedDesktopMatrix({20, 30, 40, 50}, *layout, &matrix));
  EXPECT_FALSE(matrix.empty());
  // 4 x 1 matrix.
  EXPECT_EQ(4u, matrix.size());
  EXPECT_EQ(1u, matrix[0].size());

  // [[20],
  //  [30],
  //  [40],
  //  [50]].
  EXPECT_EQ(20, matrix[0][0]);
  EXPECT_EQ(30, matrix[1][0]);
  EXPECT_EQ(40, matrix[2][0]);
  EXPECT_EQ(50, matrix[3][0]);
}

TEST(UnifiedDesktopLayoutTests, ValidGridMatrix) {
  DisplayLayoutBuilder builder(20);
  builder.AddDisplayPlacement(30, 20, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(40, 30, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(50, 20, DisplayPlacement::Position::TOP, 0);
  builder.AddDisplayPlacement(60, 30, DisplayPlacement::Position::RIGHT, 0);
  builder.AddDisplayPlacement(70, 40, DisplayPlacement::Position::RIGHT, 0);
  UnifiedDesktopLayoutMatrix matrix;
  std::unique_ptr<DisplayLayout> layout = builder.Build();
  EXPECT_TRUE(
      BuildUnifiedDesktopMatrix({20, 30, 40, 50, 60, 70}, *layout, &matrix));
  EXPECT_FALSE(matrix.empty());
  // 2 x 3 matrix.
  EXPECT_EQ(2u, matrix.size());
  EXPECT_EQ(3u, matrix[0].size());

  // [[50, 40, 70],
  //  [20, 30, 60]].
  EXPECT_EQ(50, matrix[0][0]);
  EXPECT_EQ(40, matrix[0][1]);
  EXPECT_EQ(70, matrix[0][2]);
  EXPECT_EQ(20, matrix[1][0]);
  EXPECT_EQ(30, matrix[1][1]);
  EXPECT_EQ(60, matrix[1][2]);
}

}  // namespace display
