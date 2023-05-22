// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_field.h"

#include <string>

#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace ui {

DialogModelLabel::TextReplacement::TextReplacement(std::u16string text,
                                                   bool is_emphasized)
    : text_(text), is_emphasized_(is_emphasized) {}
DialogModelLabel::TextReplacement::TextReplacement(
    int message_id,
    Callback callback,
    std::u16string accessible_name)
    : text_(l10n_util::GetStringUTF16(message_id)),
      is_emphasized_(false),
      callback_(callback),
      accessible_name_(accessible_name) {
  // Emphasized links are not supported, at least for now.
}
DialogModelLabel::TextReplacement::TextReplacement(const TextReplacement&) =
    default;
DialogModelLabel::TextReplacement::~TextReplacement() = default;

DialogModelLabel::DialogModelLabel(int message_id)
    : message_id_(message_id),
      string_(l10n_util::GetStringUTF16(message_id_)) {}

DialogModelLabel::DialogModelLabel(int message_id,
                                   std::vector<TextReplacement> replacements)
    : message_id_(message_id), replacements_(std::move(replacements)) {
  // Note that this constructor does not set `string_` which is invalid for
  // labels with `replacements_`.
}

DialogModelLabel::DialogModelLabel(std::u16string fixed_string)
    : message_id_(-1), string_(std::move(fixed_string)) {}

const std::u16string& DialogModelLabel::GetString(
    base::PassKey<DialogModelHost>) const {
  DCHECK(replacements_.empty());
  return string_;
}

DialogModelLabel::DialogModelLabel(const DialogModelLabel&) = default;

DialogModelLabel::~DialogModelLabel() = default;

DialogModelLabel DialogModelLabel::CreateWithReplacement(
    int message_id,
    TextReplacement replacement) {
  return CreateWithReplacements(message_id, {std::move(replacement)});
}

DialogModelLabel DialogModelLabel::CreateWithReplacements(
    int message_id,
    std::vector<TextReplacement> replacements) {
  return DialogModelLabel(message_id, std::move(replacements));
}

DialogModelLabel::TextReplacement DialogModelLabel::CreateLink(
    int message_id,
    base::RepeatingClosure closure,
    std::u16string accessible_name) {
  return CreateLink(
      message_id,
      base::BindRepeating([](base::RepeatingClosure closure,
                             const Event& event) { closure.Run(); },
                          std::move(closure)),
      accessible_name);
}

DialogModelLabel::TextReplacement DialogModelLabel::CreateLink(
    int message_id,
    Callback callback,
    std::u16string accessible_name) {
  return TextReplacement(message_id, callback, accessible_name);
}

DialogModelLabel::TextReplacement DialogModelLabel::CreatePlainText(
    std::u16string text) {
  return TextReplacement(text);
}

DialogModelLabel::TextReplacement DialogModelLabel::CreateEmphasizedText(
    std::u16string text) {
  return TextReplacement(text, true);
}

DialogModelField::DialogModelField(base::PassKey<DialogModel>,
                                   DialogModel* model,
                                   Type type,
                                   ElementIdentifier id,
                                   base::flat_set<Accelerator> accelerators)
    : model_(model),
      type_(type),
      id_(id),
      accelerators_(std::move(accelerators)) {}

DialogModelField::~DialogModelField() = default;

DialogModelButton* DialogModelField::AsButton(base::PassKey<DialogModelHost>) {
  return AsButton();
}

DialogModelParagraph* DialogModelField::AsParagraph(
    base::PassKey<DialogModelHost>) {
  return AsParagraph();
}

DialogModelCheckbox* DialogModelField::AsCheckbox(
    base::PassKey<DialogModelHost>) {
  return AsCheckbox();
}

DialogModelCombobox* DialogModelField::AsCombobox(
    base::PassKey<DialogModelHost>) {
  return AsCombobox();
}

DialogModelTextfield* DialogModelField::AsTextfield(
    base::PassKey<DialogModelHost>) {
  return AsTextfield();
}

