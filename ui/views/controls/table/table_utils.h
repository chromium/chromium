// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_UTILS_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_UTILS_H_

#include <optional>
#include <vector>

#include "ui/base/models/table_model.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
}

namespace views {

class TableView;

VIEWS_EXPORT extern const int kUnspecifiedColumnWidth;

// Returns the width needed to display the contents of the specified column.
// This is used internally by CalculateTableColumnSizes() and generally not
// useful by itself. |header_padding| is padding added to the header.
VIEWS_EXPORT int WidthForContent(const gfx::FontList& header_font_list,
                                 const gfx::FontList& content_font_list,
                                 int padding,
                                 int header_padding,
                                 const ui::TableColumn& column,
                                 ui::TableModel* model);

// Determines the width for each of the specified columns. |width| is the width
// to fit the columns into. |header_font_list| the font list used to draw the
// header and |content_font_list| the header used to draw the content. |padding|
// is extra horizontal spaced added to each cell, and |header_padding| added to
// the width needed for the header.
VIEWS_EXPORT std::vector<int> CalculateTableColumnSizes(
    int width,
    int first_column_padding,
    const gfx::FontList& header_font_list,
    const gfx::FontList& content_font_list,
    int padding,
    int header_padding,
    const std::vector<ui::TableColumn>& columns,
    ui::TableModel* model);

// Converts a TableColumn::Alignment to the alignment for drawing the string.
int TableColumnAlignmentToCanvasAlignment(ui::TableColumn::Alignment alignment);

// Returns the index of the closest visible column index to `x`. Return value is
// in terms of `table.visible_columns()`. Returns nullopt if there are no
// visible columns.
std::optional<size_t> GetClosestVisibleColumnIndex(const TableView& table,
                                                   int x);

// Returns the mirror of the table column alignment if the layout is
// right-to-left. If the layout is left-to-right, the same alignment is
// returned.
ui::TableColumn::Alignment GetMirroredTableColumnAlignment(
    ui::TableColumn::Alignment alignment);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_UTILS_H_
