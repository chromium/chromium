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
  NOTREACHED_NORETURN();
}

void DialogModelMenuModelAdapter::OnFieldAdded(DialogModelField* field) {
  NOTREACHED_NORETURN();
}

void DialogModelMenuModelAdapter::OnFieldChanged(DialogModelField* field) {
  NOTREACHED_NORETURN();
}

size_t DialogModelMenuModelAdapter::GetItemCount() const {
  return model_->fields(GetPassKey()).size();
}

MenuModel::ItemType DialogModelMenuModelAdapter::GetTypeAt(size_t index) const {
  return GetField(index)->type(GetPassKey()) == DialogModelField::kSeparator
             ? TYPE_SEPARATOR
             : TYPE_COMMAND;
}

MenuSeparatorType DialogModelMenuModelAdapter::GetSeparatorTypeAt(
    size_t index) const {
  DCHECK_EQ(GetField(index)->type(GetPassKey()), DialogModelField::kSeparator);
  return MenuSeparatorType::NORMAL_SEPARATOR;
}

int DialogModelMenuModelAdapter::GetCommandIdAt(size_t index) const {
  // TODO(pbos): Figure out what this should be. Combobox seems to offset by
  // 1000. Dunno why.
  return static_cast<int>(index + 1234);
}

std::u16string DialogModelMenuModelAdapter::GetLabelAt(size_t index) const {
  return GetField(index)->AsMenuItem(GetPassKey())->label(GetPassKey());
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
  NOTREACHED_NORETURN();
}

ImageModel DialogModelMenuModelAdapter::GetIconAt(size_t index) const {
  return GetField(index)->AsMenuItem(GetPassKey())->icon(GetPassKey());
}

ButtonMenuItemModel* DialogModelMenuModelAdapter::GetButtonMenuItemAt(
    size_t index) const {
  NOTREACHED_NORETURN();
}

bool DialogModelMenuModelAdapter::IsEnabledAt(size_t index) const {
  DCHECK_LT(index, GetItemCount());

  const DialogModelField* const field = GetField(index);
  return field->type(GetPassKey()) != DialogModelField::kSeparator &&
         field->AsMenuItem(GetPassKey())->is_enabled(GetPassKey());
}

ui::ElementIdentifier DialogModelMenuModelAdapter::GetElementIdentifierAt(
    size_t index) const {
  DCHECK_LT(index, GetItemCount());

  const DialogModelField* const field = GetField(index);
  return field->AsMenuItem(GetPassKey())->id(GetPassKey());
}

MenuModel* DialogModelMenuModelAdapter::GetSubmenuModelAt(size_t index) const {
  NOTREACHED_NORETURN();
}

void DialogModelMenuModelAdapter::ActivatedAt(size_t index) {
  // If this flags investigate why the ActivatedAt(index, event_flags) isn't
  // being called.
  NOTREACHED_NORETURN();
}

void DialogModelMenuModelAdapter::ActivatedAt(size_t index, int event_flags) {
  DialogModelMenuItem* menu_item = GetField(index)->AsMenuItem(GetPassKey());
  menu_item->OnActivated(GetPassKey(), event_flags);
}

const DialogModelField* DialogModelMenuModelAdapter::GetField(
    size_t index) const {
  DCHECK_LT(index, GetItemCount());
  return model_->fields(GetPassKey())[index].get();
}

DialogModelField* DialogModelMenuModelAdapter::GetField(size_t index) {
  return const_cast<DialogModelField*>(
      const_cast<const DialogModelMenuModelAdapter*>(this)->GetField(index));
}

}  // namespace ui
