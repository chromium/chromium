// Copyright 2022 The Chromium Authors
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
base::WeakPtr<ui::MenuModel> ComboboxMenuModel::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t ComboboxMenuModel::GetItemCount() const {
  return model_->GetItemCount();
}

ui::MenuModel::ItemType ComboboxMenuModel::GetTypeAt(size_t index) const {
  if (model_->IsItemSeparatorAt(index))
    return TYPE_SEPARATOR;
  return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
}

ui::MenuSeparatorType ComboboxMenuModel::GetSeparatorTypeAt(
    size_t index) const {
  return ui::NORMAL_SEPARATOR;
}

int ComboboxMenuModel::GetCommandIdAt(size_t index) const {
  // Define the id of the first item in the menu (since it needs to be > 0)
  constexpr int kFirstMenuItemId = 1000;
  return static_cast<int>(index) + kFirstMenuItemId;
}

std::u16string ComboboxMenuModel::GetLabelAt(size_t index) const {
  // Inserting the Unicode formatting characters if necessary so that the
  // text is displayed correctly in right-to-left UIs.
  std::u16string text = model_->GetItemAt(index);
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

std::u16string ComboboxMenuModel::GetSecondaryLabelAt(size_t index) const {
  std::u16string text = model_->GetDropDownSecondaryTextAt(index);
  base::i18n::AdjustStringForLocaleDirection(&text);
  return text;
}

bool ComboboxMenuModel::IsItemDynamicAt(size_t index) const {
  return true;
}

const gfx::FontList* ComboboxMenuModel::GetLabelFontListAt(size_t index) const {
  return &owner_->GetFontList();
}

bool ComboboxMenuModel::GetAcceleratorAt(size_t index,
                                         ui::Accelerator* accelerator) const {
  return false;
}

bool ComboboxMenuModel::IsItemCheckedAt(size_t index) const {
  return UseCheckmarks() && index == owner_->GetSelectedIndex();
}

int ComboboxMenuModel::GetGroupIdAt(size_t index) const {
  return -1;
}

ui::ImageModel ComboboxMenuModel::GetIconAt(size_t index) const {
  return model_->GetDropDownIconAt(index);
}

ui::ButtonMenuItemModel* ComboboxMenuModel::GetButtonMenuItemAt(
    size_t index) const {
  return nullptr;
}

bool ComboboxMenuModel::IsEnabledAt(size_t index) const {
  return model_->IsItemEnabledAt(index);
}

void ComboboxMenuModel::ActivatedAt(size_t index) {
  owner_->MenuSelectionAt(index);
}

void ComboboxMenuModel::ActivatedAt(size_t index, int event_flags) {
  ActivatedAt(index);
}

ui::MenuModel* ComboboxMenuModel::GetSubmenuModelAt(size_t index) const {
  return nullptr;
}

std::optional<ui::ColorId> ComboboxMenuModel::GetForegroundColorId(
    size_t index) {
  return model_->GetDropdownForegroundColorIdAt(index);
}

std::optional<ui::ColorId> ComboboxMenuModel::GetSubmenuBackgroundColorId(
    size_t index) {
  return model_->GetDropdownBackgroundColorIdAt(index);
}

std::optional<ui::ColorId> ComboboxMenuModel::GetSelectedBackgroundColorId(
    size_t index) {
  return model_->GetDropdownSelectedBackgroundColorIdAt(index);
}
