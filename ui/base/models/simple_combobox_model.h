// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
#define UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_

#include "base/macros.h"
#include "ui/base/models/combobox_model.h"

#include <vector>

namespace ui {

// A simple data model for a combobox that takes a string16 vector as the items.
// An empty string will be a separator.
class UI_BASE_EXPORT SimpleComboboxModel : public ComboboxModel {
 public:
  explicit SimpleComboboxModel(std::vector<base::string16> items);
  ~SimpleComboboxModel() override;

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;

 private:
  const std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SimpleComboboxModel);
};

}  // namespace ui

#endif  // UI_BASE_MODELS_SIMPLE_COMBOBOX_MODEL_H_
