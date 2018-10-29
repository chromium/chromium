// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TABLE_INFO_H_
#define UI_ACCESSIBILITY_AX_TABLE_INFO_H_

#include <set>
#include <vector>

#include "base/containers/hash_tables.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXTree;
class AXNode;

// This helper class computes info about tables and grids in AXTrees.
class AX_EXPORT AXTableInfo {
 public:
  // Returns nullptr if the node is not a valid table or grid node.
  static AXTableInfo* Create(AXTree* tree, AXNode* table_node);

  ~AXTableInfo();

  // Called automatically on Create(), but must be called again any time
  // the table is invalidated. Returns true if this is still a table.
  bool Update();

  // Whether the data is valid. Whenever the tree is updated in any way,
  // every AXTableInfo is invalidated and needs to be recomputed, just
  // to be safe.
  bool valid() const { return valid_; }
  void Invalidate();

  // The real row count, guaranteed to be at least as large as the
  // maximum row index of any cell.
  int32_t row_count = 0;

  // The real column count, guaranteed to be at least as large as the
  // maximum column index of any cell.
  int32_t col_count = 0;

  // List of column header nodes IDs for each column index.
  std::vector<std::vector<int32_t>> col_headers;

  // List of row header node IDs for each row index.
  std::vector<std::vector<int32_t>> row_headers;

  // All header cells.
  std::vector<int32_t> all_headers;

  // 2-D array of [row][column] -> cell node ID.
  // This may contain duplicates if there is a rowspan or
  // colspan. The entry is empty (zero) only if the cell
  // really is missing from the table.
  std::vector<std::vector<int32_t>> cell_ids;

  // Set of all unique cell node IDs in the table.
  std::vector<int32_t> unique_cell_ids;

  // Extra computed nodes for the accessibility tree for macOS:
  // one column node for each table column, followed by one
  // table header container node.
  std::vector<AXNode*> extra_mac_nodes;

  // Map from each cell's node ID to its index in unique_cell_ids.
  base::hash_map<int32_t, int32_t> cell_id_to_index;

 private:
  AXTableInfo(AXTree* tree, AXNode* table_node);

  void UpdateExtraMacNodes();
  void ClearExtraMacNodes();
  AXNode* CreateExtraMacColumnNode(int col_index);
  AXNode* CreateExtraMacTableHeaderNode();
  void UpdateExtraMacColumnNodeAttributes(int col_index);

  AXTree* tree_ = nullptr;
  AXNode* table_node_ = nullptr;
  bool valid_ = false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TABLE_INFO
