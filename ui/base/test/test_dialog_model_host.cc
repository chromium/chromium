// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/test_dialog_model_host.h"

#include "ui/base/models/dialog_model.h"

namespace ui {

namespace {

DialogModel::Button* GetButton(DialogModel* dialog_model,
                               TestDialogModelHost::ButtonId button_id,
                               base::PassKey<DialogModelHost> pass_key) {
  switch (button_id) {
    case TestDialogModelHost::ButtonId::kCancel:
      return dialog_model->cancel_button(pass_key);
    case TestDialogModelHost::ButtonId::kExtra:
      return dialog_model->extra_button(pass_key);
    case TestDialogModelHost::ButtonId::kOk:
      return dialog_model->ok_button(pass_key);
  }
}

}  // namespace

TestDialogModelHost::TestDialogModelHost(
    std::unique_ptr<DialogModel> dialog_model)
    : dialog_model_(std::move(dialog_model)) {}

TestDialogModelHost::~TestDialogModelHost() = default;

// These are static methods rather than a method on the host because this needs
// to result with the destruction of the host.
void TestDialogModelHost::Accept(std::unique_ptr<TestDialogModelHost> host) {
  host->dialog_model_->OnDialogAcceptAction(DialogModelHost::GetPassKey());
  DestroyWithoutAction(std::move(host));
}

void TestDialogModelHost::Cancel(std::unique_ptr<TestDialogModelHost> host) {
  host->dialog_model_->OnDialogCancelAction(DialogModelHost::GetPassKey());
  DestroyWithoutAction(std::move(host));
}

void TestDialogModelHost::Close(std::unique_ptr<TestDialogModelHost> host) {
  host->dialog_model_->OnDialogCloseAction(DialogModelHost::GetPassKey());
  DestroyWithoutAction(std::move(host));
}

void TestDialogModelHost::DestroyWithoutAction(
    std::unique_ptr<TestDialogModelHost> host) {
  host->dialog_model_->OnDialogDestroying(DialogModelHost::GetPassKey());
  // Note that `host` destroys when going out of scope here.
}

void TestDialogModelHost::TriggerExtraButton(const ui::Event& event) {
  dialog_model_->extra_button(DialogModelHost::GetPassKey())
      ->OnPressed(DialogModelHost::GetPassKey(), event);
}

DialogModelTextfield* TestDialogModelHost::FindSingleTextfield() {
  // TODO(pbos): Consider validating how "single" this field is.
  for (const auto& field :
       dialog_model_->fields(DialogModelHost::GetPassKey())) {
    if (field->type() == ui::DialogModelField::kTextfield) {
      return field->AsTextfield();
    }
  }
  NOTREACHED();
}

void TestDialogModelHost::SetSingleTextfieldText(std::u16string text) {
  FindSingleTextfield()->OnTextChanged(DialogModelFieldHost::GetPassKey(),
                                       std::move(text));
}

// Bypasses PassKey() requirement for accessing accelerators().
const base::flat_set<Accelerator>& TestDialogModelHost::GetAccelerators(
    ButtonId button_id) {
  return GetButton(dialog_model_.get(), button_id,
                   DialogModelHost::GetPassKey())
      ->accelerators();
}

const std::u16string& TestDialogModelHost::GetLabel(ButtonId button_id) {
  return GetButton(dialog_model_.get(), button_id,
                   DialogModelHost::GetPassKey())
      ->label();
}

ElementIdentifier TestDialogModelHost::GetId(ButtonId button_id) {
  return GetButton(dialog_model_.get(), button_id,
                   DialogModelHost::GetPassKey())
      ->id();
}

ElementIdentifier TestDialogModelHost::GetInitiallyFocusedField() {
  return dialog_model_->initially_focused_field(DialogModelHost::GetPassKey());
}

void TestDialogModelHost::Close() {
  // For now, TestDialogModelHost::Close() is the expected interface to close.
  NOTREACHED();
}

void TestDialogModelHost::OnDialogButtonChanged() {
  NOTREACHED();
}

}  // namespace ui