const DialogModelMenuItem* DialogModelField::AsMenuItem(
    base::PassKey<DialogModelHost>) const {
  return AsMenuItem();
}

DialogModelMenuItem* DialogModelField::AsMenuItem(
    base::PassKey<DialogModelHost>) {
  return const_cast<DialogModelMenuItem*>(AsMenuItem());
}

DialogModelCustomField* DialogModelField::AsCustomField(
    base::PassKey<DialogModelHost>) {
  return AsCustomField();
}

DialogModelButton* DialogModelField::AsButton() {
  DCHECK_EQ(type_, kButton);
  return static_cast<DialogModelButton*>(this);
}

DialogModelParagraph* DialogModelField::AsParagraph() {
  DCHECK_EQ(type_, kParagraph);
  return static_cast<DialogModelParagraph*>(this);
}

DialogModelCheckbox* DialogModelField::AsCheckbox() {
  DCHECK_EQ(type_, kCheckbox);
  return static_cast<DialogModelCheckbox*>(this);
}

DialogModelCombobox* DialogModelField::AsCombobox() {
  DCHECK_EQ(type_, kCombobox);
  return static_cast<DialogModelCombobox*>(this);
}

const DialogModelMenuItem* DialogModelField::AsMenuItem() const {
  DCHECK_EQ(type_, kMenuItem);
  return static_cast<const DialogModelMenuItem*>(this);
}

DialogModelTextfield* DialogModelField::AsTextfield() {
  DCHECK_EQ(type_, kTextfield);
  return static_cast<DialogModelTextfield*>(this);
}

DialogModelCustomField* DialogModelField::AsCustomField() {
  DCHECK_EQ(type_, kCustom);
  return static_cast<DialogModelCustomField*>(this);
}

DialogModelButton::Params::Params() = default;
DialogModelButton::Params::~Params() = default;

DialogModelButton::Params& DialogModelButton::Params::SetId(
    ElementIdentifier id) {
  DCHECK(!id_);
  DCHECK(id);
  id_ = id;
  return *this;
}

DialogModelButton::Params& DialogModelButton::Params::SetLabel(
    std::u16string label) {
  DCHECK(label_.empty());
  DCHECK(!label.empty());
  label_ = label;
  return *this;
}

DialogModelButton::Params& DialogModelButton::Params::SetStyle(
    absl::optional<ButtonStyle> style) {
  DCHECK(style_ != style);
  style_ = style;
  return *this;
}

DialogModelButton::Params& DialogModelButton::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelButton::DialogModelButton(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    base::RepeatingCallback<void(const Event&)> callback,
    const DialogModelButton::Params& params)
    : DialogModelField(pass_key,
                       model,
                       kButton,
                       params.id_,
                       params.accelerators_),
      label_(std::move(params.label_)),
      style_(params.style_),
      callback_(std::move(callback)) {
  DCHECK(callback_);
}

DialogModelButton::~DialogModelButton() = default;

void DialogModelButton::OnPressed(base::PassKey<DialogModelHost>,
                                  const Event& event) {
  callback_.Run(event);
}

DialogModelParagraph::DialogModelParagraph(base::PassKey<DialogModel> pass_key,
                                           DialogModel* model,
                                           const DialogModelLabel& label,
                                           std::u16string header,
                                           ElementIdentifier id)
    : DialogModelField(pass_key, model, kParagraph, id, {}),
      label_(label),
      header_(header) {}

DialogModelParagraph::~DialogModelParagraph() = default;

DialogModelCheckbox::DialogModelCheckbox(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    ElementIdentifier id,
    const DialogModelLabel& label,
    const DialogModelCheckbox::Params& params)
    : DialogModelField(pass_key, model, kCheckbox, id, {}),
      label_(label),
      is_checked_(params.is_checked_) {}

DialogModelCheckbox::~DialogModelCheckbox() = default;

