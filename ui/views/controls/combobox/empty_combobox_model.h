// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_EMPTY_COMBOBOX_MODEL_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_EMPTY_COMBOBOX_MODEL_H_

#include "ui/base/models/combobox_model.h"

namespace views::internal {

// An empty model for a combo box.
class EmptyComboboxModel final : public ui::ComboboxModel {
 public:
  EmptyComboboxModel();
  EmptyComboboxModel(EmptyComboboxModel&) = delete;
  EmptyComboboxModel& operator=(const EmptyComboboxModel&) = delete;
  ~EmptyComboboxModel() override;

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;
};

}  // namespace views::internal

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_EMPTY_COMBOBOX_MODEL_H_
