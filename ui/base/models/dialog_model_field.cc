// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_field.h"

#include <string>
#include <utility>

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

const std::u16string& DialogModelLabel::GetString() const {
  CHECK(replacements_.empty(), base::NotFatalUntil::M123);
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

DialogModelField::DialogModelField(Type type,
                                   ElementIdentifier id,
                                   base::flat_set<Accelerator> accelerators,
                                   const DialogModelField::Params& params)
    : type_(type),
      id_(id),
      accelerators_(std::move(accelerators)),
      is_visible_(params.is_visible_) {}

DialogModelField::~DialogModelField() = default;

base::CallbackListSubscription DialogModelField::AddOnFieldChangedCallback(
    base::RepeatingClosure on_field_changed) {
  return on_field_changed_.Add(std::move(on_field_changed));
}

void DialogModelField::SetVisible(bool visible) {
  is_visible_ = visible;
  NotifyOnFieldChanged();
}

DialogModelParagraph* DialogModelField::AsParagraph() {
  CHECK_EQ(type_, kParagraph, base::NotFatalUntil::M123);
  return static_cast<DialogModelParagraph*>(this);
}

DialogModelCheckbox* DialogModelField::AsCheckbox() {
  CHECK_EQ(type_, kCheckbox, base::NotFatalUntil::M123);
  return static_cast<DialogModelCheckbox*>(this);
}

DialogModelCombobox* DialogModelField::AsCombobox() {
  CHECK_EQ(type_, kCombobox, base::NotFatalUntil::M123);
  return static_cast<DialogModelCombobox*>(this);
}

DialogModelMenuItem* DialogModelField::AsMenuItem() {
  return const_cast<DialogModelMenuItem*>(std::as_const(*this).AsMenuItem());
}

const DialogModelMenuItem* DialogModelField::AsMenuItem() const {
  CHECK_EQ(type_, kMenuItem, base::NotFatalUntil::M123);
  return static_cast<const DialogModelMenuItem*>(this);
}

const DialogModelTitleItem* DialogModelField::AsTitleItem() const {
  CHECK_EQ(type_, kTitleItem, base::NotFatalUntil::M123);
  return static_cast<const DialogModelTitleItem*>(this);
}

DialogModelTextfield* DialogModelField::AsTextfield() {
  CHECK_EQ(type_, kTextfield, base::NotFatalUntil::M123);
  return static_cast<DialogModelTextfield*>(this);
}

DialogModelPasswordField* DialogModelField::AsPasswordField() {
  CHECK_EQ(type_, kPasswordField);
  return static_cast<DialogModelPasswordField*>(this);
}

DialogModelCustomField* DialogModelField::AsCustomField() {
  CHECK_EQ(type_, kCustom, base::NotFatalUntil::M123);
  return static_cast<DialogModelCustomField*>(this);
}

void DialogModelField::NotifyOnFieldChanged() {
  on_field_changed_.Notify();
}

DialogModelParagraph::DialogModelParagraph(const DialogModelLabel& label,
                                           std::u16string header,
                                           ElementIdentifier id)
    : DialogModelField(kParagraph, id, {}, DialogModelField::Params()),
      label_(label),
      header_(header) {}

DialogModelParagraph::~DialogModelParagraph() = default;

DialogModelCheckbox::DialogModelCheckbox(
    ElementIdentifier id,
    const DialogModelLabel& label,
    const DialogModelCheckbox::Params& params)
    : DialogModelField(kCheckbox, id, {}, params),
      label_(label),
      is_checked_(params.is_checked_) {}

DialogModelCheckbox::~DialogModelCheckbox() = default;

void DialogModelCheckbox::OnChecked(base::PassKey<DialogModelFieldHost>,
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
    ElementIdentifier id,
    std::u16string label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params)
    : DialogModelField(kCombobox, id, params.accelerators_, params),
      label_(std::move(label)),
      accessible_name_(params.accessible_name_),
      selected_index_(combobox_model->GetDefaultIndex().value()),
      combobox_model_(std::move(combobox_model)),
      callback_(params.callback_) {}

DialogModelCombobox::~DialogModelCombobox() = default;

void DialogModelCombobox::OnSelectedIndexChanged(
    base::PassKey<DialogModelFieldHost>,
    size_t selected_index) {
  selected_index_ = selected_index;
}

void DialogModelCombobox::OnPerformAction(base::PassKey<DialogModelFieldHost>) {
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
  CHECK(!id_, base::NotFatalUntil::M123);
  CHECK(id, base::NotFatalUntil::M123);
  id_ = id;
  return *this;
}

DialogModelMenuItem::DialogModelMenuItem(
    ImageModel icon,
    std::u16string label,
    base::RepeatingCallback<void(int)> callback,
    const DialogModelMenuItem::Params& params)
    : DialogModelField(kMenuItem, params.id_, {}, params),
      icon_(std::move(icon)),
      label_(std::move(label)),
      is_enabled_(params.is_enabled_),
      callback_(std::move(callback)) {}

DialogModelMenuItem::~DialogModelMenuItem() = default;

void DialogModelMenuItem::OnActivated(base::PassKey<DialogModelFieldHost>,
                                      int event_flags) {
  CHECK(callback_, base::NotFatalUntil::M123);
  callback_.Run(event_flags);
}

DialogModelSeparator::DialogModelSeparator()
    : DialogModelField(kSeparator,
                       ElementIdentifier(),
                       {},
                       DialogModelField::Params()) {}

DialogModelSeparator::~DialogModelSeparator() = default;

DialogModelTitleItem::DialogModelTitleItem(std::u16string label,
                                           ElementIdentifier id)
    : DialogModelField(kTitleItem, id, {}, DialogModelField::Params()),
      label_(std::move(label)) {}

DialogModelTitleItem::~DialogModelTitleItem() = default;

DialogModelTextfield::Params::Params() = default;
DialogModelTextfield::Params::~Params() = default;

DialogModelTextfield::Params& DialogModelTextfield::Params::AddAccelerator(
    Accelerator accelerator) {
  accelerators_.insert(std::move(accelerator));
  return *this;
}

DialogModelTextfield::DialogModelTextfield(
    ElementIdentifier id,
    std::u16string label,
    std::u16string text,
    const ui::DialogModelTextfield::Params& params)
    : DialogModelField(kTextfield, id, params.accelerators_, params),
      label_(label),
      accessible_name_(params.accessible_name_),
      text_(std::move(text)) {
  // Textfields need either an accessible name or label or the screenreader will
  // not be able to announce anything sensible.
  CHECK(!label_.empty() || !accessible_name_.empty(),
        base::NotFatalUntil::M123);
}

DialogModelTextfield::~DialogModelTextfield() = default;

void DialogModelTextfield::OnTextChanged(base::PassKey<DialogModelFieldHost>,
                                         std::u16string text) {
  if (text == text_) {
    return;
  }
  text_ = std::move(text);
  NotifyOnFieldChanged();
}

DialogModelPasswordField::DialogModelPasswordField(
    ElementIdentifier id,
    std::u16string label,
    std::u16string accessible_name,
    std::u16string incorrect_password_text,
    const DialogModelField::Params& params)
    : DialogModelField(kPasswordField, id, /*accelerators=*/{}, params),
      label_(std::move(label)),
      accessible_name_(std::move(accessible_name)),
      incorrect_password_text_(std::move(incorrect_password_text)) {}

DialogModelPasswordField::~DialogModelPasswordField() = default;

void DialogModelPasswordField::Invalidate() {
  on_invalidate_closures_.Notify();
}

void DialogModelPasswordField::OnTextChanged(
    base::PassKey<DialogModelFieldHost>,
    std::u16string text) {
  if (text == text_) {
    return;
  }
  text_ = std::move(text);
  NotifyOnFieldChanged();
}

base::CallbackListSubscription
DialogModelPasswordField::AddOnInvalidateCallback(
    base::PassKey<DialogModelFieldHost>,
    base::RepeatingClosure closure) {
  return on_invalidate_closures_.Add(std::move(closure));
}

DialogModelCustomField::Field::~Field() = default;

DialogModelCustomField::DialogModelCustomField(
    ElementIdentifier id,
    std::unique_ptr<DialogModelCustomField::Field> field)
    : DialogModelField(kCustom, id, {}, DialogModelField::Params()),
      field_(std::move(field)) {}

DialogModelCustomField::~DialogModelCustomField() = default;

DialogModelSection::DialogModelSection()
    : DialogModelField(kSection,
                       ElementIdentifier(),
                       {},
                       DialogModelField::Params()) {}

DialogModelSection::~DialogModelSection() = default;

DialogModelSection::Builder::Builder()
    : section_(std::make_unique<DialogModelSection>()) {}

DialogModelSection::Builder::~Builder() {
  CHECK(!section_) << "DialogModelSection should've been built.";
}

std::unique_ptr<DialogModelSection> DialogModelSection::Builder::Build() {
  CHECK(section_);
  return std::move(section_);
}

base::CallbackListSubscription DialogModelSection::AddOnFieldAddedCallback(
    base::RepeatingCallback<void(DialogModelField*)> on_field_added) {
  return on_field_added_.Add(std::move(on_field_added));
}

base::CallbackListSubscription DialogModelSection::AddOnFieldChangedCallback(
    base::RepeatingCallback<void(DialogModelField*)> on_field_changed) {
  return on_field_changed_.Add(std::move(on_field_changed));
}

DialogModelField* DialogModelSection::GetFieldByUniqueId(ElementIdentifier id) {
  // Assert that there are not duplicate fields corresponding to `id`. There
  // could be no matches in `fields_` if `id` corresponds to a button.
  CHECK_EQ(static_cast<int>(base::ranges::count_if(
               fields_,
               [id](auto& field) {
                 // TODO(pbos): This does not
                 // work recursively yet.
                 CHECK_NE(field->type(), DialogModelField::kSection);
                 return field->id() == id;
               })),
           1);

  for (auto& field : fields_) {
    if (field->id() == id) {
      return field.get();
    }
  }

  NOTREACHED();
}

DialogModelCheckbox* DialogModelSection::GetCheckboxByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCheckbox();
}

