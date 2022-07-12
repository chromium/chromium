// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox_menu_model.h"

ComboboxMenuModel::ComboboxMenuModel(views::Combobox* owner,
                                     ui::ComboboxModel* model)
    : owner_(owner), model_(model) {}

ComboboxMenuModel::~ComboboxMenuModel() = default;

bool ComboboxMenuModel::UseCheckmarks() const {
  return views::MenuConfig::instance().check_selected_combobox_item;
}

// Overridden from MenuModel:
bool ComboboxMenuModel::HasIcons() const {
  for (int i = 0; i < GetItemCount(); ++i) {
    if (!GetIconAt(i).IsEmpty())
      return true;
  }
  return false;
}

int ComboboxMenuModel::GetItemCount() const {
  return model_->GetItemCount();
}

ui::MenuModel::ItemType ComboboxMenuModel::GetTypeAt(int index) const {
  if (model_->IsItemSeparatorAt(index))
    return TYPE_SEPARATOR;
  return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
}

ui::MenuSeparatorType ComboboxMenuModel::GetSeparatorTypeAt(int index) const {
  return ui::NORMAL_SEPARATOR;
}

int ComboboxMenuModel::GetCommandIdAt(int index) const {
  // Define the id of the first item in the menu (since it needs to be > 0)
  constexpr int kFirstMenuItemId = 1000;
  return index + kFirstMenuItemId;
}

std::u16string ComboboxMenuModel::GetLabelAt(int index) const {
  // Inserting the Unicode formatting characters if necessary so that the
  // text is displayed correctly in right-to-left UIs.
  std::u16string text = model_->GetDropDownTextAt(index);
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

std::u16string ComboboxMenuModel::GetSecondaryLabelAt(int index) const {
  std::u16string text = model_->GetDropDownSecondaryTextAt(index);
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

bool ComboboxMenuModel::IsItemDynamicAt(int index) const {
  return true;
}

const gfx::FontList* ComboboxMenuModel::GetLabelFontListAt(int index) const {
  return &owner_->GetFontList();
}

bool ComboboxMenuModel::GetAcceleratorAt(int index,
                                         ui::Accelerator* accelerator) const {
  return false;
}

bool ComboboxMenuModel::IsItemCheckedAt(int index) const {
  return UseCheckmarks() && index == owner_->GetSelectedIndex();
}

int ComboboxMenuModel::GetGroupIdAt(int index) const {
  return -1;
}

ui::ImageModel ComboboxMenuModel::GetIconAt(int index) const {
  return model_->GetDropDownIconAt(index);
}

ui::ButtonMenuItemModel* ComboboxMenuModel::GetButtonMenuItemAt(
    int index) const {
  return nullptr;
}

bool ComboboxMenuModel::IsEnabledAt(int index) const {
  return model_->IsItemEnabledAt(index);
}

void ComboboxMenuModel::ActivatedAt(int index) {
  owner_->MenuSelectionAt(index);
}

void ComboboxMenuModel::ActivatedAt(int index, int event_flags) {
  ActivatedAt(index);
}

ui::MenuModel* ComboboxMenuModel::GetSubmenuModelAt(int index) const {
  return nullptr;
}
