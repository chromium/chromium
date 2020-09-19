// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model.h"

#include "base/bind_helpers.h"
#include "base/ranges/algorithm.h"

namespace ui {

DialogModel::Builder::Builder(std::unique_ptr<DialogModelDelegate> delegate)
    : model_(std::make_unique<DialogModel>(util::PassKey<Builder>(),
                                           std::move(delegate))) {}
DialogModel::Builder::~Builder() {
  DCHECK(!model_) << "Model should've been built.";
}

std::unique_ptr<DialogModel> DialogModel::Builder::Build() {
  DCHECK(model_);
  return std::move(model_);
}

DialogModel::Builder& DialogModel::Builder::AddOkButton(
    base::OnceClosure callback,
    base::string16 label,
    const DialogModelButton::Params& params) {
  DCHECK(!model_->accept_callback_);
  model_->accept_callback_ = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogAccepted() instead.
  model_->ok_button_.emplace(
      model_->GetPassKey(), model_.get(),
      base::BindRepeating([](const Event&) { NOTREACHED(); }), std::move(label),
      params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddCancelButton(
    base::OnceClosure callback,
    base::string16 label,
    const DialogModelButton::Params& params) {
  DCHECK(!model_->cancel_callback_);
  model_->cancel_callback_ = std::move(callback);
  // NOTREACHED() is used below to make sure this callback isn't used.
  // DialogModelHost should be using OnDialogCanceled() instead.
  model_->cancel_button_.emplace(
      model_->GetPassKey(), model_.get(),
      base::BindRepeating([](const Event&) { NOTREACHED(); }), std::move(label),
      params);

  return *this;
}

DialogModel::Builder& DialogModel::Builder::AddDialogExtraButton(
    base::RepeatingCallback<void(const Event&)> callback,
    base::string16 label,
    const DialogModelButton::Params& params) {
  model_->extra_button_.emplace(model_->GetPassKey(), model_.get(),
                                std::move(callback), std::move(label), params);
  return *this;
}

DialogModel::Builder& DialogModel::Builder::SetInitiallyFocusedField(
    int unique_id) {
  // This must be called with unique_id >= 0 (-1 is "no ID").
  DCHECK_GE(unique_id, 0);
  // This can only be called once.
  DCHECK(!model_->initially_focused_field_);
  model_->initially_focused_field_ = unique_id;
  return *this;
}

DialogModel::DialogModel(util::PassKey<Builder>,
                         std::unique_ptr<DialogModelDelegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->set_dialog_model(this);
}

DialogModel::~DialogModel() = default;

void DialogModel::AddBodyText(const DialogModelLabel& label) {
  AddField(std::make_unique<DialogModelBodyText>(GetPassKey(), this, label));
}

void DialogModel::AddCheckbox(int unique_id, const DialogModelLabel& label) {
  AddField(std::make_unique<DialogModelCheckbox>(GetPassKey(), this, unique_id,
                                                 label));
}

void DialogModel::AddCombobox(base::string16 label,
                              std::unique_ptr<ui::ComboboxModel> combobox_model,
                              const DialogModelCombobox::Params& params) {
  AddField(std::make_unique<DialogModelCombobox>(
      GetPassKey(), this, std::move(label), std::move(combobox_model), params));
}

void DialogModel::AddTextfield(base::string16 label,
                               base::string16 text,
                               const DialogModelTextfield::Params& params) {
  AddField(std::make_unique<DialogModelTextfield>(
      GetPassKey(), this, std::move(label), std::move(text), params));
}

bool DialogModel::HasField(int unique_id) const {
  return base::ranges::any_of(fields_, [unique_id](auto& field) {
    return field->unique_id_ == unique_id;
  });
}

DialogModelField* DialogModel::GetFieldByUniqueId(int unique_id) {
  for (auto& field : fields_) {
    if (field->unique_id_ == unique_id)
      return field.get();
  }
  NOTREACHED();
  return nullptr;
}

DialogModelCheckbox* DialogModel::GetCheckboxByUniqueId(int unique_id) {
  return GetFieldByUniqueId(unique_id)->AsCheckbox();
}

DialogModelCombobox* DialogModel::GetComboboxByUniqueId(int unique_id) {
  return GetFieldByUniqueId(unique_id)->AsCombobox();
}

DialogModelTextfield* DialogModel::GetTextfieldByUniqueId(int unique_id) {
  return GetFieldByUniqueId(unique_id)->AsTextfield();
}

void DialogModel::OnDialogAccepted(util::PassKey<DialogModelHost>) {
  if (accept_callback_)
    std::move(accept_callback_).Run();
}

void DialogModel::OnDialogCancelled(util::PassKey<DialogModelHost>) {
  if (cancel_callback_)
    std::move(cancel_callback_).Run();
}

void DialogModel::OnDialogClosed(util::PassKey<DialogModelHost>) {
  if (close_callback_)
    std::move(close_callback_).Run();
}

void DialogModel::OnWindowClosing(util::PassKey<DialogModelHost>) {
  if (window_closing_callback_)
    std::move(window_closing_callback_).Run();
}

void DialogModel::AddField(std::unique_ptr<DialogModelField> field) {
  fields_.push_back(std::move(field));
  if (host_)
    host_->OnFieldAdded(fields_.back().get());
}

}  // namespace ui