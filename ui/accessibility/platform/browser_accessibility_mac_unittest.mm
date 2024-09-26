// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/accessibility/platform/browser_accessibility_cocoa.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#import "ui/base/test/cocoa_helper.h"

namespace ui {

namespace {

enum class TableHeaderOption {
  NoHeaders,
  RowHeaders,
  TwoRowHeaders,
  ColumnHeaders
};

void MakeRow(AXNodeData* row, int row_id) {
  row->id = row_id;
  row->role = ax::mojom::Role::kRow;
}

void MakeCell(AXNodeData* cell,
              int cell_id,
              int row_index,
              int column_index,
              int row_span = 1,
              int column_span = 1) {
  cell->id = cell_id;
  cell->role = ax::mojom::Role::kCell;
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, row_index);
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                        column_index);
  if (row_span > 1) {
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, row_span);
  }
  if (column_span > 1) {
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                          column_span);
  }
}

void MakeRowHeader(AXNodeData* cell,
                   int cell_id,
                   int row_index,
                   int column_index,
                   int row_span = 1,
                   int column_span = 1) {
  MakeCell(cell, cell_id, row_index, column_index, row_span, column_span);
  cell->role = ax::mojom::Role::kRowHeader;
}

void MakeColumnHeader(AXNodeData* cell,
                      int cell_id,
                      int row_index,
                      int column_index,
                      int row_span = 1,
                      int column_span = 1) {
  MakeCell(cell, cell_id, row_index, column_index, row_span, column_span);
  cell->role = ax::mojom::Role::kColumnHeader;
}

void MakeTable(AXTreeUpdate* initial_state,
               int row_count,
               int column_count,
               TableHeaderOption header_option = TableHeaderOption::NoHeaders) {
  int next_id = 1;
  initial_state->root_id = next_id++;

  // Node count is the table, plus each row, plus all cells.
  initial_state->nodes.resize(1 + row_count + row_count * column_count);
  int next_node_index = 0;

  AXNodeData* table = &initial_state->nodes[next_node_index++];
  table->id = initial_state->root_id;
  table->role = ax::mojom::Role::kTable;
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, row_count);
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                         column_count);

  for (int row = 0; row < row_count; row++) {
    AXNodeData* row_node = &initial_state->nodes[next_node_index++];
    MakeRow(row_node, next_id++);
    table->child_ids.push_back(row_node->id);

    for (int column = 0; column < column_count; column++) {
      AXNodeData* cell_node = &initial_state->nodes[next_node_index++];
      if (header_option == TableHeaderOption::RowHeaders && column == 0) {
        MakeRowHeader(cell_node, next_id++, row, column);
      } else if (header_option == TableHeaderOption::TwoRowHeaders &&
                 column < 2) {
        MakeRowHeader(cell_node, next_id++, row, column);
      } else if (header_option == TableHeaderOption::ColumnHeaders &&
                 row == 0) {
        MakeColumnHeader(cell_node, next_id++, row, column);
      } else {
        MakeCell(cell_node, next_id++, row, column);
      }
      row_node->child_ids.push_back(cell_node->id);
    }
  }
}

}  // namespace

class BrowserAccessibilityMacTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    RebuildAccessibilityTree();
  }

 protected:
  void RebuildAccessibilityTree() {
    // Clean out the existing root data in case this method is called multiple
    // times in a test.
    root_ = AXNodeData();
    root_.id = 1000;
    root_.relative_bounds.bounds.set_width(500);
    root_.relative_bounds.bounds.set_height(100);
    root_.role = ax::mojom::Role::kRootWebArea;
    root_.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                             "HelpText");
    root_.child_ids.push_back(1001);
    root_.child_ids.push_back(1002);

    AXNodeData child1;
    child1.id = 1001;
    child1.role = ax::mojom::Role::kButton;
    child1.SetName("Child1");
    child1.relative_bounds.bounds.set_width(250);
    child1.relative_bounds.bounds.set_height(100);

    AXNodeData child2;
    child2.id = 1002;
    child2.relative_bounds.bounds.set_x(250);
    child2.relative_bounds.bounds.set_width(250);
    child2.relative_bounds.bounds.set_height(100);
    child2.role = ax::mojom::Role::kHeading;

    manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
        MakeAXTreeUpdateForTesting(root_, child1, child2), node_id_delegate_,
        nullptr);
    accessibility_ =
        manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  }

  void SetRootValue(std::string value) {
    if (!manager_)
      return;
    root_.SetValue(value);
    AXUpdatesAndEvents event_bundle;
    event_bundle.updates.resize(1);
    event_bundle.updates[0].nodes.push_back(root_);
    ASSERT_TRUE(manager_->OnAccessibilityEvents(event_bundle));
  }

  AXNodeData root_;
  BrowserAccessibilityCocoa* __strong accessibility_;
  TestAXNodeIdDelegate node_id_delegate_;
  std::unique_ptr<BrowserAccessibilityManager> manager_;

  const base::test::SingleThreadTaskEnvironment task_environment_;
};

