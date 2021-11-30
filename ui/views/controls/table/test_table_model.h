// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
#define UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/models/table_model.h"

class TestTableModel : public ui::TableModel {
 public:
  explicit TestTableModel(int row_count);

  TestTableModel(const TestTableModel&) = delete;
  TestTableModel& operator=(const TestTableModel&) = delete;

  ~TestTableModel() override;

  // ui::TableModel overrides:
  int RowCount() override;
  std::u16string GetText(int row, int column_id) override;
  ui::ImageModel GetIcon(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

 private:
  int row_count_;
  raw_ptr<ui::TableModelObserver> observer_;
};

#endif  // UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
