// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/example_combobox_model.h"

#include "base/strings/utf_string_conversions.h"

namespace views::examples {

ExampleComboboxModel::ExampleComboboxModel(const char* const* strings,
                                           size_t count)
    : strings_(strings), count_(count) {}

ExampleComboboxModel::~ExampleComboboxModel() = default;

size_t ExampleComboboxModel::GetItemCount() const {
  return count_;
}

std::u16string ExampleComboboxModel::GetItemAt(size_t index) const {
  return base::ASCIIToUTF16(strings_[index]);
}

}  // namespace views::examples
