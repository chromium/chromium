// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

#include <string>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class LabelButton;

namespace examples {

// TextfieldExample mimics login screen.
class VIEWS_EXAMPLES_EXPORT TextfieldExample : public ExampleBase,
                                               public TextfieldController,
                                               public ButtonListener {
 public:
  TextfieldExample();
  ~TextfieldExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // Textfields for name and password.
  Textfield* name_ = nullptr;
  Textfield* password_ = nullptr;
  Textfield* disabled_ = nullptr;
  Textfield* read_only_ = nullptr;
  Textfield* invalid_ = nullptr;
  Textfield* rtl_ = nullptr;

  // Various buttons to control textfield.
  LabelButton* show_password_ = nullptr;
  LabelButton* set_background_ = nullptr;
  LabelButton* clear_all_ = nullptr;
  LabelButton* append_ = nullptr;
  LabelButton* set_ = nullptr;
  LabelButton* set_style_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
