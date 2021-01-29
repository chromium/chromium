// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
#define UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/models/table_model.h"

class TestTableModel : public ui::TableModel {
 public:
  explicit TestTableModel(int row_count);
  ~TestTableModel() override;

  // ui::TableModel overrides:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  gfx::ImageSkia GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  int row_count_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel);
};

#endif  // UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