// Standard hit test.
TEST_F(BrowserAccessibilityMacTest, HitTestTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1", firstChild.accessibilityLabel);
}

// Test doing a hit test on the edge of a child.
TEST_F(BrowserAccessibilityMacTest, EdgeHitTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSZeroPoint];
  EXPECT_NSEQ(@"Child1", firstChild.accessibilityLabel);
}

// This will test a hit test with invalid coordinates.  It is assumed that
// the hit test has been narrowed down to this object or one of its children
// so it should return itself since it has no better hit result.
TEST_F(BrowserAccessibilityMacTest, InvalidHitTestCoordsTest) {
  BrowserAccessibilityCocoa* hitTestResult =
      [accessibility_ accessibilityHitTest:NSMakePoint(-50, 50)];
  EXPECT_NSEQ(accessibility_, hitTestResult);
}

// Test to ensure querying standard attributes works.
TEST_F(BrowserAccessibilityMacTest, BasicAttributeTest) {
  EXPECT_NSEQ(@"HelpText", [accessibility_ accessibilityHelp]);
}

TEST_F(BrowserAccessibilityMacTest, RetainedDetachedObjectsReturnNil) {
  // Get the first child. Hold it in a precise lifetime variable. This simulates
  // what the system might do with an accessibility object.
  NS_VALID_UNTIL_END_OF_SCOPE BrowserAccessibilityCocoa* retainedFirstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1", retainedFirstChild.accessibilityLabel);

  // Rebuild the accessibility tree, which should detach |retainedFirstChild|.
  RebuildAccessibilityTree();

  // Now any attributes we query should return nil.
  EXPECT_NSEQ(nil, retainedFirstChild.accessibilityLabel);
}

TEST_F(BrowserAccessibilityMacTest, TestComputeTextEdit) {
  root_ = AXNodeData();
  root_.id = 1;
  root_.role = ax::mojom::Role::kTextField;
  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      MakeAXTreeUpdateForTesting(root_), node_id_delegate_, nullptr);
  accessibility_ =
      manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();

  // Insertion but no deletion.

  SetRootValue("text");
  AXTextEdit text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"text", text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("new text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"new ", text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u" hello", text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("newer text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"er", text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  // Deletion but no insertion.

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"er", text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("new text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u" hello", text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"new ", text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"text", text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  // Both insertion and deletion.

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  SetRootValue("new word hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"text", text_edit.deleted_text);
  EXPECT_EQ(u"word", text_edit.inserted_text);

  SetRootValue("new word there");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"hello", text_edit.deleted_text);
  EXPECT_EQ(u"there", text_edit.inserted_text);

  SetRootValue("old word there");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(u"new", text_edit.deleted_text);
  EXPECT_EQ(u"old", text_edit.inserted_text);
}

// Test Mac-specific table APIs.
TEST_F(BrowserAccessibilityMacTest, TableAPIs) {
  AXTreeUpdate initial_state;
  const int kNumberOfRows = 2;
  const int kNumberOfColumns = 2;
  MakeTable(&initial_state, kNumberOfRows, kNumberOfColumns,
            TableHeaderOption::ColumnHeaders);

  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      initial_state, node_id_delegate_, nullptr);
  BrowserAccessibilityCocoa* ax_table =
      manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  NSArray* children = ax_table.children;
  EXPECT_EQ(5U, children.count);

  EXPECT_NSEQ(@"AXRow", [children[0] role]);
  EXPECT_EQ(2U, [[children[0] children] count]);

  EXPECT_NSEQ(@"AXRow", [children[1] role]);
  EXPECT_EQ(2U, [[children[1] children] count]);

  EXPECT_NSEQ(@"AXColumn", [children[2] role]);
  EXPECT_EQ(2U, [[children[2] children] count]);
  id col_children = [children[2] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);

  EXPECT_NSEQ(@"AXColumn", [children[3] role]);
  EXPECT_EQ(2U, [[children[3] children] count]);
  col_children = [children[3] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);

  EXPECT_NSEQ(@"AXGroup", [children[4] role]);
  EXPECT_EQ(2U, [[children[4] children] count]);
  col_children = [children[4] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);
}

