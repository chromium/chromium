// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {
class ProgressBar;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ProgressBarExample : public ExampleBase,
                                                 public ButtonListener {
 public:
  ProgressBarExample();
  ~ProgressBarExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // ButtonListener:
  void ButtonPressed(Button* button, const ui::Event& event) override;

  Button* minus_button_ = nullptr;
  Button* plus_button_ = nullptr;
  ProgressBar* progress_bar_ = nullptr;
  double current_percent_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(ProgressBarExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
