// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/unified_desktop_utils.h"

#include <map>
#include <set>

#include "base/containers/stack.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/display/types/display_constants.h"

namespace display {

namespace {

// Defines a row and column indices of a cell in the layout matrix.
struct Cell {
  int row;
  int column;

  Cell(int r, int c) : row(r), column(c) {}
};

// Validates that the display placements defines a graph where there is a path
// from each display to the primary display (root) and there are no cycles or
// unparented displays.
using DisplayChildToParentMap = std::map<int64_t, int64_t>;
bool ValidateDisplayGraph(const DisplayChildToParentMap& child_to_parent,
                          int64_t primary_id) {
  for (const auto& iter : child_to_parent) {
    int64_t current_id = iter.first;
    if (current_id == primary_id) {
      // The primary display should not have a parent, and shouldn't exist in
      // the map as a key. That's a potential cycle.
      LOG(ERROR) << "Primary display must not have a parent.";
      return false;
    }

    std::set<int64_t> visited_ids;
    while (current_id != primary_id) {
      if (!visited_ids.emplace(current_id).second) {
        LOG(ERROR) << "A cycle exists at display ID: " << current_id;
        return false;
      }

      const auto parent_iter = child_to_parent.find(current_id);
      if (parent_iter == child_to_parent.end()) {
        LOG(ERROR) << "Display ID: " << current_id << " has no parent.";
        return false;
      }

      current_id = parent_iter->second;
    }
  }

  return true;
}

// Builds and returns the Unified Desktop layout matrix given the display
// |layout|. This function must only be called on an already-validated |layout|.
// Returns an empty matrix if an error occurs.
UnifiedDesktopLayoutMatrix BuildDisplayMatrix(const DisplayLayout& layout) {
  // Maps a display ID to its Cell position in the matrix.
  std::map<int64_t, Cell> displays_cells;
  // The root primary display is at (0, 0).
  displays_cells.emplace(layout.primary_id, Cell(0, 0));

  // After we finish building the Cells, we might have some displays
  // positioned at negative cell coordinates (relative to the root primary
  // display). We need to normalize our Cells so that the least row and column
  // indices are zeros.
  // Calculate the min/max row and column indices.
  int max_row = 0;
  int max_column = 0;
  int min_row = 0;
  int min_column = 0;

  // Calculate the Cell positions of all displays in the placement list.
  for (const auto& placement : layout.placement_list) {
    int64_t current_display_id = placement.display_id;
    base::stack<DisplayPlacement> unhandled_displays;
    while (displays_cells.count(current_display_id) == 0) {
      auto placement_iter = std::find_if(
          layout.placement_list.begin(), layout.placement_list.end(),
          [current_display_id](const DisplayPlacement& p) {
            return p.display_id == current_display_id;
          });
      DCHECK(placement_iter != layout.placement_list.end());
      unhandled_displays.emplace(*placement_iter);
      current_display_id = placement_iter->parent_display_id;
    }

    // For each unhandled display, find its parent's cell, and use it to deduce
    // its own cell.
    while (!unhandled_displays.empty()) {
      const DisplayPlacement current_placement = unhandled_displays.top();
      unhandled_displays.pop();
      const Cell& parent_cell =
          displays_cells.at(current_placement.parent_display_id);
      std::map<int64_t, Cell>::iterator new_cell_itr;
      switch (current_placement.position) {
        case DisplayPlacement::TOP:
          // Top of its parent. Go up a row (row - 1).
          new_cell_itr =
              displays_cells
                  .emplace(current_placement.display_id,
                           Cell(parent_cell.row - 1, parent_cell.column))
                  .first;
          break;

        case DisplayPlacement::RIGHT:
          // Right of its parent. Go right a column (column + 1).
          new_cell_itr =
              displays_cells
                  .emplace(current_placement.display_id,
                           Cell(parent_cell.row, parent_cell.column + 1))
                  .first;
          break;

        case DisplayPlacement::BOTTOM:
          // Bottom of its parent. Go down a row (row + 1).
          new_cell_itr =
              displays_cells
                  .emplace(current_placement.display_id,
                           Cell(parent_cell.row + 1, parent_cell.column))
                  .first;
          break;

        case DisplayPlacement::LEFT:
          // Left of its parent. Go left a column (column - 1).
          new_cell_itr =
              displays_cells
                  .emplace(current_placement.display_id,
                           Cell(parent_cell.row, parent_cell.column - 1))
                  .first;
          break;
      }

      const Cell& cell = new_cell_itr->second;
      max_row = std::max(max_row, cell.row);
      max_column = std::max(max_column, cell.column);
      min_row = std::min(min_row, cell.row);
      min_column = std::min(min_column, cell.column);
    }
  }

  // Now build the matrix.
  UnifiedDesktopLayoutMatrix matrix;
  const size_t num_rows = max_row - min_row + 1;
  const size_t num_columns = max_column - min_column + 1;

  if (displays_cells.size() != num_rows * num_columns) {
    LOG(ERROR) << "Unified Desktop layout matrix has wrong dimentions";
    // Return an empty matrix, ValidateMatrix() will catch it as invalid.
    return matrix;
  }

  matrix.resize(num_rows);
  for (auto& matrix_row : matrix)
    matrix_row.resize(num_columns, display::kInvalidDisplayId);

  for (const auto& iter : displays_cells) {
    const Cell& cell = iter.second;
    const int row_index = cell.row - min_row;
    const int column_index = cell.column - min_column;
    matrix[row_index][column_index] = iter.first;
  }

  return matrix;
}

}  // namespace

bool ValidateMatrix(const UnifiedDesktopLayoutMatrix& matrix) {
  if (matrix.empty())
    return false;

  const size_t column_count = matrix[0].size();
  if (column_count == 0)
    return false;

  for (const auto& row : matrix) {
    if (row.size() != column_count) {
      LOG(ERROR) << "Wrong matrix dimensions. Unequal rows sizes.";
      return false;
    }

    // No holes or repeated IDs are allowed.
    for (const auto& id : row) {
      if (id == display::kInvalidDisplayId) {
        LOG(ERROR) << "Unified Desktop layout matrix has an empty cell in it.";
        return false;
      }
    }
  }

  return true;
}

bool BuildUnifiedDesktopMatrix(const DisplayIdList& ids_list,
                               const DisplayLayout& layout,
                               UnifiedDesktopLayoutMatrix* out_matrix) {
  // The primary display should be in the IDs list.
  if (!base::Contains(ids_list, layout.primary_id)) {
    LOG(ERROR) << "The primary ID: " << layout.primary_id
               << " is not in the IDs list.";
    return false;
  }

  // Each ID in |ids_list| must have a placement in the layout except the
  // primary display.
  for (const auto& id : ids_list) {
    if (id == layout.primary_id)
      continue;
    const auto iter =
        std::find_if(layout.placement_list.begin(), layout.placement_list.end(),
                     [id](const DisplayPlacement& placement) {
                       return placement.display_id == id;
                     });
    if (iter == layout.placement_list.end()) {
      LOG(ERROR) << "Display with ID: " << id << " has no placement.";
      return false;
    }
  }

  if (layout.placement_list.empty()) {
    LOG(ERROR) << "Placement list is empty.";
    return false;
  }

  // This map is used to validate that each display has no more than one child
  // on eithr of its sides.
  std::map<int64_t, std::set<DisplayPlacement::Position>> displays_filled_sides;

  // This map is used to validate that all displays has a path to the primary
  // (root) display with no cycles.
  DisplayChildToParentMap child_to_parent;

  bool has_primary_as_parent = false;
  for (const auto& placement : layout.placement_list) {
    // Unified mode placements are not allowed to have offsets.
    if (placement.offset != 0) {
      LOG(ERROR) << "Unified mode placements are not allowed to have offsets.";
      return false;
    }

    if (placement.display_id == kInvalidDisplayId) {
      LOG(ERROR) << "display_id is not initialized";
      return false;
    }
    if (placement.parent_display_id == kInvalidDisplayId) {
      LOG(ERROR) << "parent_display_id is not initialized";
      return false;
    }
    if (placement.display_id == placement.parent_display_id) {
      LOG(ERROR) << "display_id must not be the same as parent_display_id";
      return false;
    }
    if (!base::Contains(ids_list, placement.display_id)) {
      LOG(ERROR) << "display_id: " << placement.display_id
                 << " is not in the id list: " << placement.ToString();
      return false;
    }

    if (!base::Contains(ids_list, placement.parent_display_id)) {
      LOG(ERROR) << "parent_display_id: " << placement.parent_display_id
                 << " is not in the id list: " << placement.ToString();
      return false;
    }

    if (!displays_filled_sides[placement.parent_display_id]
             .emplace(placement.position)
             .second) {
      LOG(ERROR) << "Parent display with ID: " << placement.parent_display_id
                 << " has more than one display on the same side: "
                 << placement.position;
      return false;
    }

    if (!child_to_parent
             .emplace(placement.display_id, placement.parent_display_id)
             .second) {
      LOG(ERROR) << "Display ID: " << placement.display_id << " appears more "
                 << "than once in the placement list.";
      return false;
    }

    has_primary_as_parent |= layout.primary_id == placement.parent_display_id;
  }

  if (!has_primary_as_parent) {
    LOG(ERROR) << "At least, one placement must have the primary as a parent.";
    return false;
  }

  if (!ValidateDisplayGraph(child_to_parent, layout.primary_id))
    return false;

  UnifiedDesktopLayoutMatrix matrix = BuildDisplayMatrix(layout);
  if (!ValidateMatrix(matrix))
    return false;

  *out_matrix = matrix;
  return true;
}

}  // namespace display
