// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_table_info.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

namespace {

void MakeTable(AXNodeData* table, int id, int row_count, int col_count) {
  table->id = id;
  table->role = ax::mojom::Role::kTable;
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, row_count);
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount, col_count);
}

void MakeRow(AXNodeData* row, int id, int row_index) {
  row->id = id;
  row->role = ax::mojom::Role::kRow;
  row->AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, row_index);
}

void MakeCell(AXNodeData* cell,
              int id,
              int row_index,
              int col_index,
              int row_span = 1,
              int col_span = 1) {
  cell->id = id;
  cell->role = ax::mojom::Role::kCell;
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, row_index);
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                        col_index);
  if (row_span > 1)
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, row_span);
  if (col_span > 1)
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                          col_span);
}

void MakeColumnHeader(AXNodeData* cell,
                      int id,
                      int row_index,
                      int col_index,
                      int row_span = 1,
                      int col_span = 1) {
  MakeCell(cell, id, row_index, col_index, row_span, col_span);
  cell->role = ax::mojom::Role::kColumnHeader;
}

void MakeRowHeader(AXNodeData* cell,
                   int id,
                   int row_index,
                   int col_index,
                   int row_span = 1,
                   int col_span = 1) {
  MakeCell(cell, id, row_index, col_index, row_span, col_span);
  cell->role = ax::mojom::Role::kRowHeader;
}

}  // namespace

class AXTableInfoTest : public testing::Test {
 public:
  AXTableInfoTest() {}
  ~AXTableInfoTest() override {}

 protected:
  AXTableInfo* GetTableInfo(AXTree* tree, AXNode* node) {
    return tree->GetTableInfo(node);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTableInfoTest);
};

