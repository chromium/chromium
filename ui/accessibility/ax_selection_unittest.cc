// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_selection.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/test_single_ax_tree_manager.h"

// Helper macro for testing selection values and maintain
// correct stack tracing and failure causality.
#define TEST_SELECTION(tree_update, tree, input, expected)         \
  {                                                                \
    tree_update.has_tree_data = true;                              \
    tree_update.tree_data.sel_anchor_object_id = input.anchor_id;  \
    tree_update.tree_data.sel_anchor_offset = input.anchor_offset; \
    tree_update.tree_data.sel_focus_object_id = input.focus_id;    \
    tree_update.tree_data.sel_focus_offset = input.focus_offset;   \
    EXPECT_TRUE(tree->Unserialize(tree_update));                   \
    AXSelection actual = tree->GetUnignoredSelection();            \
    EXPECT_EQ(expected.anchor_id, actual.anchor_object_id);        \
    EXPECT_EQ(expected.anchor_offset, actual.anchor_offset);       \
    EXPECT_EQ(expected.focus_id, actual.focus_object_id);          \
    EXPECT_EQ(expected.focus_offset, actual.focus_offset);         \
  }

namespace ui {

TEST(AXSelectionTest, UnignoredSelection) {
  AXTreeUpdate tree_update;
  // (i) => node is ignored
  // 1
  // |__________
  // |     |   |
  // 2(i)  3   4
  // |_______________________
  // |   |      |           |
  // 5   6      7(i)        8(i)
  // |   |      |________
  // |   |      |       |
  // 9   10(i)  11(i)  12
  //     |      |____
  //     |      |   |
  //     13(i)  14  15
  //     |
  //     16
  // Unignored Tree (conceptual)
  // 1
  // |______________________
  // |  |    |   |   |  |  |
  // 5  6   14  15  12  3  4
  // |  |
  // 9  16
  tree_update.has_tree_data = true;
  tree_update.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_update.root_id = 1;
  tree_update.nodes.resize(16);
  tree_update.nodes[0].id = 1;
  tree_update.nodes[0].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[0].child_ids = {2, 3, 4};

  tree_update.nodes[1].id = 2;
  tree_update.nodes[1].child_ids = {5, 6, 7, 8};
  tree_update.nodes[1].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[1].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[2].id = 3;
  tree_update.nodes[2].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[2].SetName("text");

  tree_update.nodes[3].id = 4;
  tree_update.nodes[3].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[3].SetName("text");

  tree_update.nodes[4].id = 5;
  tree_update.nodes[4].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[4].child_ids = {9};

  tree_update.nodes[5].id = 6;
  tree_update.nodes[5].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[5].child_ids = {10};

  tree_update.nodes[6].id = 7;
  tree_update.nodes[6].child_ids = {11, 12};
  tree_update.nodes[6].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[6].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[7].id = 8;
  tree_update.nodes[7].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[7].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[8].id = 9;
  tree_update.nodes[8].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[8].SetName("text");

  tree_update.nodes[9].id = 10;
  tree_update.nodes[9].child_ids = {13};
  tree_update.nodes[9].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[9].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[10].id = 11;
  tree_update.nodes[10].child_ids = {14, 15};
  tree_update.nodes[10].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[10].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[11].id = 12;
  tree_update.nodes[11].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[11].SetName("text");

  tree_update.nodes[12].id = 13;
  tree_update.nodes[12].child_ids = {16};
  tree_update.nodes[12].role = ax::mojom::Role::kGenericContainer;
  tree_update.nodes[12].AddState(ax::mojom::State::kIgnored);

  tree_update.nodes[13].id = 14;
  tree_update.nodes[13].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[13].SetName("text");

  tree_update.nodes[14].id = 15;
  tree_update.nodes[14].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[14].SetName("text");

  tree_update.nodes[15].id = 16;
  tree_update.nodes[15].role = ax::mojom::Role::kStaticText;
  tree_update.nodes[15].SetName("text");

  TestSingleAXTreeManager test_ax_tree_manager(
      std::make_unique<AXTree>(tree_update));
  AXSelection unignored_selection =
      test_ax_tree_manager.GetTree()->GetUnignoredSelection();

  EXPECT_EQ(kInvalidAXNodeID, unignored_selection.anchor_object_id);
  EXPECT_EQ(-1, unignored_selection.anchor_offset);
  EXPECT_EQ(kInvalidAXNodeID, unignored_selection.focus_object_id);
  EXPECT_EQ(-1, unignored_selection.focus_offset);
  struct SelectionData {
    int32_t anchor_id;
    int32_t anchor_offset;
    int32_t focus_id;
    int32_t focus_offset;
  };

  SelectionData input = {1, 0, 1, 0};
  SelectionData expected = {9, 0, 9, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {1, 0, 2, 2};
  expected = {9, 0, 14, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {2, 1, 5, 0};
  expected = {16, 0, 5, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {5, 0, 9, 0};
  expected = {5, 0, 9, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {9, 0, 6, 0};
  expected = {9, 0, 16, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {6, 0, 10, 0};
  expected = {16, 0, 16, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {10, 0, 13, 0};
  expected = {16, 0, 16, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {13, 0, 16, 0};
  expected = {16, 0, 16, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {16, 0, 7, 0};
  expected = {16, 0, 14, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {7, 0, 11, 0};
  expected = {14, 0, 14, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {11, 1, 14, 2};
  expected = {15, 0, 14, 2};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {14, 2, 15, 3};
  expected = {14, 2, 15, 3};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {15, 0, 12, 0};
  expected = {15, 0, 12, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {12, 0, 8, 0};
  expected = {12, 0, 3, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {8, 0, 3, 0};
  expected = {12, 4, 3, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {3, 0, 4, 0};
  expected = {3, 0, 4, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);

  input = {4, 0, 4, 0};
  expected = {4, 0, 4, 0};
  TEST_SELECTION(tree_update, test_ax_tree_manager.GetTree(), input, expected);
}

}  // namespace ui
