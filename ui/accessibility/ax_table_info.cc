// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_table_info.h"

#include <iostream>
#include <string>
#include <unordered_set>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
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
// We recursively skip generic containers like <div> and any
// nodes that are ignored, but we don't skp any other roles
// in-between a table row and its cells.
void FindCellsInRow(AXNode* node, std::vector<AXNode*>* cell_nodes) {
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AXNode* child = iter.get();
    if (child->GetRole() == ax::mojom::Role::kGenericContainer) {
      FindCellsInRow(child, cell_nodes);
    } else if (IsCellOrTableHeader(child->GetRole())) {
      cell_nodes->push_back(child);
    }
  }
}

bool isRowlessTable(AXNode* node) {
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AXNode* child = iter.get();
    if (child->GetRole() == ax::mojom::Role::kGenericContainer ||
        child->GetRole() == ax::mojom::Role::kGroup ||
        child->GetRole() == ax::mojom::Role::kRowGroup) {
      return isRowlessTable(child);
    } else if (IsTableRow(child->GetRole())) {
      return false;
    } else if (child->GetRole() == ax::mojom::Role::kCell ||
               child->GetRole() == ax::mojom::Role::kGridCell) {
      // A row will always be reached before a cell if the table is not rowless.
      return true;
    }
  }
  // If the table is empty we can use the default code path for a rowed table.
  return false;
}

// Given a node representing a table/grid, search its children
// to find any rows and append them to |row_node_list|.
//
// We only check for the following roles in between a table and
// its rows: generic containers like <div>, any nodes that are ignored, and
// table sections (which have Role::kRowGroup).
// Returns false if cells were found. We should not reach cells if rows are
// present.
void FindRows(AXNode* node,
              std::vector<raw_ptr<AXNode, VectorExperimental>>* row_node_list,
              AXNodeID& caption_node_id) {
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AXNode* child = iter.get();
    if (child->GetRole() == ax::mojom::Role::kGenericContainer ||
        child->GetRole() == ax::mojom::Role::kGroup ||
        child->GetRole() == ax::mojom::Role::kRowGroup) {
      FindRows(child, row_node_list, caption_node_id);
    } else if (IsTableRow(child->GetRole())) {
      row_node_list->push_back(child);
    } else if (child->GetRole() == ax::mojom::Role::kCaption) {
      caption_node_id = child->id();
    }
  }
}

// For each row find its cells and add them to |cell_nodes_per_row| as a
// 2-dimensional array.
void FindCells(std::vector<raw_ptr<AXNode, VectorExperimental>>* row_node_list,
               std::vector<std::vector<AXNode*>>* cell_nodes_per_row) {
  for (AXNode* row : *row_node_list) {
    cell_nodes_per_row->emplace_back();
    FindCellsInRow(row, &cell_nodes_per_row->back());
  }
}