TEST_F(AXTableInfoTest, SimpleTable) {
  // Simple 2 x 2 table with 2 column headers in first row, 2 cells in second
  // row.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(7);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {4, 5};
  MakeRow(&initial_state.nodes[2], 3, 1);
  initial_state.nodes[2].child_ids = {6, 7};
  MakeColumnHeader(&initial_state.nodes[3], 4, 0, 0);
  MakeColumnHeader(&initial_state.nodes[4], 5, 0, 1);
  MakeCell(&initial_state.nodes[5], 6, 1, 0);
  MakeCell(&initial_state.nodes[6], 7, 1, 1);
  AXTree tree(initial_state);

  //
  // Low-level: test the AXTableInfo directly.
  //

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root()->children()[0]);
  EXPECT_FALSE(table_info);

  table_info = GetTableInfo(&tree, tree.root());
  EXPECT_TRUE(table_info);

  EXPECT_EQ(2, table_info->row_count);
  EXPECT_EQ(2, table_info->col_count);

  EXPECT_EQ(2U, table_info->row_headers.size());
  EXPECT_EQ(0U, table_info->row_headers[0].size());
  EXPECT_EQ(0U, table_info->row_headers[1].size());

  EXPECT_EQ(2U, table_info->col_headers.size());
  EXPECT_EQ(1U, table_info->col_headers[0].size());
  EXPECT_EQ(4, table_info->col_headers[0][0]);
  EXPECT_EQ(1U, table_info->col_headers[1].size());
  EXPECT_EQ(5, table_info->col_headers[1][0]);

  EXPECT_EQ(4, table_info->cell_ids[0][0]);
  EXPECT_EQ(5, table_info->cell_ids[0][1]);
  EXPECT_EQ(6, table_info->cell_ids[1][0]);
  EXPECT_EQ(7, table_info->cell_ids[1][1]);

  EXPECT_EQ(4U, table_info->unique_cell_ids.size());
  EXPECT_EQ(4, table_info->unique_cell_ids[0]);
  EXPECT_EQ(5, table_info->unique_cell_ids[1]);
  EXPECT_EQ(6, table_info->unique_cell_ids[2]);
  EXPECT_EQ(7, table_info->unique_cell_ids[3]);

  EXPECT_EQ(0, table_info->cell_id_to_index[4]);
  EXPECT_EQ(1, table_info->cell_id_to_index[5]);
  EXPECT_EQ(2, table_info->cell_id_to_index[6]);
  EXPECT_EQ(3, table_info->cell_id_to_index[7]);

  EXPECT_EQ(0U, table_info->extra_mac_nodes.size());

  //
  // High-level: Test the helper functions on AXNode.
  //

  AXNode* table = tree.root();
  EXPECT_TRUE(table->IsTable());
  EXPECT_FALSE(table->IsTableRow());
  EXPECT_FALSE(table->IsTableCellOrHeader());
  EXPECT_EQ(2, table->GetTableColCount());
  EXPECT_EQ(2, table->GetTableRowCount());

  EXPECT_EQ(4, table->GetTableCellFromCoords(0, 0)->id());
  EXPECT_EQ(5, table->GetTableCellFromCoords(0, 1)->id());
  EXPECT_EQ(6, table->GetTableCellFromCoords(1, 0)->id());
  EXPECT_EQ(7, table->GetTableCellFromCoords(1, 1)->id());
  EXPECT_EQ(nullptr, table->GetTableCellFromCoords(2, 1));
  EXPECT_EQ(nullptr, table->GetTableCellFromCoords(1, -1));

  EXPECT_EQ(4, table->GetTableCellFromIndex(0)->id());
  EXPECT_EQ(5, table->GetTableCellFromIndex(1)->id());
  EXPECT_EQ(6, table->GetTableCellFromIndex(2)->id());
  EXPECT_EQ(7, table->GetTableCellFromIndex(3)->id());
  EXPECT_EQ(nullptr, table->GetTableCellFromIndex(-1));
  EXPECT_EQ(nullptr, table->GetTableCellFromIndex(4));

  AXNode* row_0 = tree.GetFromId(2);
  EXPECT_FALSE(row_0->IsTable());
  EXPECT_TRUE(row_0->IsTableRow());
  EXPECT_FALSE(row_0->IsTableCellOrHeader());
  EXPECT_EQ(0, row_0->GetTableRowRowIndex());

  AXNode* row_1 = tree.GetFromId(3);
  EXPECT_FALSE(row_1->IsTable());
  EXPECT_TRUE(row_1->IsTableRow());
  EXPECT_FALSE(row_1->IsTableCellOrHeader());
  EXPECT_EQ(1, row_1->GetTableRowRowIndex());

  AXNode* cell_0_0 = tree.GetFromId(4);
  EXPECT_FALSE(cell_0_0->IsTable());
  EXPECT_FALSE(cell_0_0->IsTableRow());
  EXPECT_TRUE(cell_0_0->IsTableCellOrHeader());
  EXPECT_EQ(0, cell_0_0->GetTableCellIndex());
  EXPECT_EQ(0, cell_0_0->GetTableCellColIndex());
  EXPECT_EQ(0, cell_0_0->GetTableCellRowIndex());
  EXPECT_EQ(1, cell_0_0->GetTableCellColSpan());
  EXPECT_EQ(1, cell_0_0->GetTableCellRowSpan());

  AXNode* cell_1_1 = tree.GetFromId(7);
  EXPECT_FALSE(cell_1_1->IsTable());
  EXPECT_FALSE(cell_1_1->IsTableRow());
  EXPECT_TRUE(cell_1_1->IsTableCellOrHeader());
  EXPECT_EQ(3, cell_1_1->GetTableCellIndex());
  EXPECT_EQ(1, cell_1_1->GetTableCellColIndex());
  EXPECT_EQ(1, cell_1_1->GetTableCellRowIndex());
  EXPECT_EQ(1, cell_1_1->GetTableCellColSpan());
  EXPECT_EQ(1, cell_1_1->GetTableCellRowSpan());

  std::vector<AXNode*> col_headers;
  cell_1_1->GetTableCellColHeaders(&col_headers);
  EXPECT_EQ(1U, col_headers.size());
  EXPECT_EQ(5, col_headers[0]->id());

  std::vector<AXNode*> row_headers;
  cell_1_1->GetTableCellRowHeaders(&row_headers);
  EXPECT_EQ(0U, row_headers.size());
}

