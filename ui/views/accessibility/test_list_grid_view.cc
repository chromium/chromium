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

void TestListGridView::SetAriaTableSize(int row_count, int column_count) {
  GetViewAccessibility().SetAriaTableRowCount(row_count);
  GetViewAccessibility().SetAriaTableColumnCount(column_count);
}

void TestListGridView::SetTableSize(int row_count, int column_count) {
  GetViewAccessibility().SetTableRowCount(row_count);
  GetViewAccessibility().SetTableColumnCount(column_count);
}

void TestListGridView::UnsetAriaTableSize() {
  GetViewAccessibility().ClearAriaTableRowCount();
  GetViewAccessibility().ClearAriaTableColumnCount();
}

void TestListGridView::UnsetTableSize() {
  GetViewAccessibility().ClearTableRowCount();
  GetViewAccessibility().ClearTableColumnCount();
}

BEGIN_METADATA(TestListGridView)
END_METADATA

}  // namespace test
}  // namespace views
