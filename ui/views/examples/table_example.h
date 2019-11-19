// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/examples/example_base.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class Checkbox;
class TableView;

namespace examples {

class VIEWS_EXAMPLES_EXPORT TableExample : public ExampleBase,
                                           public ui::TableModel,
                                           public TableGrouper,
                                           public TableViewObserver,
                                           public ButtonListener {
 public:
  TableExample();
  ~TableExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  gfx::ImageSkia GetIcon(int row) override;
  base::string16 GetTooltip(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // TableGrouper:
  void GetGroupRange(int model_index, GroupRange* range) override;

  // TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnMiddleClick() override;
  void OnKeyDown(ui::KeyboardCode virtual_keycode) override;

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

 private:
  // The table to be tested.
  TableView* table_ = nullptr;

  Checkbox* column1_visible_checkbox_ = nullptr;
  Checkbox* column2_visible_checkbox_ = nullptr;
  Checkbox* column3_visible_checkbox_ = nullptr;
  Checkbox* column4_visible_checkbox_ = nullptr;

  SkBitmap icon1_;
  SkBitmap icon2_;

  DISALLOW_COPY_AND_ASSIGN(TableExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
