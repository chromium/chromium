// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/test_table_model.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/image/image_skia.h"

TestTableModel::TestTableModel(int row_count)
    : row_count_(row_count), observer_(nullptr) {}

TestTableModel::~TestTableModel() = default;

int TestTableModel::RowCount() {
  return row_count_;
}

base::string16 TestTableModel::GetText(int row, int column_id) {
  return base::ASCIIToUTF16(base::NumberToString(row) + "x" +
                            base::NumberToString(column_id));
}

gfx::ImageSkia TestTableModel::GetIcon(int row) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeN32Premul(16, 16));
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

void TestTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}
