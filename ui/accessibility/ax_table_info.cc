// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_table_info.h"

#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {

namespace {

void FindCells(AXNode* node, std::vector<AXNode*>* cells) {
  for (AXNode* child : node->children()) {
    if (child->data().HasState(ax::mojom::State::kIgnored) ||
        child->data().role == ax::mojom::Role::kGenericContainer)
      FindCells(child, cells);
    else if (IsCellOrTableHeader(child->data().role))
      cells->push_back(child);
  }
}

void FindRowsAndThenCells(AXNode* node, std::vector<AXNode*>* cells) {
  for (AXNode* child : node->children()) {
    if (child->data().HasState(ax::mojom::State::kIgnored) ||
        child->data().role == ax::mojom::Role::kGenericContainer)
      FindRowsAndThenCells(child, cells);
    else if (child->data().role == ax::mojom::Role::kRow)
      FindCells(child, cells);
  }
}

}  // namespace

// static
AXTableInfo* AXTableInfo::Create(AXTree* tree, AXNode* table_node) {
  DCHECK(tree);
  DCHECK(table_node);

#if DCHECK_IS_ON()
  // Sanity check, make sure the node is in the tree.
  AXNode* node = table_node;
  while (node && node != tree->root())
    node = node->parent();
  DCHECK(node == tree->root());
#endif

  if (!IsTableLike(table_node->data().role))
    return nullptr;

  AXTableInfo* info = new AXTableInfo(tree, table_node);
  bool success = info->Update();
  DCHECK(success);

  return info;
}

bool AXTableInfo::Update() {
  if (!IsTableLike(table_node_->data().role))
    return false;

  col_headers.clear();
  row_headers.clear();
  all_headers.clear();
  cell_ids.clear();
  unique_cell_ids.clear();

  std::vector<AXNode*> cells;
  FindRowsAndThenCells(table_node_, &cells);

  // Compute the actual row and column count, and the set of all unique cell ids
  // in the table.
  row_count = table_node_->data().GetIntAttribute(
      ax::mojom::IntAttribute::kTableRowCount);
  col_count = table_node_->data().GetIntAttribute(
      ax::mojom::IntAttribute::kTableColumnCount);
  for (AXNode* cell : cells) {
    int row_index = cell->data().GetIntAttribute(
        ax::mojom::IntAttribute::kTableCellRowIndex);
    int row_span = std::max(1, cell->data().GetIntAttribute(
                                   ax::mojom::IntAttribute::kTableCellRowSpan));
    row_count = std::max(row_count, row_index + row_span);
    int col_index = cell->data().GetIntAttribute(
        ax::mojom::IntAttribute::kTableCellColumnIndex);
    int col_span =
        std::max(1, cell->data().GetIntAttribute(
                        ax::mojom::IntAttribute::kTableCellColumnSpan));
    col_count = std::max(col_count, col_index + col_span);
  }

  // Allocate space for the 2-D array of cell IDs and 1-D
  // arrays of row headers and column headers.
  row_headers.resize(row_count);
  col_headers.resize(col_count);
  cell_ids.resize(row_count);
  for (auto& row : cell_ids)
    row.resize(col_count);

  // Now iterate over the cells and fill in the cell IDs, row headers,
  // and column headers based on the index and span of each cell.
  int32_t cell_index = 0;
  for (AXNode* cell : cells) {
    unique_cell_ids.push_back(cell->id());
    cell_id_to_index[cell->id()] = cell_index++;
    int row_index = cell->data().GetIntAttribute(
        ax::mojom::IntAttribute::kTableCellRowIndex);
    int row_span = std::max(1, cell->data().GetIntAttribute(
                                   ax::mojom::IntAttribute::kTableCellRowSpan));
    int col_index = cell->data().GetIntAttribute(
        ax::mojom::IntAttribute::kTableCellColumnIndex);
    int col_span =
        std::max(1, cell->data().GetIntAttribute(
                        ax::mojom::IntAttribute::kTableCellColumnSpan));

    // Cells must contain a 0-based row index and col index.
    if (row_index < 0 || col_index < 0)
      continue;

    for (int r = row_index; r < row_index + row_span; r++) {
      DCHECK_LT(r, row_count);
      for (int c = col_index; c < col_index + col_span; c++) {
        DCHECK_LT(c, col_count);
        cell_ids[r][c] = cell->id();
        if (cell->data().role == ax::mojom::Role::kColumnHeader) {
          col_headers[c].push_back(cell->id());
          all_headers.push_back(cell->id());
        } else if (cell->data().role == ax::mojom::Role::kRowHeader) {
          row_headers[r].push_back(cell->id());
          all_headers.push_back(cell->id());
        }
      }
    }
  }

  if (tree_->enable_extra_mac_nodes())
    UpdateExtraMacNodes();

  valid_ = true;
  return true;
}

