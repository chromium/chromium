// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class Checkbox;

namespace examples {

// CheckboxExample exercises a Checkbox control.
class VIEWS_EXAMPLES_EXPORT CheckboxExample : public ExampleBase {
 public:
  CheckboxExample();
  ~CheckboxExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // The number of times the contained checkbox has been clicked.
  int count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CheckboxExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_CHECKBOX_EXAMPLE_H_
