// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_table_info.h"

#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/gfx/geometry/rect_f.h"

using ax::mojom::IntAttribute;

namespace ui {

namespace {

// Given a node representing a table row, search its children
// recursively to find any cells or table headers, and append
// them to |cells|.
//
// We recursively check generic containers like <div> and any
// nodes that are ignored, but we don't search any other roles
// in-between a table row and its cells.
void FindCellsInRow(AXNode* node, std::vector<AXNode*>* cell_nodes) {
  for (AXNode* child : node->children()) {
    if (child->IsIgnored() ||
        child->data().role == ax::mojom::Role::kGenericContainer)
      FindCellsInRow(child, cell_nodes);
    else if (IsCellOrTableHeader(child->data().role))
      cell_nodes->push_back(child);
  }
}

// Given a node representing a table/grid, search its children
// recursively to find any rows and append them to |row_nodes|, then
// for each row find its cells and add them to |cell_nodes_per_row| as a
// 2-dimensional array.
//
// We only recursively check for the following roles in between a table and
// its rows: generic containers like <div>, any nodes that are ignored, and
// table sections (which have Role::kGroup).
void FindRowsAndThenCells(AXNode* node,
                          std::vector<AXNode*>* row_nodes,
                          std::vector<std::vector<AXNode*>>* cell_nodes_per_row,
                          int32_t& caption_node_id) {
  for (AXNode* child : node->children()) {
    if (child->IsIgnored() ||
        child->data().role == ax::mojom::Role::kGenericContainer ||
        child->data().role == ax::mojom::Role::kGroup) {
      FindRowsAndThenCells(child, row_nodes, cell_nodes_per_row,
                           caption_node_id);
    } else if (IsTableRow(child->data().role)) {
      row_nodes->push_back(child);
      cell_nodes_per_row->push_back(std::vector<AXNode*>());
      FindCellsInRow(child, &cell_nodes_per_row->back());
    } else if (child->data().role == ax::mojom::Role::kCaption)
      caption_node_id = child->id();
  }
}

size_t GetSizeTAttribute(const AXNode& node, IntAttribute attribute) {
  return base::saturated_cast<size_t>(node.GetIntAttribute(attribute));
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
  if (!table_node_->IsTable())
    return false;

  ClearVectors();

  std::vector<AXNode*> row_nodes;
  std::vector<std::vector<AXNode*>> cell_nodes_per_row;
  caption_id = 0;
  FindRowsAndThenCells(table_node_, &row_nodes, &cell_nodes_per_row,
                       caption_id);
  DCHECK_EQ(cell_nodes_per_row.size(), row_nodes.size());

  // Get the optional row and column count from the table. If we encounter
  // a cell with an index or span larger than this, we'll update the
  // table row and column count to be large enough to fit all cells.
  row_count = GetSizeTAttribute(*table_node_, IntAttribute::kTableRowCount);
  col_count = GetSizeTAttribute(*table_node_, IntAttribute::kTableColumnCount);

  int32_t aria_rows = table_node_->GetIntAttribute(IntAttribute::kAriaRowCount);
  aria_row_count = (aria_rows != ax::mojom::kUnknownAriaColumnOrRowCount)
                       ? base::make_optional(int{aria_rows})
                       : base::nullopt;

  int32_t aria_cols =
      table_node_->GetIntAttribute(IntAttribute::kAriaColumnCount);
  aria_col_count = (aria_cols != ax::mojom::kUnknownAriaColumnOrRowCount)
                       ? base::make_optional(int{aria_cols})
                       : base::nullopt;

  // Iterate over the cells and build up an array of CellData
  // entries, one for each cell. Compute the actual row and column
  BuildCellDataVectorFromRowAndCellNodes(row_nodes, cell_nodes_per_row);

  // At this point we have computed valid row and column indices for
  // every cell in the table, and an accurate row and column count for the
  // whole table that fits every cell and its spans. The final step is to
  // fill in a 2-dimensional array that lets us look up an individual cell
  // by its (row, column) coordinates, plus arrays to hold row and column
  // headers.
  BuildCellAndHeaderVectorsFromCellData();

  // On Mac, we add a few extra nodes to the table - see comment
  // at the top of UpdateExtraMacNodes for details.
  if (tree_->enable_extra_mac_nodes())
    UpdateExtraMacNodes();

  // The table metadata is now valid, any table queries will now be
  // fast. Any time a node in the table is updated, we'll have to
  // recompute all of this.
  valid_ = true;
  return true;
}

void AXTableInfo::Invalidate() {
  valid_ = false;
}

void AXTableInfo::ClearVectors() {
  col_headers.clear();
  row_headers.clear();
  all_headers.clear();
  cell_ids.clear();
  unique_cell_ids.clear();
  cell_data_vector.clear();
}

void AXTableInfo::BuildCellDataVectorFromRowAndCellNodes(
    const std::vector<AXNode*>& row_nodes,
    const std::vector<std::vector<AXNode*>>& cell_nodes_per_row) {
  // Iterate over the cells and build up an array of CellData
  // entries, one for each cell. Compute the actual row and column
  // indices for each cell by taking the specified row and column
  // index in the accessibility tree if legal, but replacing it with
  // valid table coordinates otherwise.
  size_t cell_index = 0;
  size_t current_aria_row_index = 1;
  size_t next_row_index = 0;
  for (size_t i = 0; i < cell_nodes_per_row.size(); i++) {
    auto& cell_nodes_in_this_row = cell_nodes_per_row[i];
    AXNode* row_node = row_nodes[i];
    bool is_first_cell_in_row = true;
    size_t current_col_index = 0;
    size_t current_aria_col_index = 1;

    // Make sure the row index is always at least as high as the one reported by
    // Blink.
    row_id_to_index[row_node->id()] =
        std::max(next_row_index,
                 GetSizeTAttribute(*row_node, IntAttribute::kTableRowIndex));
    size_t* current_row_index = &row_id_to_index[row_node->id()];

    for (AXNode* cell : cell_nodes_in_this_row) {
      // Fill in basic info in CellData.
      CellData cell_data;
      unique_cell_ids.push_back(cell->id());
      cell_id_to_index[cell->id()] = cell_index++;
      cell_data.cell = cell;

      // Get table cell accessibility attributes - note that these may
      // be missing or invalid, we'll correct them next.
      cell_data.row_index =
          GetSizeTAttribute(*cell, IntAttribute::kTableCellRowIndex);
      cell_data.row_span =
          GetSizeTAttribute(*cell, IntAttribute::kTableCellRowSpan);
      cell_data.aria_row_index =
          GetSizeTAttribute(*cell, IntAttribute::kAriaCellRowIndex);
      cell_data.col_index =
          GetSizeTAttribute(*cell, IntAttribute::kTableCellColumnIndex);
      cell_data.aria_col_index =
          GetSizeTAttribute(*cell, IntAttribute::kAriaCellColumnIndex);
      cell_data.col_span =
          GetSizeTAttribute(*cell, IntAttribute::kTableCellColumnSpan);

      // The col span and row span must be at least 1.
      cell_data.row_span = std::max(size_t{1}, cell_data.row_span);
      cell_data.col_span = std::max(size_t{1}, cell_data.col_span);

      // Ensure the column index must always be incrementing.
      cell_data.col_index = std::max(cell_data.col_index, current_col_index);

      if (is_first_cell_in_row) {
        is_first_cell_in_row = false;

        // If it's the first cell in the row, ensure the row index is
        // incrementing. The rest of the cells in this row are forced to have
        // the same row index.
        if (cell_data.row_index > *current_row_index) {
          *current_row_index = cell_data.row_index;
        } else {
          cell_data.row_index = *current_row_index;
        }

        // The starting ARIA row and column index might be specified in
        // the row node, we should check there.
        if (!cell_data.aria_row_index) {
          cell_data.aria_row_index =
              GetSizeTAttribute(*row_node, IntAttribute::kAriaCellRowIndex);
        }
        if (!cell_data.aria_col_index) {
          cell_data.aria_col_index =
              GetSizeTAttribute(*row_node, IntAttribute::kAriaCellColumnIndex);
        }
        cell_data.aria_row_index =
            std::max(cell_data.aria_row_index, current_aria_row_index);
        current_aria_row_index = cell_data.aria_row_index;
      } else {
        // Don't allow the row index to change after the beginning
        // of a row.
        cell_data.row_index = *current_row_index;
        cell_data.aria_row_index = current_aria_row_index;
      }

      // Ensure the ARIA col index is incrementing.
      cell_data.aria_col_index =
          std::max(cell_data.aria_col_index, current_aria_col_index);
      current_aria_col_index = cell_data.aria_col_index;

      // Update the row count and col count for the whole table to make
      // sure they're large enough to fit this cell, including its spans.
      // The -1 in the ARIA calculations is because ARIA indices are 1-based,
      // whereas all other indices are zero-based.
      row_count = std::max(row_count, cell_data.row_index + cell_data.row_span);
      col_count = std::max(col_count, cell_data.col_index + cell_data.col_span);
      if (aria_row_count) {
        aria_row_count =
            std::max((*aria_row_count),
                     int{current_aria_row_index + cell_data.row_span - 1});
      }
      if (aria_col_count) {
        aria_col_count =
            std::max((*aria_col_count),
                     int{current_aria_col_index + cell_data.col_span - 1});
      }
      // Update |current_col_index| to reflect the next available index after
      // this cell including its colspan. The next column index in this row
      // must be at least this large. Same for the current ARIA col index.
      current_col_index = cell_data.col_index + cell_data.col_span;
      current_aria_col_index = cell_data.aria_col_index + cell_data.col_span;

      // Add this cell to our vector.
      cell_data_vector.push_back(cell_data);
    }

    // At the end of each row, increment |current_aria_row_index| to reflect the
    // next available index after this row. The next row index must be at least
    // this large. Also update |next_row_index|.
    current_aria_row_index++;
    next_row_index = *current_row_index + 1;
  }
}

void AXTableInfo::BuildCellAndHeaderVectorsFromCellData() {
  // Allocate space for the 2-D array of cell IDs and 1-D
  // arrays of row headers and column headers.
  row_headers.resize(row_count);
  col_headers.resize(col_count);
  cell_ids.resize(row_count);
  for (auto& row : cell_ids)
    row.resize(col_count);

  // Fill in the arrays.
  //
  // At this point we have computed valid row and column indices for
  // every cell in the table, and an accurate row and column count for the
  // whole table that fits every cell and its spans. The final step is to
  // fill in a 2-dimensional array that lets us look up an individual cell
  // by its (row, column) coordinates, plus arrays to hold row and column
  // headers.
  for (auto& cell_data : cell_data_vector) {
    for (size_t r = cell_data.row_index;
         r < cell_data.row_index + cell_data.row_span; r++) {
      DCHECK_LT(r, row_count);
      for (size_t c = cell_data.col_index;
           c < cell_data.col_index + cell_data.col_span; c++) {
        DCHECK_LT(c, col_count);
        AXNode* cell = cell_data.cell;
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

  if (!extra_mac_nodes.empty()) {
    // Delete old extra nodes.
    ClearExtraMacNodes();
  }

  // One node for each column, and one more for the table header container.
  size_t extra_node_count = col_count + 1;
  // Resize.
  extra_mac_nodes.resize(extra_node_count);

  // Create column nodes.
  for (size_t i = 0; i < col_count; i++)
    extra_mac_nodes[i] = CreateExtraMacColumnNode(i);

  // Create table header container node.
  extra_mac_nodes[col_count] = CreateExtraMacTableHeaderNode();

  // Update the columns to reflect current state of the table.
  for (size_t i = 0; i < col_count; i++)
    UpdateExtraMacColumnNodeAttributes(i);

  // Update the table header container to contain all headers.
  ui::AXNodeData data = extra_mac_nodes[col_count]->data();
  data.intlist_attributes.clear();
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                           all_headers);
  extra_mac_nodes[col_count]->SetData(data);
}

AXNode* AXTableInfo::CreateExtraMacColumnNode(size_t col_index) {
  int32_t id = tree_->GetNextNegativeInternalNodeId();
  size_t index_in_parent = col_index + table_node_->children().size();
  int32_t unignored_index_in_parent =
      col_index + table_node_->GetUnignoredChildCount();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent,
                            unignored_index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kColumn;
  node->SetData(data);
  for (AXTreeObserver& observer : tree_->observers()) {
    observer.OnNodeCreated(tree_, node);
    observer.OnAtomicUpdateFinished(
        tree_, false,
        {AXTreeObserver::Change(node,
                                AXTreeObserver::ChangeType::NODE_CREATED)});
  }
  return node;
}

AXNode* AXTableInfo::CreateExtraMacTableHeaderNode() {
  int32_t id = tree_->GetNextNegativeInternalNodeId();
  size_t index_in_parent = col_count + table_node_->children().size();
  int32_t unignored_index_in_parent =
      col_count + table_node_->GetUnignoredChildCount();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent,
                            unignored_index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kTableHeaderContainer;
  node->SetData(data);

  for (AXTreeObserver& observer : tree_->observers()) {
    observer.OnNodeCreated(tree_, node);
    observer.OnAtomicUpdateFinished(
        tree_, false,
        {AXTreeObserver::Change(node,
                                AXTreeObserver::ChangeType::NODE_CREATED)});
  }

  return node;
}

void AXTableInfo::UpdateExtraMacColumnNodeAttributes(size_t col_index) {
  ui::AXNodeData data = extra_mac_nodes[col_index]->data();
  data.int_attributes.clear();

  // Update the column index.
  data.AddIntAttribute(IntAttribute::kTableColumnIndex, int32_t{col_index});

  // Update the column header.
  if (!col_headers[col_index].empty()) {
    data.AddIntAttribute(IntAttribute::kTableColumnHeaderId,
                         col_headers[col_index][0]);
  }

  // Update the list of cells in the column.
  data.intlist_attributes.clear();
  std::vector<int32_t> col_nodes;
  int32_t last = 0;
  for (size_t row_index = 0; row_index < row_count; row_index++) {
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
    for (AXTreeObserver& observer : tree_->observers())
      observer.OnNodeWillBeDeleted(tree_, extra_mac_nodes[i]);
    delete extra_mac_nodes[i];
  }
}

AXTableInfo::AXTableInfo(AXTree* tree, AXNode* table_node)
    : tree_(tree), table_node_(table_node) {}

AXTableInfo::~AXTableInfo() {
  ClearExtraMacNodes();
}

}  // namespace ui
