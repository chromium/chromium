// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/simple_combobox_model.h"

#include <utility>

namespace ui {

SimpleComboboxModel::SimpleComboboxModel(std::vector<std::u16string> items)
    : items_(std::move(items)) {}

SimpleComboboxModel::~SimpleComboboxModel() {
}

int SimpleComboboxModel::GetItemCount() const {
  return items_.size();
}

std::u16string SimpleComboboxModel::GetItemAt(int index) const {
  return items_[index];
}

bool SimpleComboboxModel::IsItemSeparatorAt(int index) const {
  return items_[index].empty();
}

int SimpleComboboxModel::GetDefaultIndex() const {
  return 0;
}

}  // namespace ui
