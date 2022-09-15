// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/test_table_model.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/image/image_skia.h"

TestTableModel::TestTableModel(size_t row_count)
    : row_count_(row_count), observer_(nullptr) {}

TestTableModel::~TestTableModel() = default;

size_t TestTableModel::RowCount() {
  return row_count_;
}

std::u16string TestTableModel::GetText(size_t row, int column_id) {
  return base::ASCIIToUTF16(base::NumberToString(row) + "x" +
                            base::NumberToString(column_id));
}

ui::ImageModel TestTableModel::GetIcon(size_t row) {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeN32Premul(16, 16));
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

void TestTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}
