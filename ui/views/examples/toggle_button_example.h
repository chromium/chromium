// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_

#include "ui/views/examples/example_base.h"

namespace views {
class ToggleButton;

namespace examples {

// ToggleButtonExample exercises a ToggleButton control.
class VIEWS_EXAMPLES_EXPORT ToggleButtonExample : public ExampleBase {
 public:
  ToggleButtonExample();

  ToggleButtonExample(const ToggleButtonExample&) = delete;
  ToggleButtonExample& operator=(const ToggleButtonExample&) = delete;

  ~ToggleButtonExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  int count_1_ = 0;
  int count_2_ = 0;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_
