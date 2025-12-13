// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/dialog_model_example.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace views::examples {

namespace {

// Identifiers for the fields in the dialog model. This is used to retrieve the
// field from the dialog model in response to user actions.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNameTextfield);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPasswordField);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kRememberMeCheckbox);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFruitCombobox);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomField);

// A combobox model that provides a list of fruits.
// Used by `DialogModel::Builder::AddCombobox()`.
class FruitsComboboxModel : public ui::ComboboxModel {
 public:
  FruitsComboboxModel() {
    fruits_.push_back(u"Apple");
    fruits_.push_back(u"Banana");
    fruits_.push_back(u"Orange");
  }

  FruitsComboboxModel(const FruitsComboboxModel&) = delete;
  FruitsComboboxModel& operator=(const FruitsComboboxModel&) = delete;

  ~FruitsComboboxModel() override = default;

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return fruits_.size(); }
  std::u16string GetItemAt(size_t index) const override {
    return fruits_[index];
  }

 private:
  std::vector<std::u16string> fruits_;
};

// This delegate is owned by the dialog model. It provides access to the dialog
// model via the `dialog_model()` method. The client uses this to read the
// values of the fields.
class DialogModelExampleDelegate : public ui::DialogModelDelegate {
 public:
  DialogModelExampleDelegate() = default;
  DialogModelExampleDelegate(const DialogModelExampleDelegate&) = delete;
  DialogModelExampleDelegate& operator=(const DialogModelExampleDelegate&) =
      delete;
  ~DialogModelExampleDelegate() override = default;

  void set_custom_checkbox(Checkbox* checkbox) { custom_checkbox_ = checkbox; }

  // This is called when the "OK" button is pressed.
  // This is registered via `DialogModel::Builder::AddOkButton()`.
  void OnDialogAccepted() {
    std::u16string output = base::StrCat(
        {u"Hello ",
         dialog_model()->GetTextfieldByUniqueId(kNameTextfield)->text(),
         u", your password is ",
         dialog_model()->GetPasswordFieldByUniqueId(kPasswordField)->text(),
         dialog_model()
                 ->GetCheckboxByUniqueId(kRememberMeCheckbox)
                 ->is_checked()
             ? u". You want to be remembered."
             : u".",
         u"Your favorite fruit is ",
         dialog_model()
             ->GetComboboxByUniqueId(kFruitCombobox)
             ->combobox_model()
             ->GetItemAt(dialog_model()
                             ->GetComboboxByUniqueId(kFruitCombobox)
                             ->selected_index()),
         custom_checkbox_ && custom_checkbox_->GetChecked()
             ? u". Custom checkbox is checked!"
             : u"."});

    // Print the status to the bottom of the dialog.
    PrintStatus(base::UTF16ToUTF8(output));
  }

  // This is called when the "Cancel" button is pressed, or when the dialog is
  // closed without pressing a button.
  // This is registered via `DialogModel::Builder::AddCancelButton()`.
  void OnDialogCancelled() { PrintStatus("Dialog cancelled."); }

  // This is called when the "Extra Button" is pressed.
  // This is registered via `DialogModel::Builder::AddExtraButton()`.
  void OnExtraButtonPressed(const ui::Event& event) {
    PrintStatus("Extra button pressed.");
  }

 private:
  raw_ptr<Checkbox> custom_checkbox_ = nullptr;
};

}  // namespace

DialogModelExample::DialogModelExample() : ExampleBase("Dialog Model") {}

DialogModelExample::~DialogModelExample() = default;

void DialogModelExample::CreateExampleView(View* container) {
  container->SetUseDefaultFillLayout(true);

  // Add a "Show Dialog" button that invokes `ShowDialog()` when clicked.
  auto view = Builder<BoxLayoutView>()
                  .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kStart)
                  .AddChildren(Builder<MdTextButton>()
                                   .CopyAddressTo(&show_dialog_button_)
                                   .SetText(u"Show Dialog")
                                   .SetCallback(base::BindRepeating(
                                       &DialogModelExample::ShowDialog,
                                       base::Unretained(this))))
                  .Build();

  container->AddChildView(std::move(view));
}

void DialogModelExample::ShowDialog() {
  // Create a client-supplied model delegate. This is owned by the dialog model.
  // The delegate's methods are registered as button callbacks via the dialog
  // model builder.
  auto model_delegate = std::make_unique<DialogModelExampleDelegate>();
  auto* model_delegate_ptr = model_delegate.get();

  // Create a custom view with a checkbox. This view is later added to the
  // dialog as a custom field via `DialogModel::Builder::AddCustomField()`.
  auto custom_view_builder =
      Builder<BoxLayoutView>()
          .SetOrientation(BoxLayout::Orientation::kHorizontal)
          .AddChildren(Builder<Label>().SetText(u"This is a custom field!"));
  Checkbox* custom_checkbox = nullptr;
  auto custom_view = std::move(custom_view_builder)
                         .AddChild(Builder<Checkbox>()
                                       .SetText(u"Check me")
                                       .CopyAddressTo(&custom_checkbox))
                         .Build();
  model_delegate_ptr->set_custom_checkbox(custom_checkbox);

  auto dialog_model =
      ui::DialogModel::Builder(std::move(model_delegate))
          .SetTitle(u"Hello, world!")
          .AddParagraph(ui::DialogModelLabel(u"This is a paragraph."))
          .AddTextfield(kNameTextfield, u"Name", u"")
          .AddPasswordField(kPasswordField, u"Password", u"Password", u"")
          .AddCheckbox(kRememberMeCheckbox,
                       ui::DialogModelLabel(u"Remember me"))
          .AddSeparator()
          .AddParagraph(ui::DialogModelLabel(u"This is another paragraph."))
          .AddCombobox(kFruitCombobox, u"Favorite Fruit",
                       std::make_unique<FruitsComboboxModel>())
          .AddCustomField(std::make_unique<BubbleDialogModelHost::CustomView>(
                              std::move(custom_view),
                              BubbleDialogModelHost::FieldType::kText),
                          kCustomField)
          .AddOkButton(
              base::BindOnce(&DialogModelExampleDelegate::OnDialogAccepted,
                             base::Unretained(model_delegate_ptr)))
          .AddCancelButton(
              base::BindOnce(&DialogModelExampleDelegate::OnDialogCancelled,
                             base::Unretained(model_delegate_ptr)))
          .AddExtraButton(
              base::BindRepeating(
                  &DialogModelExampleDelegate::OnExtraButtonPressed,
                  base::Unretained(model_delegate_ptr)),
              ui::DialogModel::Button::Params().SetLabel(u"Extra Button"))
          .SetFootnote(ui::DialogModelLabel(u"This is a footnote."))
          .Build();

  // Creates a dialog host that anchors the dialog to the "Show Dialog" button.
  auto bubble = std::make_unique<BubbleDialogModelHost>(
      std::move(dialog_model), show_dialog_button_, BubbleBorder::TOP_LEFT);

  // Creates and shows the dialog.
  BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();
}

}  // namespace views::examples
