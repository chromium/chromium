// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/examples/example_base.h"

namespace views {

class ImageButton;
class LabelButton;
class MdTextButton;

namespace examples {

// ButtonExample simply counts the number of clicks.
class VIEWS_EXAMPLES_EXPORT ButtonExample : public ExampleBase,
                                            public ButtonListener {
 public:
  ButtonExample();
  ~ButtonExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void LabelButtonPressed(LabelButton* label_button, const ui::Event& event);

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Example buttons.
  LabelButton* label_button_ = nullptr;
  MdTextButton* md_button_ = nullptr;
  MdTextButton* md_disabled_button_ = nullptr;
  MdTextButton* md_default_button_ = nullptr;
  ImageButton* image_button_ = nullptr;

  const gfx::ImageSkia* icon_ = nullptr;

  // The number of times the buttons are pressed.
  int count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ButtonExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_BUTTON_EXAMPLE_H_
