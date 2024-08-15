// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/dialog_model_menu_model_adapter.h"

#include "ui/base/models/dialog_model.h"

namespace ui {

DialogModelMenuModelAdapter::DialogModelMenuModelAdapter(
    std::unique_ptr<DialogModel> model)
    : model_(std::move(model)) {}

DialogModelMenuModelAdapter::~DialogModelMenuModelAdapter() = default;

void DialogModelMenuModelAdapter::Close() {
  // TODO(pbos): Implement, or document why menus can't be closed through this
  // interface.
  NOTREACHED();
}

// TODO(pbos): This should probably not be hosting a DialogModel but rather
// another model with DialogModelSection(s).
void DialogModelMenuModelAdapter::OnDialogButtonChanged() {
  NOTREACHED();
}

base::WeakPtr<ui::MenuModel> DialogModelMenuModelAdapter::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t DialogModelMenuModelAdapter::GetItemCount() const {
  return model_->fields(DialogModelHost::GetPassKey()).size();
}

MenuModel::ItemType DialogModelMenuModelAdapter::GetTypeAt(size_t index) const {
  const auto type = GetField(index)->type();
  if (type == DialogModelField::kTitleItem) {
    return TYPE_TITLE;
  }
  return type == DialogModelField::kSeparator ? TYPE_SEPARATOR : TYPE_COMMAND;
}

MenuSeparatorType DialogModelMenuModelAdapter::GetSeparatorTypeAt(
    size_t index) const {
  CHECK_EQ(GetField(index)->type(), DialogModelField::kSeparator,
           base::NotFatalUntil::M123);
  return MenuSeparatorType::NORMAL_SEPARATOR;
}

int DialogModelMenuModelAdapter::GetCommandIdAt(size_t index) const {
  const auto type = GetField(index)->type();
  if (type == DialogModelField::kTitleItem) {
    return ui::MenuModel::kTitleId;
  }
  // TODO(pbos): Figure out what this should be. Combobox seems to offset by
  // 1000. Dunno why.
  return static_cast<int>(index + 1234);
}

std::u16string DialogModelMenuModelAdapter::GetLabelAt(size_t index) const {
  const DialogModelField* const field = GetField(index);
  if (field->type() == DialogModelField::kTitleItem) {
    return field->AsTitleItem()->label();
  }
  return field->AsMenuItem()->label();
}

bool DialogModelMenuModelAdapter::IsItemDynamicAt(size_t index) const {
  return false;
}

bool DialogModelMenuModelAdapter::GetAcceleratorAt(
    size_t index,
    ui::Accelerator* accelerator) const {
  // TODO(pbos): Add support for accelerators.
  return false;
}

bool DialogModelMenuModelAdapter::IsItemCheckedAt(size_t index) const {
  // TODO(pbos): Add support for checkbox items.
  return false;
}

int DialogModelMenuModelAdapter::GetGroupIdAt(size_t index) const {
  NOTREACHED();
}

ImageModel DialogModelMenuModelAdapter::GetIconAt(size_t index) const {
  const DialogModelField* const field = GetField(index);
  if (field->type() == DialogModelField::kTitleItem) {
    return ImageModel();
  }
  return field->AsMenuItem()->icon();
}

ButtonMenuItemModel* DialogModelMenuModelAdapter::GetButtonMenuItemAt(
    size_t index) const {
  NOTREACHED();
}

bool DialogModelMenuModelAdapter::IsEnabledAt(size_t index) const {
  CHECK_LT(index, GetItemCount(), base::NotFatalUntil::M123);

  const DialogModelField* const field = GetField(index);
  // Non-interactive title should be disabled.
  if (field->type() == DialogModelField::kTitleItem) {
    return false;
  }
  return field->type() != DialogModelField::kSeparator &&
         field->AsMenuItem()->is_enabled();
}

ui::ElementIdentifier DialogModelMenuModelAdapter::GetElementIdentifierAt(
    size_t index) const {
  CHECK_LT(index, GetItemCount(), base::NotFatalUntil::M123);

  const DialogModelField* const field = GetField(index);
  if (field->type() == DialogModelField::kTitleItem) {
    return field->id();
  }
  return field->AsMenuItem()->id();
}

MenuModel* DialogModelMenuModelAdapter::GetSubmenuModelAt(size_t index) const {
  NOTREACHED();
}

void DialogModelMenuModelAdapter::ActivatedAt(size_t index) {
  // If this flags investigate why the ActivatedAt(index, event_flags) isn't
  // being called.
  NOTREACHED();
}

void DialogModelMenuModelAdapter::ActivatedAt(size_t index, int event_flags) {
  DialogModelMenuItem* menu_item = GetField(index)->AsMenuItem();
  menu_item->OnActivated(DialogModelFieldHost::GetPassKey(), event_flags);
}

const DialogModelField* DialogModelMenuModelAdapter::GetField(
    size_t index) const {
  CHECK_LT(index, GetItemCount(), base::NotFatalUntil::M123);
  return model_->fields(DialogModelHost::GetPassKey())[index].get();
}

DialogModelField* DialogModelMenuModelAdapter::GetField(size_t index) {
  return const_cast<DialogModelField*>(
      const_cast<const DialogModelMenuModelAdapter*>(this)->GetField(index));
}

}  // namespace ui
