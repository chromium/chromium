// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/empty_combobox_model.h"

#include <string>

#include "base/notreached.h"

namespace views::internal {

EmptyComboboxModel::EmptyComboboxModel() = default;
EmptyComboboxModel::~EmptyComboboxModel() = default;

size_t EmptyComboboxModel::GetItemCount() const {
  return 0;
}

std::u16string EmptyComboboxModel::GetItemAt(size_t index) const {
  NOTREACHED();
}

std::optional<size_t> EmptyComboboxModel::GetDefaultIndex() const {
  return std::nullopt;
}

}  // namespace views::internal
