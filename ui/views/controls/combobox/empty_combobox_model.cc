// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/empty_combobox_model.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/string16.h"

namespace views {
namespace internal {

EmptyComboboxModel::EmptyComboboxModel() = default;
EmptyComboboxModel::~EmptyComboboxModel() = default;

int EmptyComboboxModel::GetItemCount() const {
  return 0;
}

base::string16 EmptyComboboxModel::GetItemAt(int index) const {
  NOTREACHED();
  return base::string16();
}

int EmptyComboboxModel::GetDefaultIndex() const {
  return -1;
}

}  // namespace internal
}  // namespace views
