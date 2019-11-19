// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class ToggleButton;

namespace examples {

// ToggleButtonExample exercises a ToggleButton control.
class VIEWS_EXAMPLES_EXPORT ToggleButtonExample : public ExampleBase,
                                                  public ButtonListener {
 public:
  ToggleButtonExample();
  ~ToggleButtonExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // The only control in this test.
  ToggleButton* button_ = nullptr;

  int count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ToggleButtonExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TOGGLE_BUTTON_EXAMPLE_H_
