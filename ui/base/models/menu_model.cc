// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/menu_model.h"

namespace ui {

MenuModel::MenuModel() : menu_model_delegate_(nullptr) {}

MenuModel::~MenuModel() {
  if (menu_model_delegate_)
    menu_model_delegate_->OnMenuClearingDelegate();
}

bool MenuModel::IsVisibleAt(int index) const {
  return true;
}

// static
bool MenuModel::GetModelAndIndexForCommandId(int command_id,
                                             MenuModel** model,
                                             int* index) {
  const int item_count = (*model)->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    const int candidate_index = i;
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

base::string16 MenuModel::GetMinorTextAt(int index) const {
  return base::string16();
}

const gfx::VectorIcon* MenuModel::GetMinorIconAt(int index) const {
  return nullptr;
}

const gfx::FontList* MenuModel::GetLabelFontListAt(int index) const {
  return NULL;
}

const gfx::VectorIcon* MenuModel::GetVectorIconAt(int index) const {
  return nullptr;
}

// Default implementation ignores the event flags.
void MenuModel::ActivatedAt(int index, int event_flags) {
  ActivatedAt(index);
}

void MenuModel::SetMenuModelDelegate(MenuModelDelegate* delegate) {
  // A non-null delegate overwriting our non-null delegate is not allowed.
  DCHECK(!(menu_model_delegate_ && delegate));
  if (menu_model_delegate_)
    menu_model_delegate_->OnMenuClearingDelegate();
  menu_model_delegate_ = delegate;
}

}  // namespace ui