// Test table row header support.
TEST_F(BrowserAccessibilityMacTest, TableWithRowHeaders) {
  // A non-table object should return nil for rowHeaders.
  root_ = AXNodeData();
  root_.id = 1;
  root_.role = ax::mojom::Role::kTextField;
  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      MakeAXTreeUpdateForTesting(root_), node_id_delegate_, nullptr);
  BrowserAccessibilityCocoa* ax_textfield =
      manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  NSArray* row_headers = [ax_textfield rowHeaders];
  EXPECT_EQ(nil, row_headers);

  // A table with no row headers should return nil for rowHeaders.
  AXTreeUpdate headerless_table_state;
  const int kNumberOfRows = 2;
  const int kNumberOfColumns = 2;
  MakeTable(&headerless_table_state, kNumberOfRows, kNumberOfColumns);
  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      headerless_table_state, node_id_delegate_, nullptr);
  BrowserAccessibilityCocoa* ax_table =
      manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  ax_table = manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();
  row_headers = [ax_table rowHeaders];
  EXPECT_EQ(nil, row_headers);

  // Create a table with row headers.
  AXTreeUpdate table_state;
  MakeTable(&table_state, kNumberOfRows, kNumberOfColumns,
            TableHeaderOption::RowHeaders);
  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      table_state, node_id_delegate_, nullptr);
  ax_table = manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();

  // Confirm the AX structure is as expected.
  NSArray* ax_table_children = ax_table.children;
  EXPECT_EQ(5U, ax_table_children.count);

  id first_row = ax_table_children[0];
  EXPECT_NSEQ(@"AXRow", [first_row role]);
  id first_row_children = [first_row children];
  EXPECT_EQ(2U, [first_row_children count]);
  EXPECT_NSEQ(@"AXCell", [first_row_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [first_row_children[1] role]);

  id second_row = ax_table_children[1];
  EXPECT_NSEQ(@"AXRow", [second_row role]);
  id second_row_children = [second_row children];
  EXPECT_EQ(2U, [second_row_children count]);
  EXPECT_NSEQ(@"AXCell", [second_row_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [second_row_children[1] role]);

  id first_column = ax_table_children[2];
  EXPECT_NSEQ(@"AXColumn", [first_column role]);
  id first_column_children = [first_column children];
  EXPECT_EQ(2U, [first_column_children count]);
  EXPECT_NSEQ(@"AXCell", [first_column_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [first_column_children[1] role]);

  id second_column = ax_table_children[3];
  EXPECT_NSEQ(@"AXColumn", [second_column role]);
  id second_column_children = [second_column children];
  EXPECT_EQ(2U, [second_column_children count]);
  EXPECT_NSEQ(@"AXCell", [second_column_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [second_column_children[1] role]);

  EXPECT_EQ(first_row_children[0], first_column_children[0]);
  EXPECT_EQ(first_row_children[1], second_column_children[0]);
  EXPECT_EQ(second_row_children[0], first_column_children[1]);
  EXPECT_EQ(second_row_children[1], second_column_children[1]);

  id table_group = ax_table_children[4];
  EXPECT_NSEQ(@"AXGroup", [table_group role]);
  EXPECT_EQ(0U, [table_group children].count);

  // Confirm the table has row headers, and that they match the expected cells
  // in the table.
  row_headers = [ax_table rowHeaders];
  EXPECT_EQ(2U, [row_headers count]);
  id first_row_header_cell = row_headers[0];
  EXPECT_EQ(first_row_header_cell, first_row_children[0]);
  id second_row_header_cell = row_headers[1];
  EXPECT_EQ(second_row_header_cell, second_row_children[0]);

  // If we ask a row header cell for its rowHeaders, we should get that
  // cell back.
  row_headers = [first_row_header_cell rowHeaders];
  EXPECT_EQ(1U, [row_headers count]);
  EXPECT_EQ(first_row_header_cell, row_headers[0]);

  // A non-row-header cell should return the header for its row.
  id last_cell_second_row = second_row_children[1];
  row_headers = [last_cell_second_row rowHeaders];
  EXPECT_EQ(1U, [row_headers count]);
  EXPECT_NSEQ(second_row_header_cell, row_headers[0]);
}

// Test table with more than one row header.
TEST_F(BrowserAccessibilityMacTest, TableWithTwoRowHeaders) {
  // Create a table with two row headers per row.
  const int kNumberOfRows = 2;
  const int kNumberOfColumns = 3;
  AXTreeUpdate table_state;
  MakeTable(&table_state, kNumberOfRows, kNumberOfColumns,
            TableHeaderOption::TwoRowHeaders);
  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      table_state, node_id_delegate_, nullptr);
  BrowserAccessibilityCocoa* ax_table =
      manager_->GetBrowserAccessibilityRoot()->GetNativeViewAccessible();

  // Confirm the AX structure is as expected.
  NSArray* ax_table_children = ax_table.children;
  EXPECT_EQ(6U, ax_table_children.count);

  id first_row = ax_table_children[0];
  EXPECT_NSEQ(@"AXRow", [first_row role]);
  id first_row_children = [first_row children];
  EXPECT_EQ(3U, [first_row_children count]);
  EXPECT_NSEQ(@"AXCell", [first_row_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [first_row_children[1] role]);
  EXPECT_NSEQ(@"AXCell", [first_row_children[2] role]);

  id second_row = ax_table_children[1];
  EXPECT_NSEQ(@"AXRow", [second_row role]);
  id second_row_children = [second_row children];
  EXPECT_EQ(3U, [second_row_children count]);
  EXPECT_NSEQ(@"AXCell", [second_row_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [second_row_children[1] role]);
  EXPECT_NSEQ(@"AXCell", [second_row_children[2] role]);

  id first_column = ax_table_children[2];
  EXPECT_NSEQ(@"AXColumn", [first_column role]);
  id first_column_children = [first_column children];
  EXPECT_EQ(2U, [first_column_children count]);
  EXPECT_NSEQ(@"AXCell", [first_column_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [first_column_children[1] role]);

  id second_column = ax_table_children[3];
  EXPECT_NSEQ(@"AXColumn", [second_column role]);
  id second_column_children = [second_column children];
  EXPECT_EQ(2U, [second_column_children count]);
  EXPECT_NSEQ(@"AXCell", [second_column_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [second_column_children[1] role]);

  id third_column = ax_table_children[4];
  EXPECT_NSEQ(@"AXColumn", [third_column role]);
  id third_column_children = [third_column children];
  EXPECT_EQ(2U, [third_column_children count]);
  EXPECT_NSEQ(@"AXCell", [third_column_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [third_column_children[1] role]);

  EXPECT_EQ(first_row_children[0], first_column_children[0]);
  EXPECT_EQ(first_row_children[1], second_column_children[0]);
  EXPECT_EQ(first_row_children[2], third_column_children[0]);
  EXPECT_EQ(second_row_children[0], first_column_children[1]);
  EXPECT_EQ(second_row_children[1], second_column_children[1]);
  EXPECT_EQ(second_row_children[2], third_column_children[1]);

  id table_group = ax_table_children[5];
  EXPECT_NSEQ(@"AXGroup", [table_group role]);
  EXPECT_EQ(0U, [table_group children].count);

  // Confirm the table has two row headers per row, and that they match
  // the expected cells in the table.
  NSArray* row_headers = [ax_table rowHeaders];
  EXPECT_EQ(4U, [row_headers count]);
  id first_row_header_cell = row_headers[0];
  EXPECT_EQ(first_row_header_cell, first_row_children[0]);
  id second_row_header_cell = row_headers[1];
  EXPECT_EQ(second_row_header_cell, first_row_children[1]);
  id third_row_header_cell = row_headers[2];
  EXPECT_EQ(third_row_header_cell, second_row_children[0]);
  id fourth_row_header_cell = row_headers[3];
  EXPECT_EQ(fourth_row_header_cell, second_row_children[1]);

  // A non-row-header cell should return the headers for its row.
  id last_cell_second_row = second_row_children[2];
  row_headers = [last_cell_second_row rowHeaders];
  EXPECT_EQ(2U, [row_headers count]);
  EXPECT_NSEQ(third_row_header_cell, row_headers[0]);
  EXPECT_NSEQ(fourth_row_header_cell, row_headers[1]);
}

// Test Mac indirect columns and descendants.
TEST_F(BrowserAccessibilityMacTest, TableColumnsAndDescendants) {
  AXTreeUpdate initial_state;
  const int kNumberOfRows = 2;
  const int kNumberOfColumns = 2;
  MakeTable(&initial_state, kNumberOfRows, kNumberOfColumns,
            TableHeaderOption::ColumnHeaders);

  // This relation is the key to force
  // AXEventGenerator::FireRelationSourceEvents to trigger addition of an event
  // which had caused a crash below.
  initial_state.nodes[6].AddIntListAttribute(
      ax::mojom::IntListAttribute::kFlowtoIds, {1});

  manager_ = std::make_unique<BrowserAccessibilityManagerMac>(
      initial_state, node_id_delegate_, nullptr);

  BrowserAccessibilityMac* root = static_cast<BrowserAccessibilityMac*>(
      manager_->GetBrowserAccessibilityRoot());

  // This triggers computation of the extra Mac table cells. 2 rows, 2 extra
  // columns, and 1 extra column header. This used to crash.
  ASSERT_EQ(root->PlatformChildCount(), 5U);
}

}  // namespace ui
