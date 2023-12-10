// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_TABLE_LAYOUT_VIEW_H_
#define UI_VIEWS_LAYOUT_TABLE_LAYOUT_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {

class VIEWS_EXPORT TableLayoutView : public View {
  METADATA_HEADER(TableLayoutView, View)

 public:
  TableLayoutView() = default;
  TableLayoutView(const TableLayoutView&) = delete;
  TableLayoutView& operator=(const TableLayoutView&) = delete;
  ~TableLayoutView() override = default;

  // The following surface the corresponding APIs from TableLayout. See the
  // comments/documentation on TableLayout for more information.
  TableLayoutView& AddColumn(LayoutAlignment h_align,
                             LayoutAlignment v_align,
                             float horizontal_resize,
                             TableLayout::ColumnSize size_type,
                             int fixed_width,
                             int min_width);
  TableLayoutView& AddPaddingColumn(float horizontal_resize, int width);
  TableLayoutView& AddRows(size_t n, float vertical_resize, int height = 0);
  TableLayoutView& AddPaddingRow(float vertical_resize, int height);
  TableLayoutView& LinkColumnSizes(std::vector<size_t> columns);
  TableLayoutView& SetLinkedColumnSizeLimit(int size_limit);
  TableLayoutView& SetMinimumSize(const gfx::Size& size);

 private:
  raw_ptr<TableLayout> table_layout_ =
      SetLayoutManager(std::make_unique<TableLayout>());
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, TableLayoutView, View)
VIEW_BUILDER_METHOD(AddColumn,
                    views::LayoutAlignment,
                    views::LayoutAlignment,
                    float,
                    views::TableLayout::ColumnSize,
                    int,
                    int)
VIEW_BUILDER_METHOD(AddPaddingColumn, float, int)
VIEW_BUILDER_METHOD(AddRows, size_t, float, int)
VIEW_BUILDER_METHOD(AddPaddingRow, float, int)
VIEW_BUILDER_METHOD(LinkColumnSizes, std::vector<size_t>)
VIEW_BUILDER_PROPERTY(int, LinkedColumnSizeLimit)
VIEW_BUILDER_PROPERTY(gfx::Size, MinimumSize)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, TableLayoutView)

#endif  // UI_VIEWS_LAYOUT_TABLE_LAYOUT_VIEW_H_