TEST_F(AXTableInfoTest, ComputedTableSizeIncludesSpans) {
  // Simple 2 x 2 table with 2 column headers in first row, 2 cells in second
  // row, but two cells have spans, affecting the computed row and column count.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(7);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {4, 5};
  MakeRow(&initial_state.nodes[2], 3, 1);
  initial_state.nodes[2].child_ids = {6, 7};
  MakeCell(&initial_state.nodes[3], 4, 0, 0);
  MakeCell(&initial_state.nodes[4], 5, 0, 1, 1, 5);  // Column span of 5
  MakeCell(&initial_state.nodes[5], 6, 1, 0);
  MakeCell(&initial_state.nodes[6], 7, 1, 1, 3, 1);  // Row span of 3
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(4, table_info->row_count);
  EXPECT_EQ(6, table_info->col_count);
}

TEST_F(AXTableInfoTest, AuthorRowAndColumnCountsAreRespected) {
  // Simple 1 x 1 table, but the table's authored row and column
  // counts imply a larger table (with missing cells).
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  MakeTable(&initial_state.nodes[0], 1, 8, 9);
  initial_state.nodes[0].child_ids = {2};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {3};
  MakeCell(&initial_state.nodes[2], 2, 0, 1);
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(8, table_info->row_count);
  EXPECT_EQ(9, table_info->col_count);
}

TEST_F(AXTableInfoTest, TableInfoRecomputedOnlyWhenTableChanges) {
  // Simple 1 x 1 table.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {3};
  MakeCell(&initial_state.nodes[2], 3, 0, 0);
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(1, table_info->row_count);
  EXPECT_EQ(1, table_info->col_count);

  // Table info is cached.
  AXTableInfo* table_info_2 = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(table_info, table_info_2);

  // Update the table so that the cell has a span.
  AXTreeUpdate update = initial_state;
  MakeCell(&update.nodes[2], 3, 0, 0, 1, 2);
  EXPECT_TRUE(tree.Unserialize(update));

  AXTableInfo* table_info_3 = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(1, table_info_3->row_count);
  EXPECT_EQ(2, table_info_3->col_count);
}

TEST_F(AXTableInfoTest, CellIdsHandlesSpansAndMissingCells) {
  // 3 column x 2 row table with spans and missing cells:
  //
  // +---+---+---+
  // |   |   5   |
  // + 4 +---+---+
  // |   | 6 |
  // +---+---+
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {4, 5};
  MakeRow(&initial_state.nodes[2], 3, 1);
  initial_state.nodes[2].child_ids = {6};
  MakeCell(&initial_state.nodes[3], 4, 0, 0, 2, 1);  // Row span of 2
  MakeCell(&initial_state.nodes[4], 5, 0, 1, 1, 5);  // Column span of 2
  MakeCell(&initial_state.nodes[5], 6, 1, 1);
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root());
  EXPECT_EQ(4, table_info->cell_ids[0][0]);
  EXPECT_EQ(5, table_info->cell_ids[0][1]);
  EXPECT_EQ(5, table_info->cell_ids[0][1]);
  EXPECT_EQ(4, table_info->cell_ids[1][0]);
  EXPECT_EQ(6, table_info->cell_ids[1][1]);
  EXPECT_EQ(0, table_info->cell_ids[1][2]);

  EXPECT_EQ(3U, table_info->unique_cell_ids.size());
  EXPECT_EQ(4, table_info->unique_cell_ids[0]);
  EXPECT_EQ(5, table_info->unique_cell_ids[1]);
  EXPECT_EQ(6, table_info->unique_cell_ids[2]);

  EXPECT_EQ(0, table_info->cell_id_to_index[4]);
  EXPECT_EQ(1, table_info->cell_id_to_index[5]);
  EXPECT_EQ(2, table_info->cell_id_to_index[6]);
}

