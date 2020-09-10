// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_

#include "base/macros.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Combobox;
class DialogDelegate;
class GridLayout;
class Label;
class LabelButton;
class Textfield;

namespace examples {

// An example that exercises BubbleDialogDelegateView or DialogDelegateView.
class VIEWS_EXAMPLES_EXPORT DialogExample : public ExampleBase,
                                            public ButtonListener,
                                            public TextfieldController {
 public:
  DialogExample();
  ~DialogExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  template <class>
  class Delegate;
  class Bubble;
  class Dialog;

  // Helper methods to setup the configuration Views.
  void StartRowWithLabel(GridLayout* layout, const char* label);
  void StartTextfieldRow(GridLayout* layout,
                         Textfield** member,
                         const char* label,
                         const char* value);
  void AddCheckbox(GridLayout* layout, Checkbox** member);

  // Checkbox callback
  void OnPerformAction();

  // Interrogates the configuration Views for DialogDelegate.
  ui::ModalType GetModalType() const;
  int GetDialogButtons() const;

  // Invoked when the dialog is closing.
  bool AllowDialogClose(bool accept);

  // Resize the dialog Widget to match the preferred size. Triggers Layout().
  void ResizeDialog();

  // ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override;

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;

  DialogDelegate* last_dialog_ = nullptr;
  Label* last_body_label_ = nullptr;

  Textfield* title_;
  Textfield* body_;
  Textfield* ok_button_label_;
  Checkbox* has_ok_button_;
  Textfield* cancel_button_label_;
  Checkbox* has_cancel_button_;
  Textfield* extra_button_label_;
  Checkbox* has_extra_button_;
  Combobox* mode_;
  Checkbox* bubble_;
  Checkbox* persistent_bubble_;
  LabelButton* show_;
  ui::SimpleComboboxModel mode_model_;

  DISALLOW_COPY_AND_ASSIGN(DialogExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_