DialogModelCombobox* DialogModelSection::GetComboboxByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsCombobox();
}

DialogModelTextfield* DialogModelSection::GetTextfieldByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsTextfield();
}

DialogModelPasswordField* DialogModelSection::GetPasswordFieldByUniqueId(
    ElementIdentifier id) {
  return GetFieldByUniqueId(id)->AsPasswordField();
}

void DialogModelSection::AddParagraph(const DialogModelLabel& label,
                                      std::u16string header,
                                      ElementIdentifier id) {
  AddField(std::make_unique<DialogModelParagraph>(label, header, id));
}

void DialogModelSection::AddCheckbox(
    ElementIdentifier id,
    const DialogModelLabel& label,
    const DialogModelCheckbox::Params& params) {
  AddField(std::make_unique<DialogModelCheckbox>(id, label, params));
}

void DialogModelSection::AddCombobox(
    ElementIdentifier id,
    std::u16string label,
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const DialogModelCombobox::Params& params) {
  AddField(std::make_unique<DialogModelCombobox>(
      id, std::move(label), std::move(combobox_model), params));
}

void DialogModelSection::AddSeparator() {
  AddField(std::make_unique<DialogModelSeparator>());
}

void DialogModelSection::AddMenuItem(
    ImageModel icon,
    std::u16string label,
    base::RepeatingCallback<void(int)> callback,
    const DialogModelMenuItem::Params& params) {
  AddField(std::make_unique<DialogModelMenuItem>(
      std::move(icon), std::move(label), std::move(callback), params));
}