TEST_F(AXTableInfoTest, SkipsGenericAndIgnoredNodes) {
  // Simple 2 x 2 table with 2 cells in the first row, 2 cells in the second
  // row, but with extra divs and ignored nodes in the tree.
  //
  // 1 Table
  //   2 Row
  //     3 Ignored
  //       4 Generic
  //         5 Cell
  //       6 Cell
  //   7 Ignored
  //     8 Row
  //       9 Cell
  //       10 Cell

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(10);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 7};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {3};
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids = {4, 6};
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[3].child_ids = {5};
  MakeCell(&initial_state.nodes[4], 5, 0, 0);
  MakeCell(&initial_state.nodes[5], 6, 0, 1);
  initial_state.nodes[6].id = 7;
  initial_state.nodes[6].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[6].child_ids = {8};
  MakeRow(&initial_state.nodes[7], 8, 1);
  initial_state.nodes[7].child_ids = {9, 10};
  MakeCell(&initial_state.nodes[8], 9, 1, 0);
  MakeCell(&initial_state.nodes[9], 10, 1, 1);
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root()->children()[0]);
  EXPECT_FALSE(table_info);

  table_info = GetTableInfo(&tree, tree.root());
  EXPECT_TRUE(table_info);

  EXPECT_EQ(2, table_info->row_count);
  EXPECT_EQ(2, table_info->col_count);

  EXPECT_EQ(5, table_info->cell_ids[0][0]);
  EXPECT_EQ(6, table_info->cell_ids[0][1]);
  EXPECT_EQ(9, table_info->cell_ids[1][0]);
  EXPECT_EQ(10, table_info->cell_ids[1][1]);
}

TEST_F(AXTableInfoTest, HeadersWithSpans) {
  // Row and column headers spanning multiple cells.
  // In the figure below, 5 and 6 are headers.
  //
  //     +---+---+
  //     |   5   |
  // +---+---+---+
  // |   | 7 |
  // + 6 +---+---+
  // |   |   | 8 |
  // +---+   +---+
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(8);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3, 4};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {5};
  MakeRow(&initial_state.nodes[2], 3, 1);
  initial_state.nodes[2].child_ids = {6, 7};
  MakeRow(&initial_state.nodes[3], 4, 2);
  initial_state.nodes[3].child_ids = {8};
  MakeColumnHeader(&initial_state.nodes[4], 5, 0, 1, 1, 2);
  MakeRowHeader(&initial_state.nodes[5], 6, 1, 0, 2, 1);
  MakeCell(&initial_state.nodes[6], 7, 1, 1);
  MakeCell(&initial_state.nodes[7], 8, 2, 2);
  AXTree tree(initial_state);

  AXTableInfo* table_info = GetTableInfo(&tree, tree.root()->children()[0]);
  EXPECT_FALSE(table_info);

  table_info = GetTableInfo(&tree, tree.root());
  EXPECT_TRUE(table_info);

  EXPECT_EQ(3U, table_info->row_headers.size());
  EXPECT_EQ(0U, table_info->row_headers[0].size());
  EXPECT_EQ(1U, table_info->row_headers[1].size());
  EXPECT_EQ(6, table_info->row_headers[1][0]);
  EXPECT_EQ(1U, table_info->row_headers[1].size());
  EXPECT_EQ(6, table_info->row_headers[2][0]);

  EXPECT_EQ(3U, table_info->col_headers.size());
  EXPECT_EQ(0U, table_info->col_headers[0].size());
  EXPECT_EQ(1U, table_info->col_headers[1].size());
  EXPECT_EQ(5, table_info->col_headers[1][0]);
  EXPECT_EQ(1U, table_info->col_headers[2].size());
  EXPECT_EQ(5, table_info->col_headers[2][0]);

  EXPECT_EQ(0, table_info->cell_ids[0][0]);
  EXPECT_EQ(5, table_info->cell_ids[0][1]);
  EXPECT_EQ(5, table_info->cell_ids[0][2]);
  EXPECT_EQ(6, table_info->cell_ids[1][0]);
  EXPECT_EQ(7, table_info->cell_ids[1][1]);
  EXPECT_EQ(0, table_info->cell_ids[1][2]);
  EXPECT_EQ(6, table_info->cell_ids[2][0]);
  EXPECT_EQ(0, table_info->cell_ids[2][1]);
  EXPECT_EQ(8, table_info->cell_ids[2][2]);
}

