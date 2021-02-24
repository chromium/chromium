// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_
#define UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_

#include "base/macros.h"
#include "ui/base/models/combobox_model.h"

namespace views {
namespace examples {

class ExampleComboboxModel : public ui::ComboboxModel {
 public:
  ExampleComboboxModel(const char* const* strings, int count);
  ~ExampleComboboxModel() override;

  // ui::ComboboxModel:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) const override;

 private:
  const char* const* const strings_;
  const int count_;

  DISALLOW_COPY_AND_ASSIGN(ExampleComboboxModel);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLE_COMBOBOX_MODEL_H_
