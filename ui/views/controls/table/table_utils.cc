// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_utils.h"

#include <stddef.h>

#include <algorithm>

#include "base/notreached.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/table/table_view.h"

namespace views {

const int kUnspecifiedColumnWidth = 90;

int WidthForContent(const gfx::FontList& header_font_list,
                    const gfx::FontList& content_font_list,
                    int padding,
                    int header_padding,
                    const ui::TableColumn& column,
                    ui::TableModel* model) {
  int width = header_padding;
  if (!column.title.empty())
    width =
        gfx::GetStringWidth(column.title, header_font_list) + header_padding;

  for (int i = 0, row_count = model->RowCount(); i < row_count; ++i) {
    const int cell_width =
        gfx::GetStringWidth(model->GetText(i, column.id), content_font_list);
    width = std::max(width, cell_width);
  }
  return width + padding;
}

std::vector<int> CalculateTableColumnSizes(
    int width,
    int first_column_padding,
    const gfx::FontList& header_font_list,
    const gfx::FontList& content_font_list,
    int padding,
    int header_padding,
    const std::vector<ui::TableColumn>& columns,
    ui::TableModel* model) {
  float total_percent = 0;
  int non_percent_width = 0;
  std::vector<int> content_widths(columns.size(), 0);
  for (size_t i = 0; i < columns.size(); ++i) {
    const ui::TableColumn& column(columns[i]);
    if (column.width <= 0) {
      if (column.percent > 0) {
        total_percent += column.percent;
        // Make sure there is at least enough room for the header.
        content_widths[i] =
            gfx::GetStringWidth(column.title, header_font_list) + padding +
            header_padding;
      } else {
        content_widths[i] =
            WidthForContent(header_font_list, content_font_list, padding,
                            header_padding, column, model);
        if (i == 0)
          content_widths[i] += first_column_padding;
      }
      non_percent_width += content_widths[i];
    } else {
      content_widths[i] = column.width;
      non_percent_width += column.width;
    }
  }

  std::vector<int> widths;
  const int available_width = width - non_percent_width;
  for (size_t i = 0; i < columns.size(); ++i) {
    const ui::TableColumn& column = columns[i];
    int column_width = content_widths[i];
    if (column.width <= 0 && column.percent > 0 && available_width > 0) {
      column_width +=
          static_cast<int>(available_width * (column.percent / total_percent));
    }
    widths.push_back(column_width == 0 ? kUnspecifiedColumnWidth
                                       : column_width);
  }

  // If no columns have specified a percent give the last column all the extra
  // space.
  if (!columns.empty() && total_percent == 0.f && available_width > 0 &&
      columns.back().width <= 0 && columns.back().percent == 0.f) {
    widths.back() += available_width;
  }

  return widths;
}

int TableColumnAlignmentToCanvasAlignment(
    ui::TableColumn::Alignment alignment) {
  switch (alignment) {
    case ui::TableColumn::LEFT:
      return gfx::Canvas::TEXT_ALIGN_LEFT;
    case ui::TableColumn::CENTER:
      return gfx::Canvas::TEXT_ALIGN_CENTER;
    case ui::TableColumn::RIGHT:
      return gfx::Canvas::TEXT_ALIGN_RIGHT;
  }
  NOTREACHED();
  return gfx::Canvas::TEXT_ALIGN_LEFT;
}

int GetClosestVisibleColumnIndex(const TableView* table, int x) {
  const std::vector<TableView::VisibleColumn>& columns(
      table->visible_columns());
  for (size_t i = 0; i < columns.size(); ++i) {
    if (x <= columns[i].x + columns[i].width)
      return static_cast<int>(i);
  }
  return static_cast<int>(columns.size()) - 1;
}

}  // namespace views