TEST_F(AXTableInfoTest, ExtraMacNodes) {
  // Simple 2 x 2 table with 2 column headers in first row, 2 cells in second
  // row.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(7);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3};
  MakeRow(&initial_state.nodes[1], 2, 0);
  initial_state.nodes[1].child_ids = {4, 5};
  MakeRow(&initial_state.nodes[2], 3, 1);
  initial_state.nodes[2].child_ids = {6, 7};
  MakeColumnHeader(&initial_state.nodes[3], 4, 0, 0);
  MakeColumnHeader(&initial_state.nodes[4], 5, 0, 1);
  MakeCell(&initial_state.nodes[5], 6, 1, 0);
  MakeCell(&initial_state.nodes[6], 7, 1, 1);
  AXTree tree(initial_state);

  tree.SetEnableExtraMacNodes(true);
  AXTableInfo* table_info = GetTableInfo(&tree, tree.root()->children()[0]);
  EXPECT_FALSE(table_info);

  table_info = GetTableInfo(&tree, tree.root());
  EXPECT_TRUE(table_info);

  // We expect 3 extra Mac nodes: two column nodes, and one header node.
  EXPECT_EQ(3U, table_info->extra_mac_nodes.size());

  // The first column.
  AXNodeData extra_node_0 = table_info->extra_mac_nodes[0]->data();
  EXPECT_EQ(-1, table_info->extra_mac_nodes[0]->id());
  EXPECT_EQ(1, table_info->extra_mac_nodes[0]->parent()->id());
  EXPECT_EQ(ax::mojom::Role::kColumn, extra_node_0.role);
  EXPECT_EQ(0, extra_node_0.GetIntAttribute(
                   ax::mojom::IntAttribute::kTableColumnIndex));
  std::vector<int32_t> indirect_child_ids;
  EXPECT_EQ(true, extra_node_0.GetIntListAttribute(
                      ax::mojom::IntListAttribute::kIndirectChildIds,
                      &indirect_child_ids));
  EXPECT_EQ(2U, indirect_child_ids.size());
  EXPECT_EQ(4, indirect_child_ids[0]);
  EXPECT_EQ(6, indirect_child_ids[1]);

  // The second column.
  AXNodeData extra_node_1 = table_info->extra_mac_nodes[1]->data();
  EXPECT_EQ(-2, table_info->extra_mac_nodes[1]->id());
  EXPECT_EQ(1, table_info->extra_mac_nodes[1]->parent()->id());
  EXPECT_EQ(ax::mojom::Role::kColumn, extra_node_1.role);
  EXPECT_EQ(1, extra_node_1.GetIntAttribute(
                   ax::mojom::IntAttribute::kTableColumnIndex));
  indirect_child_ids.clear();
  EXPECT_EQ(true, extra_node_1.GetIntListAttribute(
                      ax::mojom::IntListAttribute::kIndirectChildIds,
                      &indirect_child_ids));
  EXPECT_EQ(2U, indirect_child_ids.size());
  EXPECT_EQ(5, indirect_child_ids[0]);
  EXPECT_EQ(7, indirect_child_ids[1]);

  // The table header container.
  AXNodeData extra_node_2 = table_info->extra_mac_nodes[2]->data();
  EXPECT_EQ(-3, table_info->extra_mac_nodes[2]->id());
  EXPECT_EQ(1, table_info->extra_mac_nodes[2]->parent()->id());
  EXPECT_EQ(ax::mojom::Role::kTableHeaderContainer, extra_node_2.role);
  indirect_child_ids.clear();
  EXPECT_EQ(true, extra_node_2.GetIntListAttribute(
                      ax::mojom::IntListAttribute::kIndirectChildIds,
                      &indirect_child_ids));
  EXPECT_EQ(2U, indirect_child_ids.size());
  EXPECT_EQ(4, indirect_child_ids[0]);
  EXPECT_EQ(5, indirect_child_ids[1]);
}

}  // namespace ui