void DialogModelSection::AddTitleItem(std::u16string label,
                                      ElementIdentifier id) {
  AddField(std::make_unique<DialogModelTitleItem>(std::move(label), id));
}

void DialogModelSection::AddTextfield(
    ElementIdentifier id,
    std::u16string label,
    std::u16string text,
    const DialogModelTextfield::Params& params) {
  AddField(std::make_unique<DialogModelTextfield>(id, std::move(label),
                                                  std::move(text), params));
}

void DialogModelSection::AddPasswordField(
    ElementIdentifier id,
    std::u16string label,
    std::u16string accessible_text,
    std::u16string incorrect_password_text,
    const DialogModelField::Params& params) {
  AddField(std::make_unique<DialogModelPasswordField>(
      id, std::move(label), std::move(accessible_text),
      std::move(incorrect_password_text), params));
}

void DialogModelSection::AddCustomField(
    std::unique_ptr<DialogModelCustomField::Field> field,
    ElementIdentifier id) {
  AddField(std::make_unique<DialogModelCustomField>(id, std::move(field)));
}

void DialogModelSection::AddField(std::unique_ptr<DialogModelField> field) {
  CHECK(field);

  // This probably needs to be updated for recursive fields. CHECK that we don't
  // add recursive sections until we've thought through how the updates are
  // communicated.
  CHECK_NE(field->type(), DialogModelField::kSection);
  auto* const field_ptr = field.get();
  field_subscriptions_.push_back(field->AddOnFieldChangedCallback(
      base::BindRepeating(&DialogModelSection::OnFieldChanged,
                          base::Unretained(this), field_ptr)));
  fields_.push_back(std::move(field));
  on_field_added_.Notify(field_ptr);
}

void DialogModelSection::OnFieldChanged(DialogModelField* field) {
  on_field_changed_.Notify(field);
}

}  // namespace ui
