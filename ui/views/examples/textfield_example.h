// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class LabelButton;

namespace examples {

// TextfieldExample mimics login screen.
class VIEWS_EXAMPLES_EXPORT TextfieldExample : public ExampleBase,
                                               public TextfieldController {
 public:
  TextfieldExample();

  TextfieldExample(const TextfieldExample&) = delete;
  TextfieldExample& operator=(const TextfieldExample&) = delete;

  ~TextfieldExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // TextfieldController:
  bool HandleKeyEvent(Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  void ClearAllButtonPressed();
  void AppendButtonPressed();
  void SetButtonPressed();
  void SetStyleButtonPressed();

  // Textfields for name and password.
  raw_ptr<Textfield> name_ = nullptr;
  raw_ptr<Textfield> password_ = nullptr;
  raw_ptr<Textfield> disabled_ = nullptr;
  raw_ptr<Textfield> read_only_ = nullptr;
  raw_ptr<Textfield> invalid_ = nullptr;
  raw_ptr<Textfield> rtl_ = nullptr;

  // Various buttons to control textfield.
  raw_ptr<LabelButton> show_password_ = nullptr;
  raw_ptr<LabelButton> set_background_ = nullptr;
  raw_ptr<LabelButton> clear_all_ = nullptr;
  raw_ptr<LabelButton> append_ = nullptr;
  raw_ptr<LabelButton> set_ = nullptr;
  raw_ptr<LabelButton> set_style_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
