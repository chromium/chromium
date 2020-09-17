// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_field.h"
#include "base/bind.h"
#include "ui/base/models/dialog_model.h"

namespace ui {

DialogModelLabel::Link::Link(int message_id, Callback callback)
    : message_id(message_id), callback(std::move(callback)) {}
DialogModelLabel::Link::Link(int message_id, base::RepeatingClosure closure)
    : Link(message_id,
           base::BindRepeating([](base::RepeatingClosure closure,
                                  const Event& event) { closure.Run(); },
                               std::move(closure))) {}
DialogModelLabel::Link::Link(const Link&) = default;
DialogModelLabel::Link::~Link() = default;

DialogModelLabel::DialogModelLabel(int message_id) : message_id_(message_id) {}
DialogModelLabel::DialogModelLabel(int message_id, std::vector<Link> links)
    : message_id_(message_id), links_(std::move(links)) {}

DialogModelLabel::DialogModelLabel(const DialogModelLabel&) = default;

DialogModelLabel::~DialogModelLabel() = default;

DialogModelLabel DialogModelLabel::CreateWithLink(int message_id, Link link) {
  return CreateWithLinks(message_id, {link});
}

DialogModelLabel DialogModelLabel::CreateWithLinks(int message_id,
                                                   std::vector<Link> links) {
  return DialogModelLabel(message_id, std::move(links));
}

DialogModelField::DialogModelField(util::PassKey<DialogModel>,
                                   DialogModel* model,
                                   Type type,
                                   int unique_id,
                                   base::flat_set<Accelerator> accelerators)
    : model_(model),
      type_(type),
      unique_id_(unique_id),
      accelerators_(std::move(accelerators)) {
  // TODO(pbos): Assert that unique_id_ is unique.
}

DialogModelField::~DialogModelField() = default;

DialogModelButton* DialogModelField::AsButton(util::PassKey<DialogModelHost>) {
  return AsButton();
}

DialogModelBodyText* DialogModelField::AsBodyText(
    util::PassKey<DialogModelHost>) {
  return AsBodyText();
}

DialogModelCheckbox* DialogModelField::AsCheckbox(
    util::PassKey<DialogModelHost>) {
  return AsCheckbox();
}

DialogModelCombobox* DialogModelField::AsCombobox(
    util::PassKey<DialogModelHost>) {
  return AsCombobox();
}

DialogModelTextfield* DialogModelField::AsTextfield(
    util::PassKey<DialogModelHost>) {
  return AsTextfield();
}

DialogModelButton* DialogModelField::AsButton() {
  DCHECK_EQ(type_, kButton);
  return static_cast<DialogModelButton*>(this);
}

DialogModelBodyText* DialogModelField::AsBodyText() {
  DCHECK_EQ(type_, kBodyText);
  return static_cast<DialogModelBodyText*>(this);
}

DialogModelCheckbox* DialogModelField::AsCheckbox() {
  DCHECK_EQ(type_, kCheckbox);
  return static_cast<DialogModelCheckbox*>(this);
}

DialogModelCombobox* DialogModelField::AsCombobox() {
  DCHECK_EQ(type_, kCombobox);
  return static_cast<DialogModelCombobox*>(this);
}

DialogModelTextfield* DialogModelField::AsTextfield() {
  DCHECK_EQ(type_, kTextfield);
  return static_cast<DialogModelTextfield*>(this);
}

DialogModelButton::Params::Params() = default;
DialogModelButton::Params::~Params() = default;

DialogModelButton::Params& DialogModelButton::Params::SetUniqueId(
    int unique_id) {
  DCHECK_GE(unique_id, 0);
  unique_id_ = unique_id;
  return *this;
}

DialogModelButton::Params& DialogModelButton::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelButton::DialogModelButton(
    util::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::RepeatingCallback<void(const Event&)> callback,
    base::string16 label,
    const DialogModelButton::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kButton,
                       params.unique_id_,
                       params.accelerators_),
      label_(std::move(label)),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}

DialogModelButton::~DialogModelButton() = default;

void DialogModelButton::OnPressed(util::PassKey<DialogModelHost>,
                                  const Event& event) {
  callback_.Run(event);
}

DialogModelBodyText::DialogModelBodyText(util::PassKey<DialogModel> pass_key,
                                         DialogModel* model,
                                         const DialogModelLabel& label)
    : DialogModelField(pass_key,
                       model,
                       kBodyText,
                       -1,
                       base::flat_set<Accelerator>()),
      label_(label) {}

DialogModelBodyText::~DialogModelBodyText() = default;

DialogModelCheckbox::DialogModelCheckbox(util::PassKey<DialogModel> pass_key,
                                         DialogModel* model,
                                         int unique_id,
                                         const DialogModelLabel& label)
    : DialogModelField(pass_key,
                       model,
                       kCheckbox,
                       unique_id,
                       base::flat_set<Accelerator>()),
      label_(label) {}

DialogModelCheckbox::~DialogModelCheckbox() = default;

void DialogModelCheckbox::OnChecked(util::PassKey<DialogModelHost>,
                                    bool is_checked) {
  is_checked_ = is_checked;
}

DialogModelCombobox::Params::Params() = default;
DialogModelCombobox::Params::~Params() = default;

DialogModelCombobox::Params& DialogModelCombobox::Params::SetUniqueId(
    int unique_id) {
  DCHECK_GE(unique_id, 0);
  unique_id_ = unique_id;
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::SetCallback(
    base::RepeatingClosure callback) {
  callback_ = std::move(callback);
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelCombobox::Params& DialogModelCombobox::Params::SetAccessibleName(
    base::string16 accessible_name) {
  accessible_name_ = std::move(accessible_name);
  return *this;
}

DialogModelCombobox::DialogModelCombobox(
    util::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::string16 label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kCombobox,
                       params.unique_id_,
                       params.accelerators_),
      label_(std::move(label)),
      accessible_name_(params.accessible_name_),
      selected_index_(combobox_model->GetDefaultIndex()),
      combobox_model_(std::move(combobox_model)),
      callback_(params.callback_) {}

DialogModelCombobox::~DialogModelCombobox() = default;

void DialogModelCombobox::OnSelectedIndexChanged(util::PassKey<DialogModelHost>,
                                                 int selected_index) {
  selected_index_ = selected_index;
}

void DialogModelCombobox::OnPerformAction(util::PassKey<DialogModelHost>) {
  if (callback_)
    callback_.Run();
}

DialogModelTextfield::Params::Params() = default;
DialogModelTextfield::Params::~Params() = default;

DialogModelTextfield::Params& DialogModelTextfield::Params::SetUniqueId(
    int unique_id) {
  DCHECK_GE(unique_id, 0);
  unique_id_ = unique_id;
  return *this;
}

DialogModelTextfield::Params& DialogModelTextfield::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelTextfield::Params& DialogModelTextfield::Params::SetAccessibleName(
    base::string16 accessible_name) {
  accessible_name_ = accessible_name;
  return *this;
}

DialogModelTextfield::DialogModelTextfield(
    util::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::string16 label,
    base::string16 text,
    const ui::DialogModelTextfield::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kTextfield,
                       params.unique_id_,
                       params.accelerators_),
      label_(label),
      accessible_name_(params.accessible_name_),
      text_(std::move(text)) {}

DialogModelTextfield::~DialogModelTextfield() = default;

void DialogModelTextfield::OnTextChanged(util::PassKey<DialogModelHost>,
                                         base::string16 text) {
  text_ = std::move(text);
}

}  // namespace ui