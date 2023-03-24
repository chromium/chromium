// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Combobox;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ComboboxExample : public ExampleBase {
 public:
  ComboboxExample();
  ComboboxExample(const ComboboxExample&) = delete;
  ComboboxExample& operator=(const ComboboxExample&) = delete;
  ~ComboboxExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void ValueChanged();

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Combobox* combobox_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_COMBOBOX_EXAMPLE_H_
