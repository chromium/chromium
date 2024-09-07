// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/test_list_grid_view.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace views {
namespace test {

TestListGridView::TestListGridView() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kListGrid);
}

TestListGridView::~TestListGridView() = default;

void TestListGridView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (aria_row_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kAriaRowCount,
                               *aria_row_count);
  }
  if (aria_column_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                               *aria_column_count);
  }
  if (table_row_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount,
                               *table_row_count);
  }
  if (table_column_count) {
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                               *table_column_count);
  }
}

void TestListGridView::SetAriaTableSize(int row_count, int column_count) {
  aria_row_count = std::make_optional(row_count);
  aria_column_count = std::make_optional(column_count);
}

void TestListGridView::SetTableSize(int row_count, int column_count) {
  table_row_count = std::make_optional(row_count);
  table_column_count = std::make_optional(column_count);
}

void TestListGridView::UnsetAriaTableSize() {
  aria_row_count = std::nullopt;
  aria_column_count = std::nullopt;
}

void TestListGridView::UnsetTableSize() {
  table_row_count = std::nullopt;
  table_column_count = std::nullopt;
}

BEGIN_METADATA(TestListGridView)
END_METADATA

}  // namespace test
}  // namespace views
