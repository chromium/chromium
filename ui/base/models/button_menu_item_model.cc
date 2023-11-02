// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/button_menu_item_model.h"

#include "ui/base/l10n/l10n_util.h"

namespace ui {

bool ButtonMenuItemModel::Delegate::IsItemForCommandIdDynamic(
    int command_id) const {
  return false;
}

std::u16string ButtonMenuItemModel::Delegate::GetLabelForCommandId(
    int command_id) const {
  return std::u16string();
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
  std::u16string label;
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
  Item item = {command_id, TYPE_BUTTON, std::u16string(), false};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddButtonLabel(int command_id, int string_id) {
  Item item = {command_id, TYPE_BUTTON_LABEL,
               l10n_util::GetStringUTF16(string_id), false};
  items_.push_back(item);
}

void ButtonMenuItemModel::AddSpace() {
  Item item = {0, TYPE_SPACE, std::u16string(), false};
  items_.push_back(item);
}

size_t ButtonMenuItemModel::GetItemCount() const {
  return items_.size();
}

ButtonMenuItemModel::ButtonType ButtonMenuItemModel::GetTypeAt(
    size_t index) const {
  return items_[index].type;
}

int ButtonMenuItemModel::GetCommandIdAt(size_t index) const {
  return items_[index].command_id;
}

bool ButtonMenuItemModel::IsItemDynamicAt(size_t index) const {
  return delegate_ &&
         delegate_->IsItemForCommandIdDynamic(GetCommandIdAt(index));
}

bool ButtonMenuItemModel::GetAcceleratorAt(size_t index,
                                           ui::Accelerator* accelerator) const {
  return delegate_ && delegate_->GetAcceleratorForCommandId(
                          GetCommandIdAt(index), accelerator);
}

std::u16string ButtonMenuItemModel::GetLabelAt(size_t index) const {
  return IsItemDynamicAt(index)
             ? delegate_->GetLabelForCommandId(GetCommandIdAt(index))
             : items_[index].label;
}

bool ButtonMenuItemModel::PartOfGroup(size_t index) const {
  return items_[index].part_of_group;
}

void ButtonMenuItemModel::ActivatedAt(size_t index) {
  if (delegate_)
    delegate_->ExecuteCommand(GetCommandIdAt(index), 0);
}

bool ButtonMenuItemModel::IsEnabledAt(size_t index) const {
  return IsCommandIdEnabled(items_[index].command_id);
}

bool ButtonMenuItemModel::DismissesMenuAt(size_t index) const {
  return DoesCommandIdDismissMenu(items_[index].command_id);
}

bool ButtonMenuItemModel::IsCommandIdEnabled(int command_id) const {
  return !delegate_ || delegate_->IsCommandIdEnabled(command_id);
}

bool ButtonMenuItemModel::DoesCommandIdDismissMenu(int command_id) const {
  return !delegate_ || delegate_->DoesCommandIdDismissMenu(command_id);
}


}  // namespace ui
