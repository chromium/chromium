// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/empty_combobox_model.h"

#include <string>

#include "base/notreached.h"

namespace views {
namespace internal {

EmptyComboboxModel::EmptyComboboxModel() = default;
EmptyComboboxModel::~EmptyComboboxModel() = default;

int EmptyComboboxModel::GetItemCount() const {
  return 0;
}

std::u16string EmptyComboboxModel::GetItemAt(int index) const {
  NOTREACHED();
  return std::u16string();
}

int EmptyComboboxModel::GetDefaultIndex() const {
  return -1;
}

}  // namespace internal
}  // namespace views
