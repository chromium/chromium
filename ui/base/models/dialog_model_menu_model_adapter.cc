// Copyright 2021 The Chromium Authors. All rights reserved.
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

void DialogModelMenuModelAdapter::OnFieldAdded(DialogModelField* field) {
  NOTREACHED();
}

bool DialogModelMenuModelAdapter::HasIcons() const {
  const auto& fields = model_->fields(GetPassKey());
  for (const auto& field : fields) {
    if (field->type(GetPassKey()) != DialogModelField::kMenuItem)
      continue;
    if (!field->AsMenuItem(GetPassKey())->icon(GetPassKey()).IsEmpty())
      return true;
  }

  return false;
}

int DialogModelMenuModelAdapter::GetItemCount() const {
  return static_cast<int>(model_->fields(GetPassKey()).size());
}

MenuModel::ItemType DialogModelMenuModelAdapter::GetTypeAt(int index) const {
  return GetField(index)->type(GetPassKey()) == DialogModelField::kSeparator
             ? TYPE_SEPARATOR
             : TYPE_COMMAND;
}

MenuSeparatorType DialogModelMenuModelAdapter::GetSeparatorTypeAt(
    int index) const {
  NOTREACHED();
  return MenuSeparatorType::NORMAL_SEPARATOR;
}

int DialogModelMenuModelAdapter::GetCommandIdAt(int index) const {
  // TODO(pbos): Figure out what this should be. Combobox seems to offset by
  // 1000. Dunno why.
  return index + 1234;
}

std::u16string DialogModelMenuModelAdapter::GetLabelAt(int index) const {
  return GetField(index)->AsMenuItem(GetPassKey())->label(GetPassKey());
}

bool DialogModelMenuModelAdapter::IsItemDynamicAt(int index) const {
  return false;
}

bool DialogModelMenuModelAdapter::GetAcceleratorAt(
    int index,
    ui::Accelerator* accelerator) const {
  // TODO(pbos): Add support for accelerators.
  return false;
}

bool DialogModelMenuModelAdapter::IsItemCheckedAt(int index) const {
  // TODO(pbos): Add support for checkbox items.
  return false;
}

int DialogModelMenuModelAdapter::GetGroupIdAt(int index) const {
  NOTREACHED();
  return -1;
}

ImageModel DialogModelMenuModelAdapter::GetIconAt(int index) const {
  return GetField(index)->AsMenuItem(GetPassKey())->icon(GetPassKey());
}

ButtonMenuItemModel* DialogModelMenuModelAdapter::GetButtonMenuItemAt(
    int index) const {
  NOTREACHED();
  return nullptr;
}

bool DialogModelMenuModelAdapter::IsEnabledAt(int index) const {
  DCHECK_LT(index, GetItemCount());

  const DialogModelField* const field = GetField(index);
  if (field->type(GetPassKey()) == DialogModelField::kSeparator)
    return false;

  return field->AsMenuItem(GetPassKey())->is_enabled(GetPassKey());
}

MenuModel* DialogModelMenuModelAdapter::GetSubmenuModelAt(int index) const {
  NOTREACHED();
  return nullptr;
}

void DialogModelMenuModelAdapter::ActivatedAt(int index) {
  // If this flags investigate why the ActivatedAt(index, event_flags) isn't
  // being called.
  NOTREACHED();
}

void DialogModelMenuModelAdapter::ActivatedAt(int index, int event_flags) {
  DialogModelMenuItem* menu_item = GetField(index)->AsMenuItem(GetPassKey());
  menu_item->OnActivated(GetPassKey(), event_flags);
}

const DialogModelField* DialogModelMenuModelAdapter::GetField(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetItemCount());
  return model_->fields(GetPassKey())[index].get();
}

DialogModelField* DialogModelMenuModelAdapter::GetField(int index) {
  return const_cast<DialogModelField*>(
      const_cast<const DialogModelMenuModelAdapter*>(this)->GetField(index));
}

}  // namespace ui
