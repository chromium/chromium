// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_utils.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/table/test_table_model.h"

using ui::TableColumn;
using ui::TableModel;

namespace views {

namespace {

std::string IntVectorToString(const std::vector<int>& values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0)
      result += ",";
    result += base::NumberToString(values[i]);
  }
  return result;
}

ui::TableColumn CreateTableColumnWithWidth(int width) {
  ui::TableColumn column;
  column.width = width;
  return column;
}

}  // namespace

// Verifies columns with a specified width is honored.
TEST(TableUtilsTest, SetWidthHonored) {
  TestTableModel model(4);
  std::vector<TableColumn> columns;
  columns.push_back(CreateTableColumnWithWidth(20));
  columns.push_back(CreateTableColumnWithWidth(30));
  gfx::FontList font_list;
  std::vector<int> result(CalculateTableColumnSizes(
      100, 0, font_list, font_list, 0, 0, columns, &model));
  EXPECT_EQ("20,30", IntVectorToString(result));

  // Same with some padding, it should be ignored.
  result = CalculateTableColumnSizes(
      100, 0, font_list, font_list, 2, 0, columns, &model);
  EXPECT_EQ("20,30", IntVectorToString(result));

  // Same with not enough space, it shouldn't matter.
  result = CalculateTableColumnSizes(
      10, 0, font_list, font_list, 2, 0, columns, &model);
  EXPECT_EQ("20,30", IntVectorToString(result));
}

// Verifies if no size is specified the last column gets all the available
// space.
TEST(TableUtilsTest, LastColumnGetsAllSpace) {
  TestTableModel model(4);
  std::vector<TableColumn> columns;
  columns.emplace_back();
  columns.emplace_back();
  gfx::FontList font_list;
  std::vector<int> result(CalculateTableColumnSizes(
      500, 0, font_list, font_list, 0, 0, columns, &model));
  EXPECT_NE(0, result[0]);
  EXPECT_GE(result[1],
            WidthForContent(font_list, font_list, 0, 0, columns[1], &model));
  EXPECT_EQ(500, result[0] + result[1]);
}

// Verifies a single column with a percent=1 is resized correctly.
TEST(TableUtilsTest, SingleResizableColumn) {
  TestTableModel model(4);
  std::vector<TableColumn> columns;
  columns.emplace_back();
  columns.emplace_back();
  columns.emplace_back();
  columns[2].percent = 1.0f;
  gfx::FontList font_list;
  std::vector<int> result(CalculateTableColumnSizes(
      500, 0, font_list, font_list, 0, 0, columns, &model));
  EXPECT_EQ(result[0],
            WidthForContent(font_list, font_list, 0, 0, columns[0], &model));
  EXPECT_EQ(result[1],
            WidthForContent(font_list, font_list, 0, 0, columns[1], &model));
  EXPECT_EQ(500 - result[0] - result[1], result[2]);

  // The same with a slightly larger width passed in.
  result = CalculateTableColumnSizes(
      1000, 0, font_list, font_list, 0, 0, columns, &model);
  EXPECT_EQ(result[0],
            WidthForContent(font_list, font_list, 0, 0, columns[0], &model));
  EXPECT_EQ(result[1],
            WidthForContent(font_list, font_list, 0, 0, columns[1], &model));
  EXPECT_EQ(1000 - result[0] - result[1], result[2]);

  // Verify padding for the first column is honored.
  result = CalculateTableColumnSizes(
      1000, 10, font_list, font_list, 0, 0, columns, &model);
  EXPECT_EQ(result[0],
            WidthForContent(font_list, font_list, 0, 0, columns[0], &model)
                + 10);
  EXPECT_EQ(result[1],
            WidthForContent(font_list, font_list, 0, 0, columns[1], &model));
  EXPECT_EQ(1000 - result[0] - result[1], result[2]);

  // Just enough space to show the first two columns. Should force last column
  // to min size.
  result = CalculateTableColumnSizes(
      1000, 0, font_list, font_list, 0, 0, columns, &model);
  result = CalculateTableColumnSizes(
      result[0] + result[1], 0, font_list, font_list, 0, 0, columns, &model);
  EXPECT_EQ(result[0],
            WidthForContent(font_list, font_list, 0, 0, columns[0], &model));
  EXPECT_EQ(result[1],
            WidthForContent(font_list, font_list, 0, 0, columns[1], &model));
  EXPECT_EQ(kUnspecifiedColumnWidth, result[2]);
}

}  // namespace views
