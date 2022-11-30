// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/view_observer.h"

namespace views {

class Checkbox;
class View;

namespace examples {

class VIEWS_EXAMPLES_EXPORT TableExample : public ExampleBase,
                                           public ui::TableModel,
                                           public TableGrouper,
                                           public TableViewObserver,
                                           public ViewObserver {
 public:
  TableExample();

  TableExample(const TableExample&) = delete;
  TableExample& operator=(const TableExample&) = delete;

  ~TableExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ui::TableModel:
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column_id) override;
  ui::ImageModel GetIcon(size_t row) override;
  std::u16string GetTooltip(size_t row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // TableGrouper:
  void GetGroupRange(size_t model_index, GroupRange* range) override;

  // TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnMiddleClick() override;
  void OnKeyDown(ui::KeyboardCode virtual_keycode) override;

  // ViewObserver:
  void OnViewThemeChanged(View* observed_view) override;
  void OnViewIsDeleting(View* observed_view) override;

 private:
  std::string SelectedColumnName();

  // The table to be tested.
  raw_ptr<TableView> table_ = nullptr;

  raw_ptr<Checkbox> column1_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column2_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column3_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column4_visible_checkbox_ = nullptr;

  SkBitmap icon1_;
  SkBitmap icon2_;

  base::ScopedObservation<View, ViewObserver> observer_{this};
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
