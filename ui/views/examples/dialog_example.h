// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Combobox;
class DialogDelegate;
class Label;
class LabelButton;
class Textfield;
class View;

namespace examples {

// An example that exercises BubbleDialogDelegateView or DialogDelegateView.
class VIEWS_EXAMPLES_EXPORT DialogExample : public ExampleBase,
                                            public TextfieldController {
 public:
  DialogExample();

  DialogExample(const DialogExample&) = delete;
  DialogExample& operator=(const DialogExample&) = delete;

  ~DialogExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  template <class>
  class Delegate;
  class Bubble;
  class Dialog;

  // Helper methods to setup the configuration Views.
  void StartTextfieldRow(View* parent,
                         Textfield** member,
                         std::u16string label,
                         std::u16string value,
                         Label** created_label = nullptr,
                         bool pad_last_col = false);
  void AddCheckbox(View* parent, Checkbox** member, Label* label);

  // Checkbox callback
  void OnPerformAction();

  // Interrogates the configuration Views for DialogDelegate.
  ui::ModalType GetModalType() const;
  int GetDialogButtons() const;

  void OnCloseCallback();
  // Invoked when the dialog is closing.
  bool AllowDialogClose(bool accept);

  // Resize the dialog Widget to match the preferred size. Triggers Layout().
  void ResizeDialog();

  void ShowButtonPressed();
  void BubbleCheckboxPressed();
  void OtherCheckboxPressed();

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

  raw_ptr<DialogDelegate> last_dialog_ = nullptr;
  raw_ptr<Label> last_body_label_ = nullptr;

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Textfield* title_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Textfield* body_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Textfield* ok_button_label_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Checkbox* has_ok_button_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Textfield* cancel_button_label_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Checkbox* has_cancel_button_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Textfield* extra_button_label_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Checkbox* has_extra_button_;
  raw_ptr<Combobox> mode_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Checkbox* bubble_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION Checkbox* persistent_bubble_;
  raw_ptr<LabelButton> show_;
  ui::SimpleComboboxModel mode_model_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_DIALOG_EXAMPLE_H_
