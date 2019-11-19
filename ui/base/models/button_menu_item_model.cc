// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/button_menu_item_model.h"

#include "ui/base/l10n/l10n_util.h"

namespace ui {

bool ButtonMenuItemModel::Delegate::IsItemForCommandIdDynamic(
    int command_id) const {
  return false;
}

base::string16 ButtonMenuItemModel::Delegate::GetLabelForCommandId(
    int command_id) const {
  return base::string16();
}

bool ButtonMenuItemModel::Delegate::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ButtonMenuItemModel::Delegate::DoesCommandIdDismissMenu(
    int command_id) const {
  return true;
}

bool ButtonMenuItemModel::Delegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return false;
}

struct ButtonMenuItemModel::Item {
  int command_id;
  ButtonType type;
  base::string16 label;
  bool part_of_group;
};

ButtonMenuItemModel::ButtonMenuItemModel(
    int string_id,
    ButtonMenuItemModel::Delegate* delegate)
    : item_label_(l10n_util::GetStringUTF16(string_id)),
      delegate_(delegate) {
}

ButtonMenuItemModel::~ButtonMenuItemModel() {
}

void ButtonMenuItemModel::AddGroupItemWithStringId(
    int command_id, int string_id) {
  Item item = {command_id, TYPE_BUTTON, l10n_util::GetStringUTF16(string_id),
               true};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddImageItem(int command_id) {
  Item item = {command_id, TYPE_BUTTON, base::string16(), false};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddButtonLabel(int command_id, int string_id) {
  Item item = {command_id, TYPE_BUTTON_LABEL,
               l10n_util::GetStringUTF16(string_id), false};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddSpace() {
  Item item = {0, TYPE_SPACE, base::string16(), false};
  items_.push_back(item);
}

int ButtonMenuItemModel::GetItemCount() const {
  return static_cast<int>(items_.size());
}

ButtonMenuItemModel::ButtonType ButtonMenuItemModel::GetTypeAt(
    int index) const {
  return items_[index].type;
}

int ButtonMenuItemModel::GetCommandIdAt(int index) const {
  return items_[index].command_id;
}

bool ButtonMenuItemModel::IsItemDynamicAt(int index) const {
  if (delegate_)
    return delegate_->IsItemForCommandIdDynamic(GetCommandIdAt(index));
  return false;
}

bool ButtonMenuItemModel::GetAcceleratorAt(int index,
                                           ui::Accelerator* accelerator) const {
  if (delegate_) {
    return delegate_->GetAcceleratorForCommandId(GetCommandIdAt(index),
                                                 accelerator);
  }
  return false;
}

base::string16 ButtonMenuItemModel::GetLabelAt(int index) const {
  if (IsItemDynamicAt(index))
    return delegate_->GetLabelForCommandId(GetCommandIdAt(index));
  return items_[index].label;
}

bool ButtonMenuItemModel::PartOfGroup(int index) const {
  return items_[index].part_of_group;
}

void ButtonMenuItemModel::ActivatedAt(int index) {
  if (delegate_)
    delegate_->ExecuteCommand(GetCommandIdAt(index), 0);
}

bool ButtonMenuItemModel::IsEnabledAt(int index) const {
  return IsCommandIdEnabled(items_[index].command_id);
}

bool ButtonMenuItemModel::DismissesMenuAt(int index) const {
  return DoesCommandIdDismissMenu(items_[index].command_id);
}

bool ButtonMenuItemModel::IsCommandIdEnabled(int command_id) const {
  if (delegate_)
    return delegate_->IsCommandIdEnabled(command_id);
  return true;
}

bool ButtonMenuItemModel::DoesCommandIdDismissMenu(int command_id) const {
  if (delegate_)
    return delegate_->DoesCommandIdDismissMenu(command_id);
  return true;
}


}  // namespace ui
