// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/ui_base_types.h"

namespace ui {

DialogModel::Builder::Builder(std::unique_ptr<DialogModelDelegate> delegate)
    : model_(std::make_unique<DialogModel>(base::PassKey<Builder>(),
                                           std::move(delegate))) {}

DialogModel::Builder::Builder() : Builder(nullptr) {}

DialogModel::Builder::~Builder() {
  DCHECK(!model_) << "Model should've been built.";
}

std::unique_ptr<DialogModel> DialogModel::Builder::Build() {
  DCHECK(model_);
  return std::move(model_);
}

DialogModel::Builder& DialogModel::Builder::AddOkButton(
    ButtonCallbackVariant callback,
    const DialogModelButton::Params& params) {
  return AddButtonInternal(std::move(callback), params, model_->ok_button_,
                           model_->accept_action_callback_);
}

DialogModel::Builder& DialogModel::Builder::AddCancelButton(
    ButtonCallbackVariant callback,
    const DialogModelButton::Params& params) {
  return AddButtonInternal(std::move(callback), params, model_->cancel_button_,
                           model_->cancel_action_callback_);
}

DialogModel::Builder& DialogModel::Builder::AddButtonInternal(
    ButtonCallbackVariant callback,
    const DialogModelButton::Params& params,
    absl::optional<ui::DialogModelButton>& model_button,
    ButtonCallbackVariant& model_callback) {
  CHECK(params.is_visible_);
  CHECK(!model_button.has_value());
  absl::visit(
      base::Overloaded{
          [](decltype(base::DoNothing())& callback) {
            // Intentional noop
          },
          [](base::RepeatingCallback<bool()>& callback) { CHECK(callback); },
          [](base::OnceClosure& closure) { CHECK(closure); },
      },
      callback);
  model_callback = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogCanceled() instead.
  model_button.emplace(model_->GetPassKey(), model_.get(),
                       base::BindRepeating([](const Event&) { NOTREACHED(); }),
                       params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraButton(
    base::RepeatingCallback<void(const Event&)> callback,
    const DialogModelButton::Params& params) {
  CHECK(params.is_visible_);
  DCHECK(!model_->extra_button_);
  DCHECK(!model_->extra_link_);
  // Extra buttons are required to have labels.
  DCHECK(!params.label_.empty());
  model_->extra_button_.emplace(model_->GetPassKey(), model_.get(),
                                std::move(callback), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddExtraLink(
    DialogModelLabel::TextReplacement link) {
  DCHECK(!model_->extra_button_);
  DCHECK(!model_->extra_link_);
  model_->extra_link_.emplace(std::move(link));
  return *this;
}

DialogModel::Builder& DialogModel::Builder::OverrideDefaultButton(
    DialogButton button) {
  // This can only be called once.
  DCHECK(!model_->override_default_button_);
  // Confirm the button exists.
  switch (button) {
    case DIALOG_BUTTON_NONE:
      break;
    case DIALOG_BUTTON_OK:
      DCHECK(model_->ok_button_);
      break;
    case DIALOG_BUTTON_CANCEL:
      DCHECK(model_->cancel_button_);
      break;
  }
  model_->override_default_button_ = button;
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetInitiallyFocusedField(
    ElementIdentifier id) {
  // This must be called with a non-null id
  DCHECK(id);
  // This can only be called once.
  DCHECK(!model_->initially_focused_field_);
  model_->initially_focused_field_ = id;
  return *this;
}

DialogModel::DialogModel(base::PassKey<Builder>,
                         std::unique_ptr<DialogModelDelegate> delegate)
    : delegate_(std::move(delegate)) {
  if (delegate_)
    delegate_->set_dialog_model(this);
}

DialogModel::~DialogModel() = default;

void DialogModel::AddParagraph(const DialogModelLabel& label,
                               std::u16string header,
                               ElementIdentifier id) {
  AddField(std::make_unique<DialogModelParagraph>(GetPassKey(), this, label,
                                                  header, id));
}

void DialogModel::AddCheckbox(ElementIdentifier id,
                              const DialogModelLabel& label,
                              const DialogModelCheckbox::Params& params) {
  AddField(std::make_unique<DialogModelCheckbox>(GetPassKey(), this, id, label,
                                                 params));
}

void DialogModel::AddCombobox(ElementIdentifier id,
                              std::u16string label,
                              std::unique_ptr<ui::ComboboxModel> combobox_model,
                              const DialogModelCombobox::Params& params) {
  AddField(std::make_unique<DialogModelCombobox>(
      GetPassKey(), this, id, std::move(label), std::move(combobox_model),
      params));
}

void DialogModel::AddSeparator() {
  AddField(std::make_unique<DialogModelSeparator>(GetPassKey(), this));
}

void DialogModel::AddMenuItem(ImageModel icon,
                              std::u16string label,
                              base::RepeatingCallback<void(int)> callback,
                              const DialogModelMenuItem::Params& params) {
  AddField(std::make_unique<DialogModelMenuItem>(
      GetPassKey(), this, std::move(icon), std::move(label),
      std::move(callback), params));
}

void DialogModel::AddTextfield(ElementIdentifier id,
                               std::u16string label,
                               std::u16string text,
                               const DialogModelTextfield::Params& params) {
  AddField(std::make_unique<DialogModelTextfield>(
      GetPassKey(), this, id, std::move(label), std::move(text), params));
}

void DialogModel::AddCustomField(
    std::unique_ptr<DialogModelCustomField::Field> field,
    ElementIdentifier id) {
  AddField(std::make_unique<DialogModelCustomField>(GetPassKey(), this, id,
                                                    std::move(field)));
}

bool DialogModel::HasField(ElementIdentifier id) const {
  return base::ranges::any_of(fields_,
                              [id](auto& field) { return field->id_ == id; }) ||
         (ok_button_ && ok_button_->id_ == id) ||
         (cancel_button_ && cancel_button_->id_ == id) ||
         (extra_button_ && extra_button_->id_ == id);
}

DialogModelField* DialogModel::GetFieldByUniqueId(ElementIdentifier id) {
  // Assert that there are not duplicate fields corresponding to `id`. There
  // could be no matches in `fields_` if `id` corresponds to a button.
  DCHECK_LE(static_cast<int>(base::ranges::count_if(
                fields_, [id](auto& field) { return field->id_ == id; })),
            1);

  for (auto& field : fields_) {
    if (field->id_ == id)
      return field.get();
  }

  // Buttons are fields, too.
  if (ok_button_ && ok_button_->id_ == id)
    return &ok_button_.value();
  if (cancel_button_ && cancel_button_->id_ == id)
    return &cancel_button_.value();
  if (extra_button_ && cancel_button_->id_ == id)
    return &extra_button_.value();

  NOTREACHED_NORETURN();
}

DialogModelCheckbox* DialogModel::GetCheckboxByUniqueId(ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCheckbox();
}

DialogModelCombobox* DialogModel::GetComboboxByUniqueId(ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCombobox();
}

DialogModelTextfield* DialogModel::GetTextfieldByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsTextfield();
}

DialogModelButton* DialogModel::GetButtonByUniqueId(ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsButton();
}

bool DialogModel::OnDialogAcceptAction(base::PassKey<DialogModelHost>) {
  return RunDialogModelButtonCallback(accept_action_callback_);
}

bool DialogModel::OnDialogCancelAction(base::PassKey<DialogModelHost>) {
  return RunDialogModelButtonCallback(cancel_action_callback_);
}

bool DialogModel::RunDialogModelButtonCallback(
    ButtonCallbackVariant& callback_variant) {
  return absl::visit(
      base::Overloaded{
          [](decltype(base::DoNothing())& callback) { return true; },
          [](base::RepeatingCallback<bool()>& callback) {
            return callback.Run();
          },
          [](base::OnceClosure& callback) {
            CHECK(callback);
            std::move(callback).Run();
            return true;
          },
      },
      callback_variant);
}

void DialogModel::OnDialogCloseAction(base::PassKey<DialogModelHost>) {
  if (close_action_callback_)
    std::move(close_action_callback_).Run();
}

void DialogModel::OnDialogDestroying(base::PassKey<DialogModelHost>) {
  if (dialog_destroying_callback_)
    std::move(dialog_destroying_callback_).Run();
}

void DialogModel::SetVisible(ElementIdentifier id, bool visible) {
  DialogModelField* const field = GetFieldByUniqueId(id);

  CHECK(field);
  field->set_visible(visible);

  if (host_) {
    host_->OnFieldChanged(field);
  }
}

void DialogModel::SetButtonLabel(DialogModelButton* button,
                                 const std::u16string& label) {
  CHECK(button);
  button->label_ = label;

  if (host_) {
    host_->OnFieldChanged(button);
  }
}

void DialogModel::AddField(std::unique_ptr<DialogModelField> field) {
  fields_.push_back(std::move(field));
  if (host_)
    host_->OnFieldAdded(fields_.back().get());
}

}  // namespace ui
