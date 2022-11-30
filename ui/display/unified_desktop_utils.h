// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UNIFIED_DESKTOP_UTILS_H_
#define UI_DISPLAY_UNIFIED_DESKTOP_UTILS_H_

#include <vector>

#include "ui/display/display_export.h"
#include "ui/display/display_layout.h"

namespace display {

// Defines a display position in the unified display matrix.
enum class DisplayPositionInUnifiedMatrix {
  kTopLeft,
  kTopRight,
  kBottomLeft,
};

// Type of the matrix that represents the display layout in Unified Desktop
// mode. The ID of a display is placed in a cell in the matrix where that
// display is desired to be placed in the actual layout.
using UnifiedDesktopLayoutMatrix = std::vector<std::vector<int64_t>>;

// Validates that:
// - The matrix is not empty.
// - There are no holes (empty display IDs) in the matrix.
// - All matrix rows have equal non-zero widths.
bool DISPLAY_EXPORT ValidateMatrix(const UnifiedDesktopLayoutMatrix& matrix);

// Validates that the given display |layout| is convertable to a valid Unified
// Desktop layout matrix. If yes, then the matrix will be built and filled in
// |out_matrix| and true is returned. False is returned and |out_matrix| will be
// unchanged.
//
// Rules of a valid Unified Desktop layout:
// - All IDs in the |layout|'s |primary_id| and |placement_list| must exist in
//   the display |ids_list|.
// - There is a placement for each display in the |ids_list| except the primary
//   display.
// - The primary display must be the root.
// - There is a path from each display to the root display with no cycles.
// - No placement offsets are allowed.
// - The |layout|'s |placement_list| is not required to be sorted.
// - A display can only have at most one child on either of its sides.
// - Layouts that specify a display grid must be complete; i.e. no empty holes
//   in the grid.
bool DISPLAY_EXPORT
BuildUnifiedDesktopMatrix(const DisplayIdList& ids_list,
                          const DisplayLayout& layout,
                          UnifiedDesktopLayoutMatrix* out_matrix);

}  // namespace display

#endif  // UI_DISPLAY_UNIFIED_DESKTOP_UTILS_H_
