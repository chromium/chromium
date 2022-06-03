// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ui/gfx/selection_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace gfx {

TEST(SelectionModelTest, Construction) {
  {
    SelectionModel selection_model;
    EXPECT_EQ(selection_model.selection(), Range(0));
    EXPECT_EQ(selection_model.caret_pos(), 0u);
    EXPECT_EQ(selection_model.secondary_selections(), std::vector<Range>());
  }
  {
    SelectionModel selection_model{5, CURSOR_FORWARD};
    EXPECT_EQ(selection_model.selection(), Range(5));
    EXPECT_EQ(selection_model.caret_pos(), 5u);
    EXPECT_EQ(selection_model.secondary_selections(), std::vector<Range>());
  }
  {
    SelectionModel selection_model{{3, 2}, CURSOR_BACKWARD};
    EXPECT_EQ(selection_model.selection(), Range(3, 2));
    EXPECT_EQ(selection_model.caret_pos(), 2u);
    EXPECT_EQ(selection_model.secondary_selections(), std::vector<Range>());
  }
  {
    SelectionModel selection_model{{{2, 3}, {5, 5}, {1, 0}}, CURSOR_BACKWARD};
    EXPECT_EQ(selection_model.selection(), Range(2, 3));
    EXPECT_EQ(selection_model.caret_pos(), 3u);
    EXPECT_EQ(selection_model.secondary_selections(),
              std::vector<Range>({{5, 5}, {1, 0}}));
  }
}

TEST(SelectionModelTest, AddSecondarySelection) {
  SelectionModel selection_model;
  selection_model.AddSecondarySelection({5, 6});
  selection_model.AddSecondarySelection({7, 6});
  selection_model.AddSecondarySelection({8, 8});
  EXPECT_EQ(selection_model.selection(), Range(0));
  EXPECT_EQ(selection_model.caret_pos(), 0u);
  EXPECT_EQ(selection_model.secondary_selections(),
            std::vector<Range>({{5, 6}, {7, 6}, {8, 8}}));
}

TEST(SelectionModelTest, GetAllSelections) {
  SelectionModel selection_model{{3, 2}, CURSOR_BACKWARD};
  selection_model.AddSecondarySelection({5, 6});
  selection_model.AddSecondarySelection({7, 6});
  selection_model.AddSecondarySelection({8, 8});
  EXPECT_EQ(selection_model.GetAllSelections(),
            std::vector<Range>({{3, 2}, {5, 6}, {7, 6}, {8, 8}}));
}

TEST(SelectionModelTest, EqualityOperators) {
  SelectionModel selection_model{{3, 2}, CURSOR_BACKWARD};
  selection_model.AddSecondarySelection({5, 6});
  selection_model.AddSecondarySelection({7, 6});
  selection_model.AddSecondarySelection({8, 8});

  // Equal
  EXPECT_EQ(selection_model,
            SelectionModel({{3, 2}, {5, 6}, {7, 6}, {8, 8}}, CURSOR_BACKWARD));
  // Unequal selection
  EXPECT_NE(selection_model,
            SelectionModel({{3, 3}, {5, 6}, {7, 6}, {8, 8}}, CURSOR_BACKWARD));
  // Unequal secondary selections
  EXPECT_NE(selection_model,
            SelectionModel({{3, 2}, {5, 6}, {7, 6}, {9, 8}}, CURSOR_BACKWARD));
  // Unequal cursor affinity
  EXPECT_NE(selection_model,
            SelectionModel({{3, 2}, {5, 6}, {7, 6}, {8, 8}}, CURSOR_FORWARD));
}

TEST(SelectionModelTest, ToString) {
  SelectionModel selection_model{{3, 2}, CURSOR_BACKWARD};
  selection_model.AddSecondarySelection({5, 6});
  selection_model.AddSecondarySelection({7, 6});
  selection_model.AddSecondarySelection({8, 8});
  EXPECT_EQ(selection_model.ToString(), "{{3,2},BACKWARD,{5,6},{7,6},8}");
}

}  // namespace gfx