void DialogModelCheckbox::OnChecked(base::PassKey<DialogModelHost>,
                                    bool is_checked) {
  is_checked_ = is_checked;
}

DialogModelCombobox::Params::Params() = default;
DialogModelCombobox::Params::~Params() = default;

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

DialogModelCombobox::DialogModelCombobox(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    ElementIdentifier id,
    std::u16string label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params)
    : DialogModelField(pass_key, model, kCombobox, id, params.accelerators_),
      label_(std::move(label)),
      accessible_name_(params.accessible_name_),
      selected_index_(combobox_model->GetDefaultIndex().value()),
      combobox_model_(std::move(combobox_model)),
      callback_(params.callback_) {}

DialogModelCombobox::~DialogModelCombobox() = default;

void DialogModelCombobox::OnSelectedIndexChanged(base::PassKey<DialogModelHost>,
                                                 size_t selected_index) {
  selected_index_ = selected_index;
}

void DialogModelCombobox::OnPerformAction(base::PassKey<DialogModelHost>) {
  if (callback_)
    callback_.Run();
}

DialogModelMenuItem::Params::Params() = default;
DialogModelMenuItem::Params::~Params() = default;

DialogModelMenuItem::Params& DialogModelMenuItem::Params::SetIsEnabled(
    bool is_enabled) {
  is_enabled_ = is_enabled;
  return *this;
}

DialogModelMenuItem::Params& DialogModelMenuItem::Params::SetId(
    ElementIdentifier id) {
  DCHECK(!id_);
  DCHECK(id);
  id_ = id;
  return *this;
}

DialogModelMenuItem::DialogModelMenuItem(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    ImageModel icon,
    std::u16string label,
    base::RepeatingCallback<void(int)> callback,
    const DialogModelMenuItem::Params& params)
    : DialogModelField(pass_key, model, kMenuItem, params.id_, {}),
      icon_(std::move(icon)),
      label_(std::move(label)),
      is_enabled_(params.is_enabled_),
      callback_(std::move(callback)) {}

DialogModelMenuItem::~DialogModelMenuItem() = default;

void DialogModelMenuItem::OnActivated(base::PassKey<DialogModelHost> pass_key,
                                      int event_flags) {
  DCHECK(callback_);
  callback_.Run(event_flags);
}

DialogModelSeparator::DialogModelSeparator(base::PassKey<DialogModel> pass_key,
                                           DialogModel* model)
    : DialogModelField(pass_key, model, kSeparator, ElementIdentifier(), {}) {}

DialogModelSeparator::~DialogModelSeparator() = default;

DialogModelTextfield::Params::Params() = default;
DialogModelTextfield::Params::~Params() = default;

DialogModelTextfield::Params& DialogModelTextfield::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelTextfield::DialogModelTextfield(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    ElementIdentifier id,
    std::u16string label,
    std::u16string text,
    const ui::DialogModelTextfield::Params& params)
    : DialogModelField(pass_key, model, kTextfield, id, params.accelerators_),
      label_(label),
      accessible_name_(params.accessible_name_),
      text_(std::move(text)) {
  // Textfields need either an accessible name or label or the screenreader will
  // not be able to announce anything sensible.
  DCHECK(!label_.empty() || !accessible_name_.empty());
}

DialogModelTextfield::~DialogModelTextfield() = default;

void DialogModelTextfield::OnTextChanged(base::PassKey<DialogModelHost>,
                                         std::u16string text) {
  text_ = std::move(text);
}

DialogModelCustomField::Field::~Field() = default;

DialogModelCustomField::DialogModelCustomField(
    base::PassKey<DialogModel> pass_key,
    DialogModel* model,
    ElementIdentifier id,
    std::unique_ptr<DialogModelCustomField::Field> field)
    : DialogModelField(pass_key, model, kCustom, id, {}),
      field_(std::move(field)) {}

DialogModelCustomField::~DialogModelCustomField() = default;

}  // namespace ui
