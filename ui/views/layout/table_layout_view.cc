// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/table_layout_view.h"

#include <utility>
#include <vector>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_manager.h"

namespace views {

TableLayoutView& TableLayoutView::AddColumn(LayoutAlignment h_align,
                                            LayoutAlignment v_align,
                                            float horizontal_resize,
                                            TableLayout::ColumnSize size_type,
                                            int fixed_width,
                                            int min_width) {
  table_layout_->AddColumn(h_align, v_align, horizontal_resize, size_type,
                           fixed_width, min_width);
  return *this;
}

TableLayoutView& TableLayoutView::AddPaddingColumn(float horizontal_resize,
                                                   int width) {
  table_layout_->AddPaddingColumn(horizontal_resize, width);
  return *this;
}

TableLayoutView& TableLayoutView::AddRows(size_t n,
                                          float vertical_resize,
                                          int height) {
  table_layout_->AddRows(n, vertical_resize, height);
  return *this;
}

TableLayoutView& TableLayoutView::AddPaddingRow(float vertical_resize,
                                                int height) {
  table_layout_->AddPaddingRow(vertical_resize, height);
  return *this;
}

TableLayoutView& TableLayoutView::LinkColumnSizes(std::vector<size_t> columns) {
  table_layout_->LinkColumnSizes(std::move(columns));
  return *this;
}

TableLayoutView& TableLayoutView::SetLinkedColumnSizeLimit(int size_limit) {
  table_layout_->SetLinkedColumnSizeLimit(size_limit);
  return *this;
}

TableLayoutView& TableLayoutView::SetMinimumSize(const gfx::Size& size) {
  table_layout_->SetMinimumSize(size);
  return *this;
}

BEGIN_METADATA(TableLayoutView)
END_METADATA

}  // namespace views
