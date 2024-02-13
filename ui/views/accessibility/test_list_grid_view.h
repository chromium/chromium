// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_TEST_LIST_GRID_VIEW_H_
#define UI_VIEWS_ACCESSIBILITY_TEST_LIST_GRID_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace views {
namespace test {

// Class used for testing row and column count accessibility APIs.
class TestListGridView : public View {
  METADATA_HEADER(TestListGridView, View)

 public:
  TestListGridView();
  ~TestListGridView() override;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void SetAriaTableSize(int row_count, int column_count);
  void SetTableSize(int row_count, int column_count);
  void UnsetAriaTableSize();
  void UnsetTableSize();

 private:
  std::optional<int> aria_row_count = std::nullopt;
  std::optional<int> aria_column_count = std::nullopt;
  std::optional<int> table_row_count = std::nullopt;
  std::optional<int> table_column_count = std::nullopt;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_TEST_LIST_GRID_VIEW_H_
