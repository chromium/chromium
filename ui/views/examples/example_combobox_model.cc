// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/example_combobox_model.h"

#include "base/strings/utf_string_conversions.h"

namespace views {
namespace examples {

ExampleComboboxModel::ExampleComboboxModel(const char* const* strings,
                                           int count)
    : strings_(strings), count_(count) {}

ExampleComboboxModel::~ExampleComboboxModel() = default;

int ExampleComboboxModel::GetItemCount() const {
  return count_;
}

base::string16 ExampleComboboxModel::GetItemAt(int index) {
  return base::ASCIIToUTF16(strings_[index]);
}

}  // namespace examples
}  // namespace views