void AXTableInfo::Invalidate() {
  valid_ = false;
}

void AXTableInfo::UpdateExtraMacNodes() {
  // On macOS, maintain additional AXNodes: one column node for each
  // column of the table, and one table header container.
  //
  // The nodes all set the table as the parent node, that way the Mac-specific
  // platform code can treat these nodes as additional children of the table
  // node.
  //
  // The columns have id -1, -2, -3, ... - this won't conflict with ids from
  // Blink, which are all positive.
  //
  // Each column has the kColumnIndex attribute set, and then each of the cells
  // in that column gets added as an indirect ID. That exposes them as children
  // via Mac APIs but ensures we don't explore those nodes multiple times when
  // walking the tree. The column also has the ID of the first column header
  // set.
  //
  // The table header container is just a node with all of the headers in the
  // table as indirect children.

  // One node for each column, and one more for the table header container.
  size_t extra_node_count = static_cast<size_t>(col_count + 1);

  if (extra_mac_nodes.size() != extra_node_count) {
    // Delete old extra nodes.
    ClearExtraMacNodes();

    // Resize.
    extra_mac_nodes.resize(col_count + 1);

    // Create column nodes.
    for (int i = 0; i < col_count; i++)
      extra_mac_nodes[i] = CreateExtraMacColumnNode(i);

    // Create table header container node.
    extra_mac_nodes[col_count] = CreateExtraMacTableHeaderNode();
  }

  // Update the columns to reflect current state of the table.
  for (int i = 0; i < col_count; i++)
    UpdateExtraMacColumnNodeAttributes(i);

  // Update the table header container to contain all headers.
  ui::AXNodeData data = extra_mac_nodes[col_count]->data();
  data.intlist_attributes.clear();
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                           all_headers);
  extra_mac_nodes[col_count]->SetData(data);
}

AXNode* AXTableInfo::CreateExtraMacColumnNode(int col_index) {
  int32_t id = tree_->GetNextNegativeInternalNodeId();
  int32_t index_in_parent = col_index + table_node_->child_count();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kColumn;
  node->SetData(data);
  if (tree_->delegate())
    tree_->delegate()->OnNodeCreated(tree_, node);
  return node;
}

AXNode* AXTableInfo::CreateExtraMacTableHeaderNode() {
  int32_t id = tree_->GetNextNegativeInternalNodeId();
  int32_t index_in_parent = col_count + table_node_->child_count();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kTableHeaderContainer;
  node->SetData(data);
  if (tree_->delegate())
    tree_->delegate()->OnNodeCreated(tree_, node);

  return node;
}

void AXTableInfo::UpdateExtraMacColumnNodeAttributes(int col_index) {
  ui::AXNodeData data = extra_mac_nodes[col_index]->data();
  data.int_attributes.clear();

  // Update the column index.
  data.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnIndex, col_index);

  // Update the column header.
  if (!col_headers[col_index].empty()) {
    data.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnHeaderId,
                         col_headers[col_index][0]);
  }

  // Update the list of cells in the column.
  data.intlist_attributes.clear();
  std::vector<int32_t> col_nodes;
  int32_t last = 0;
  for (int row_index = 0; row_index < row_count; row_index++) {
    int32_t cell_id = cell_ids[row_index][col_index];
    if (cell_id != 0 && cell_id != last)
      col_nodes.push_back(cell_id);
    last = cell_id;
  }
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                           col_nodes);
  extra_mac_nodes[col_index]->SetData(data);
}

void AXTableInfo::ClearExtraMacNodes() {
  for (size_t i = 0; i < extra_mac_nodes.size(); i++) {
    if (tree_->delegate())
      tree_->delegate()->OnNodeWillBeDeleted(tree_, extra_mac_nodes[i]);
    delete extra_mac_nodes[i];
  }
}

AXTableInfo::AXTableInfo(AXTree* tree, AXNode* table_node)
    : tree_(tree), table_node_(table_node) {}

AXTableInfo::~AXTableInfo() {
  ClearExtraMacNodes();
}

}  // namespace ui
