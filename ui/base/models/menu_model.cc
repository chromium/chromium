// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/menu_model.h"

#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace ui {

MenuModel::MenuModel() : menu_model_delegate_(nullptr) {}

MenuModel::~MenuModel() {
  if (menu_model_delegate_)
    menu_model_delegate_->OnMenuClearingDelegate();
}

bool MenuModel::IsVisibleAt(size_t index) const {
  return true;
}

bool MenuModel::IsAlertedAt(size_t index) const {
  return false;
}

bool MenuModel::IsNewFeatureAt(size_t index) const {
  return false;
}

ElementIdentifier MenuModel::GetElementIdentifierAt(size_t index) const {
  return ElementIdentifier();
}

// static
bool MenuModel::GetModelAndIndexForCommandId(int command_id,
                                             MenuModel** model,
                                             size_t* index) {
  const size_t item_count = (*model)->GetItemCount();
  for (size_t i = 0; i < item_count; ++i) {
    const size_t candidate_index = i;
    // Actionable submenus have commands.
    if ((*model)->GetTypeAt(candidate_index) == TYPE_ACTIONABLE_SUBMENU &&
        (*model)->GetCommandIdAt(candidate_index) == command_id) {
      *index = candidate_index;
      return true;
    }
    if ((*model)->GetTypeAt(candidate_index) == TYPE_SUBMENU ||
        (*model)->GetTypeAt(candidate_index) == TYPE_ACTIONABLE_SUBMENU) {
      MenuModel* submenu_model = (*model)->GetSubmenuModelAt(candidate_index);
      if (GetModelAndIndexForCommandId(command_id, &submenu_model, index)) {
        *model = submenu_model;
        return true;
      }
    }
    if ((*model)->GetCommandIdAt(candidate_index) == command_id) {
      *index = candidate_index;
      return true;
    }
  }
  return false;
}

std::u16string MenuModel::GetMinorTextAt(size_t index) const {
  return std::u16string();
}

std::u16string MenuModel::GetSecondaryLabelAt(size_t index) const {
  return std::u16string();
}

ImageModel MenuModel::GetMinorIconAt(size_t index) const {
  return ImageModel();
}

bool MenuModel::MayHaveMnemonicsAt(size_t index) const {
  return true;
}

std::u16string MenuModel::GetAccessibleNameAt(size_t index) const {
  return std::u16string();
}

const gfx::FontList* MenuModel::GetLabelFontListAt(size_t index) const {
  return (GetTypeAt(index) == ui::MenuModel::TYPE_TITLE)
             ? &ui::ResourceBundle::GetSharedInstance().GetFontList(
                   ui::ResourceBundle::BoldFont)
             : nullptr;
}

// Default implementation ignores the event flags.
void MenuModel::ActivatedAt(size_t index, int event_flags) {
  ActivatedAt(index);
}

void MenuModel::SetMenuModelDelegate(MenuModelDelegate* delegate) {
  // A non-null delegate overwriting our non-null delegate is not allowed.
  DCHECK(!(menu_model_delegate_ && delegate));
  if (menu_model_delegate_)
    menu_model_delegate_->OnMenuClearingDelegate();
  menu_model_delegate_ = delegate;
}

std::optional<ui::ColorId> MenuModel::GetForegroundColorId(size_t index) {
  return std::nullopt;
}

std::optional<ui::ColorId> MenuModel::GetSubmenuBackgroundColorId(
    size_t index) {
  return std::nullopt;
}

std::optional<ui::ColorId> MenuModel::GetSelectedBackgroundColorId(
    size_t index) {
  return std::nullopt;
}

}  // namespace ui
