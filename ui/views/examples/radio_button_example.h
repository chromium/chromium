// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {

class LabelButton;
class RadioButton;

namespace examples {

class VIEWS_EXAMPLES_EXPORT RadioButtonExample : public ExampleBase,
                                                 public ButtonListener {
 public:
  RadioButtonExample();
  ~RadioButtonExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Group of 3 radio buttons.
  std::vector<RadioButton*> radio_buttons_;

  // Control button to select radio buttons, and show the status of buttons.
  LabelButton* select_;
  LabelButton* status_;

  DISALLOW_COPY_AND_ASSIGN(RadioButtonExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_RADIO_BUTTON_EXAMPLE_H_
