// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/checked_ptr.h"
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
  CheckedPtr<Textfield> name_ = nullptr;
  CheckedPtr<Textfield> password_ = nullptr;
  CheckedPtr<Textfield> disabled_ = nullptr;
  CheckedPtr<Textfield> read_only_ = nullptr;
  CheckedPtr<Textfield> invalid_ = nullptr;
  CheckedPtr<Textfield> rtl_ = nullptr;

  // Various buttons to control textfield.
  CheckedPtr<LabelButton> show_password_ = nullptr;
  CheckedPtr<LabelButton> set_background_ = nullptr;
  CheckedPtr<LabelButton> clear_all_ = nullptr;
  CheckedPtr<LabelButton> append_ = nullptr;
  CheckedPtr<LabelButton> set_ = nullptr;
  CheckedPtr<LabelButton> set_style_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextfieldExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TEXTFIELD_EXAMPLE_H_