// Find all the cells in a container that does not contain rows as part of the
// encoding.
//
// Example:
//
// <Grid>
//    <Cell row=0,col=0>
//    <Cell row=0,col=1>
//    <Cell row=1,col=0>
//    <Cell row=1,col=1>
// </Grid>
//
// Would be equivalent to
//
// <Grid>
//    <Row>
//      <Cell col=0>
//      <Cell col=1>
//    <Row>
//    <Row>
//      <Cell col=0>
//      <Cell col=1>
//    <Row>
// </Grid>
void FindCellsForRowlessTable(
    AXNode* grid_node,
    std::vector<std::vector<AXNode*>>* cell_nodes_per_row) {
  int current_row = -1;
  int current_index = -1;
  base::queue<AXNode*> child_queue;
  for (auto iter = grid_node->UnignoredChildrenBegin();
       iter != grid_node->UnignoredChildrenEnd(); ++iter) {
    child_queue.push(iter.get());
    while (!child_queue.empty()) {
      auto* child = child_queue.front();
      child_queue.pop();
      if (child->GetRole() == ax::mojom::Role::kGenericContainer ||
          child->GetRole() == ax::mojom::Role::kGroup) {
        // Add children of the container to the queue
        for (auto container_itr = child->UnignoredChildrenBegin();
             container_itr != child->UnignoredChildrenEnd(); ++container_itr) {
          child_queue.push(container_itr.get());
        }
        continue;
      } else if (IsCellOrTableHeader(child->GetRole())) {
        const int rowIndex =
            child->GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
        CHECK_GE(rowIndex,0);
        if (current_row < rowIndex) {
          cell_nodes_per_row->emplace_back();
          current_row = rowIndex;
          current_index++;
        }
        CHECK_GE(current_index,0);
        auto& cell_nodes = cell_nodes_per_row->at(current_index);
        cell_nodes.push_back(child);
      }
    }
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
  // Confidence check, make sure the node is in the tree.
  AXNode* node = table_node;
  while (node && node != tree->root()) {
    node = node->GetParent();
  }
  DCHECK_EQ(node, tree->root());
#endif

  if (!IsTableLike(table_node->GetRole()) || table_node->IsIgnored()) {
    return nullptr;
  }

  AXTableInfo* info = new AXTableInfo(tree, table_node);
  bool success = info->Update();
  DCHECK(success);

  return info;
}

bool AXTableInfo::Update() {
  if (!table_node_->IsTable()) {
    return false;
  }

  ClearVectors();

  std::vector<std::vector<AXNode*>> cell_nodes_per_row;
  caption_id = 0;

  // Get the optional row and column count from the table. If we encounter
  // a cell with an index or span larger than this, we'll update the
  // table row and column count to be large enough to fit all cells.
  row_count = GetSizeTAttribute(*table_node_, IntAttribute::kTableRowCount);
  col_count = GetSizeTAttribute(*table_node_, IntAttribute::kTableColumnCount);

  // Note - GetIntAttribute returns 0 if no value has been specified for the
  // attribute.
  aria_row_count =
      int{table_node_->GetIntAttribute(IntAttribute::kAriaRowCount)};
  aria_col_count =
      int{table_node_->GetIntAttribute(IntAttribute::kAriaColumnCount)};

  // Find all the rows.

  if (!isRowlessTable(table_node_)) {
    FindRows(table_node_, &row_nodes, caption_id);
    FindCells(&row_nodes, &cell_nodes_per_row);
    // Iterate over the cells and build up an array of CellData
    // entries, one for each cell. Compute the actual row and column
    BuildCellDataVectorFromRowAndCellNodes(row_nodes, cell_nodes_per_row);
    DCHECK_EQ(cell_nodes_per_row.size(), row_nodes.size());
  } else {
    FindCellsForRowlessTable(table_node_, &cell_nodes_per_row);
    BuildCellDataVectorFromCellNodes(cell_nodes_per_row);
  }

  // At this point we have computed valid row and column indices for
  // every cell in the table, and an accurate row and column count for the
  // whole table that fits every cell and its spans. The final step is to
  // fill in a 2-dimensional array that lets us look up an individual cell
  // by its (row, column) coordinates, plus arrays to hold row and column
  // headers.
  BuildCellAndHeaderVectorsFromCellData();

  // On Mac, we add a few extra nodes to the table - see comment
  // at the top of UpdateExtraMacNodes for details.
#if defined(AX_EXTRA_MAC_NODES)
  UpdateExtraMacNodes();
#endif

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
  all_col_headers.clear();
  row_headers.clear();
  cell_ids.clear();
  unique_cell_ids.clear();
  cell_data_vector.clear();
  row_nodes.clear();
  cell_id_to_index.clear();
  row_id_to_index.clear();
  incremental_row_col_map_.clear();
}

void AXTableInfo::BuildCellData(AXNode* cell,
                                AXNode* row_or_first_cell,
                                CellBuildState& state) {
  // Fill in basic info in CellData.
  CellData cell_data;
  unique_cell_ids.push_back(cell->id());
  cell_id_to_index[cell->id()] = state.cell_index++;
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
  cell_data.col_index = std::max(cell_data.col_index, state.current_col_index);

  // And update the spanned column index.
  state.spanned_col_index =
      std::max(state.spanned_col_index, cell_data.col_index);

  if (state.is_first_cell_in_row) {
    state.is_first_cell_in_row = false;

    // If it's the first cell in the row, ensure the row index is
    // incrementing. The rest of the cells in this row are forced to have
    // the same row index.
    if (cell_data.row_index > state.current_row_index) {
      state.current_row_index = cell_data.row_index;
    } else {
      cell_data.row_index = state.current_row_index;
    }

    // The starting ARIA row and column index might be specified in
    // the row node, we should check there.
    if (!cell_data.aria_row_index) {
      cell_data.aria_row_index = GetSizeTAttribute(
          *row_or_first_cell, IntAttribute::kAriaCellRowIndex);
    }
    if (!cell_data.aria_col_index) {
      cell_data.aria_col_index = GetSizeTAttribute(
          *row_or_first_cell, IntAttribute::kAriaCellColumnIndex);
    }
    cell_data.aria_row_index =
        std::max(cell_data.aria_row_index, state.current_aria_row_index);
    state.current_aria_row_index = cell_data.aria_row_index;
  } else {
    // Don't allow the row index to change after the beginning
    // of a row.
    cell_data.row_index = state.current_row_index;
    cell_data.aria_row_index = state.current_aria_row_index;
  }

  // Adjust the spanned col index by looking at the incremental row col map.
  // This map contains already filled in values, accounting for spans, of
  // all row, col indices. The map should have filled in all values we need
  // (upper left triangle of cells of the table).
  while (true) {
    const auto& row_it = incremental_row_col_map_.find(state.current_row_index);
    if (row_it == incremental_row_col_map_.end()) {
      break;
    } else {
      const auto& col_it = row_it->second.find(state.spanned_col_index);
      if (col_it == row_it->second.end()) {
        break;
      } else {
        // A pre-existing cell resides in our desired position. Make a
        // best-fit to the right of the existing span.
        const CellData& spanned_cell_data = col_it->second;
        state.spanned_col_index =
            spanned_cell_data.col_index + spanned_cell_data.col_span;

        // Adjust the actual col index to be the best fit with the existing
        // spanned cell data.
        cell_data.col_index = state.spanned_col_index;
      }
    }
  }

  // Memoize the cell data using our incremental row col map.
  for (size_t r = cell_data.row_index;
       r < (cell_data.row_index + cell_data.row_span); r++) {
    for (size_t c = cell_data.col_index;
         c < (cell_data.col_index + cell_data.col_span); c++) {
      incremental_row_col_map_[r][c] = cell_data;
    }
  }

  // Ensure the ARIA col index is incrementing.
  cell_data.aria_col_index =
      std::max(cell_data.aria_col_index, state.current_aria_col_index);
  state.current_aria_col_index = cell_data.aria_col_index;

  // Update the row count and col count for the whole table to make
  // sure they're large enough to fit this cell, including its spans.
  // The -1 in the ARIA calculations is because ARIA indices are 1-based,
  // whereas all other indices are zero-based.
  row_count = std::max(row_count, cell_data.row_index + cell_data.row_span);
  col_count = std::max(col_count, cell_data.col_index + cell_data.col_span);
  if (aria_row_count != ax::mojom::kUnknownAriaColumnOrRowCount) {
    aria_row_count = std::max((aria_row_count),
                              static_cast<int>(state.current_aria_row_index +
                                               cell_data.row_span - 1));
  }
  if (aria_col_count != ax::mojom::kUnknownAriaColumnOrRowCount) {
    aria_col_count = std::max((aria_col_count),
                              static_cast<int>(state.current_aria_col_index +
                                               cell_data.col_span - 1));
  }
  // Update |current_col_index| to reflect the next available index after
  // this cell including its colspan. The next column index in this row
  // must be at least this large. Same for the current ARIA col index.
  state.current_col_index = cell_data.col_index + cell_data.col_span;
  state.current_aria_col_index = cell_data.aria_col_index + cell_data.col_span;
  state.spanned_col_index = state.current_col_index;

  // Add this cell to our vector.
  cell_data_vector.push_back(cell_data);
}

void AXTableInfo::BuildCellDataVectorFromCellNodes(
    const std::vector<std::vector<AXNode*>>& cell_nodes_per_row) {
  // Iterate over the cells and build up an array of CellData
  // entries, one for each cell. Compute the actual row and column
  // indices for each cell by taking the specified row and column
  // index in the accessibility tree if legal, but replacing it with
  // valid table coordinates otherwise.
  CellBuildState state;
  state.cell_index = 0;
  state.current_aria_row_index = 1;
  for (auto& cells_in_row : cell_nodes_per_row) {
    AXNode* first_cell_node = cells_in_row[0];
    state.is_first_cell_in_row = true;
    state.current_col_index = 0;
    state.current_aria_col_index = 1;

    // Make sure the row index is always at least as high as the one reported by
    // the source tree.
    state.current_row_index = GetSizeTAttribute(
        *first_cell_node, ax::mojom::IntAttribute::kTableCellRowIndex);
    state.spanned_col_index = 0;
    for (AXNode* cell : cells_in_row) {
      BuildCellData(cell, first_cell_node, state);
    }

    // At the end of each row, increment |current_aria_row_index| to reflect the
    // next available index after this row. The next row index must be at least
    // this large. Also update |next_row_index|.
    state.current_aria_row_index++;
  }
}

void AXTableInfo::BuildCellDataVectorFromRowAndCellNodes(
    const std::vector<raw_ptr<AXNode, VectorExperimental>>& row_node_list,
    const std::vector<std::vector<AXNode*>>& cell_nodes_per_row) {
  // Iterate over the cells and build up an array of CellData
  // entries, one for each cell. Compute the actual row and column
  // indices for each cell by taking the specified row and column
  // index in the accessibility tree if legal, but replacing it with
  // valid table coordinates otherwise.
  CellBuildState state;
  state.cell_index = 0;
  state.current_aria_row_index = 1;
  size_t next_row_index = 0;
  for (size_t i = 0; i < cell_nodes_per_row.size(); i++) {
    auto& cell_nodes_in_this_row = cell_nodes_per_row[i];
    AXNode* row_node = row_node_list[i];
    state.is_first_cell_in_row = true;
    state.current_col_index = 0;
    state.current_aria_col_index = 1;

    // Make sure the row index is always at least as high as the one reported by
    // the source tree.
    row_id_to_index[row_node->id()] =
        std::max(next_row_index,
                 GetSizeTAttribute(*row_node, IntAttribute::kTableRowIndex));
    state.current_row_index = row_id_to_index[row_node->id()];
    state.spanned_col_index = 0;
    for (AXNode* cell : cell_nodes_in_this_row) {
      BuildCellData(cell, row_node, state);
    }

    // At the end of each row, increment |current_aria_row_index| to reflect the
    // next available index after this row. The next row index must be at least
    // this large. Also update |next_row_index|.
    state.current_aria_row_index++;
    next_row_index = state.current_row_index + 1;
  }
}

void AXTableInfo::BuildCellAndHeaderVectorsFromCellData() {
  // Allocate space for the 2-D array of cell IDs and 1-D
  // arrays of row headers and column headers.
  row_headers.resize(row_count);
  col_headers.resize(col_count);
  // Fill in the arrays.
  //
  // At this point we have computed valid row and column indices for
  // every cell in the table, and an accurate row and column count for the
  // whole table that fits every cell and its spans. The final step is to
  // fill in a 2-dimensional array that lets us look up an individual cell
  // by its (row, column) coordinates, plus arrays to hold row and column
  // headers.

  // For cells.
  cell_ids.resize(row_count);
  for (size_t r = 0; r < row_count; r++) {
    cell_ids[r].resize(col_count);
    for (size_t c = 0; c < col_count; c++) {
      const auto& row_it = incremental_row_col_map_.find(r);
      if (row_it != incremental_row_col_map_.end()) {
        const auto& col_it = row_it->second.find(c);
        if (col_it != row_it->second.end()) {
          cell_ids[r][c] = col_it->second.cell->id();
        }
      }
    }
  }

  // No longer need this.
  incremental_row_col_map_.clear();

  // For relations.
  for (auto& cell_data : cell_data_vector) {
    for (size_t r = cell_data.row_index;
         r < cell_data.row_index + cell_data.row_span; r++) {
      DCHECK_LT(r, row_count);
      for (size_t c = cell_data.col_index;
           c < cell_data.col_index + cell_data.col_span; c++) {
        DCHECK_LT(c, col_count);
        AXNode* cell = cell_data.cell;
        if (cell->GetRole() == ax::mojom::Role::kColumnHeader) {
          // If this is a column header spanning vertically, we'll encounter
          // this cell multiple times as we scan down the column. Don't add it
          // twice just because it takes up more than one space in the table.
          if (!col_headers[c].empty() && col_headers[c].back() == cell->id()) {
            continue;
          }
          col_headers[c].push_back(cell->id());
          all_col_headers.push_back(cell->id());
        } else if (cell->GetRole() == ax::mojom::Role::kRowHeader) {
          // If this is a row header spanning horizontally, we'll encounter this
          // cell multiple times as we scan across the row.
          // Don't add it twice just because it takes up more than one space in
          // the table.
          if (!row_headers[r].empty() && row_headers[r].back() == cell->id()) {
            continue;
          }
          row_headers[r].push_back(cell->id());
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
  // the source tree, which are all positive.
  //
  // Each column has the kColumnIndex attribute set, and then each of the cells
  // in that column gets added as an indirect ID. That exposes them as children
  // via Mac APIs but ensures we don't explore those nodes multiple times when
  // walking the tree. The column also has the ID of the first column header
  // set.
  //
  // The table header container is just a node with all of the headers in the
  // table as indirect children.

  // Delete old extra nodes.
  ClearExtraMacNodes();

  // There is one node for each column, and one more for the table header
  // container.
  size_t extra_node_count = col_count + 1;
  std::vector<raw_ptr<AXNode, VectorExperimental>> new_extra_mac_nodes;
  new_extra_mac_nodes.reserve(extra_node_count);
  std::vector<AXTreeObserver::Change> changes;
  // Reserve room for the extra Mac nodes plus for the table itself.
  changes.reserve(extra_node_count + 1);

  for (size_t i = 0; i < col_count; i++) {
    new_extra_mac_nodes.push_back(CreateExtraMacColumnNode(i));
    changes.emplace_back(new_extra_mac_nodes[i],
                         AXTreeObserver::ChangeType::NODE_CREATED);
  }
  new_extra_mac_nodes.push_back(CreateExtraMacTableHeaderNode());
  changes.emplace_back(new_extra_mac_nodes[col_count],
                       AXTreeObserver::ChangeType::NODE_CREATED);

  {
    ScopedTreeUpdateInProgressStateSetter tree_update_in_progress(*tree_);

    // Add the newly created columns to the accessibility tree.
    extra_mac_nodes.swap(new_extra_mac_nodes);

    // Update the newly added columns to reflect the current state of the table.
    for (size_t i = 0; i < col_count; i++) {
      UpdateExtraMacColumnNodeAttributes(i);
    }

    // Update the table header container to contain all column headers. Row
    // headers should not be included, according to the Core-AAM 1.2 about the
    // table role.
    AXNodeData data = extra_mac_nodes[col_count]->data();
    data.intlist_attributes.clear();
    data.AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                             all_col_headers);
    extra_mac_nodes[col_count]->SetData(data);

  }  // tree_update_in_progress.

  changes.emplace_back(table_node_, AXTreeObserver::ChangeType::NODE_CHANGED);

  for (AXNode* node : extra_mac_nodes) {
    for (AXTreeObserver& observer : tree_->observers()) {
      observer.OnNodeCreated(tree_, node);
    }
  }

  for (AXTreeObserver& observer : tree_->observers()) {
    observer.OnAtomicUpdateFinished(tree_, /* root_changed= */ false, changes);
  }
}

AXNode* AXTableInfo::CreateExtraMacColumnNode(size_t col_index) {
  AXNodeID id = tree_->GetNextNegativeInternalNodeId();
  size_t index_in_parent = col_index + table_node_->children().size();
  int32_t unignored_index_in_parent =
      col_index + table_node_->GetUnignoredChildCount();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent,
                            unignored_index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kColumn;
  node->SetData(data);
  return node;
}

AXNode* AXTableInfo::CreateExtraMacTableHeaderNode() {
  AXNodeID id = tree_->GetNextNegativeInternalNodeId();
  size_t index_in_parent = col_count + table_node_->children().size();
  int32_t unignored_index_in_parent =
      col_count + table_node_->GetUnignoredChildCount();
  AXNode* node = new AXNode(tree_, table_node_, id, index_in_parent,
                            unignored_index_in_parent);
  AXNodeData data;
  data.id = id;
  data.role = ax::mojom::Role::kTableHeaderContainer;
  node->SetData(data);
  return node;
}

void AXTableInfo::UpdateExtraMacColumnNodeAttributes(size_t col_index) {
  AXNodeData data = extra_mac_nodes[col_index]->data();
  data.int_attributes.clear();

  // Update the column index.
  data.AddIntAttribute(IntAttribute::kTableColumnIndex,
                       static_cast<int32_t>(col_index));

  // Update the column header.
  if (!col_headers[col_index].empty()) {
    data.AddIntAttribute(IntAttribute::kTableColumnHeaderId,
                         col_headers[col_index][0]);
  }

  // Update the list of cells in the column.
  data.intlist_attributes.clear();
  std::vector<AXNodeID> col_nodes;
  AXNodeID last = 0;
  for (size_t row_index = 0; row_index < row_count; row_index++) {
    AXNodeID cell_id = cell_ids[row_index][col_index];
    if (cell_id != 0 && cell_id != last) {
      col_nodes.push_back(cell_id);
    }
    last = cell_id;
  }
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kIndirectChildIds,
                           col_nodes);
  extra_mac_nodes[col_index]->SetData(data);
}

void AXTableInfo::ClearExtraMacNodes() {
  if (extra_mac_nodes.empty()) {
    return;
  }

  std::set<AXNodeID> deleting_node_ids;
  for (AXNode* extra_mac_node : extra_mac_nodes) {
    deleting_node_ids.insert(extra_mac_node->id());
    for (AXTreeObserver& observer : tree_->observers()) {
      observer.OnNodeWillBeDeleted(tree_, extra_mac_node);
    }
  }

  for (AXTreeObserver& observer : tree_->observers()) {
    observer.OnAtomicUpdateStarting(tree_, deleting_node_ids, {});
  }

  {
    ScopedTreeUpdateInProgressStateSetter tree_update_in_progress(*tree_);

    for (AXNode* extra_mac_node : extra_mac_nodes) {
      delete extra_mac_node;
    }

    extra_mac_nodes.clear();

  }  // tree_update_in_progress.

  for (AXNodeID deleted_id : deleting_node_ids) {
    for (AXTreeObserver& observer : tree_->observers()) {
      observer.OnNodeDeleted(tree_, deleted_id);
    }
  }

  for (AXTreeObserver& observer : tree_->observers()) {
    observer.OnAtomicUpdateFinished(
        tree_, /* root_changed= */ false,
        {{table_node_, AXTreeObserver::ChangeType::NODE_CHANGED}});
  }
}

// The first cell in a row is important because it stores the ARIA row index.
// We recursively check generic containers like <div> and any
// nodes that are ignored, but we don't search any other roles
// in-between a table row and its cells.
const AXNode* AXTableInfo::GetFirstCellInRow(const AXNode* row) const {
  const AXNode* child = row;
  while (true) {
    child = child->GetUnignoredChildAtIndex(0);
    if (!child) {
      return nullptr;
    }
    if (child->GetRole() != ax::mojom::Role::kGenericContainer) {
      break;
    }
  }
  return IsCellOrTableHeader(child->GetRole()) ? child : nullptr;
}

std::string AXTableInfo::ToString() const {
  // First, scan through to get the length of the largest id.
  int padding = 0;
  for (size_t r = 0; r < row_count; r++) {
    for (size_t c = 0; c < col_count; c++) {
      // Extract the length of the id for padding purposes.
      padding = std::max(padding, static_cast<int>(log10(cell_ids[r][c])));
    }
  }

  std::string result;
  for (size_t r = 0; r < row_count; r++) {
    result += "|";
    for (size_t c = 0; c < col_count; c++) {
      int cell_id = cell_ids[r][c];
      result += base::NumberToString(cell_id);
      int cell_padding = padding;
      if (cell_id != 0) {
        cell_padding = padding - static_cast<int>(log10(cell_id));
      }
      result += std::string(cell_padding, ' ') + '|';
    }
    result += "\n";
  }
  return result;
}

AXTableInfo::AXTableInfo(AXTree* tree, AXNode* table_node)
    : tree_(tree), table_node_(table_node) {}

AXTableInfo::~AXTableInfo() {
  ClearExtraMacNodes();
}

}  // namespace ui
