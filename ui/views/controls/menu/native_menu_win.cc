// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/native_menu_win.h"

#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_insertion_delegate_win.h"

namespace views {

struct NativeMenuWin::ItemData {
  // The Windows API requires that whoever creates the menus must own the
  // strings used for labels, and keep them around for the lifetime of the
  // created menu. So be it.
  std::u16string label;

  // Someone needs to own submenus, it may as well be us.
  std::unique_ptr<NativeMenuWin> submenu;

  // We need a pointer back to the containing menu in various circumstances.
  raw_ptr<NativeMenuWin> native_menu_win;

  // The index of the item within the menu's model.
  size_t model_index;
};

// Returns the NativeMenuWin for a particular HMENU.
static NativeMenuWin* GetNativeMenuWinFromHMENU(HMENU hmenu) {
  MENUINFO mi = {0};
  mi.cbSize = sizeof(mi);
  mi.fMask = MIM_MENUDATA | MIM_STYLE;
  GetMenuInfo(hmenu, &mi);
  return reinterpret_cast<NativeMenuWin*>(mi.dwMenuData);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, public:

NativeMenuWin::NativeMenuWin(ui::MenuModel* model, HWND sysmenu_hwnd)
    : model_(model), sysmenu_hwnd_(sysmenu_hwnd) {}

NativeMenuWin::~NativeMenuWin() {
  items_.clear();
  DestroyMenu(menu_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, MenuWrapper implementation:

void NativeMenuWin::Rebuild(MenuInsertionDelegateWin* delegate) {
  ResetNativeMenu();
  items_.clear();

  first_item_index_ = delegate ? delegate->GetInsertionIndex(menu_) : size_t{0};
  for (size_t model_index = 0; model_index < model_->GetItemCount();
       ++model_index) {
    size_t menu_index = model_index + first_item_index_;
    if (model_->GetTypeAt(model_index) == ui::MenuModel::TYPE_SEPARATOR)
      AddSeparatorItemAt(menu_index, model_index);
    else
      AddMenuItemAt(menu_index, model_index);
  }
}

void NativeMenuWin::UpdateStates() {
  // A depth-first walk of the menu items, updating states.
  size_t model_index = 0;
  for (const auto& item : items_) {
    size_t menu_index = model_index + first_item_index_;
    SetMenuItemState(menu_index, model_->IsEnabledAt(model_index),
                     model_->IsItemCheckedAt(model_index), false);
    if (model_->IsItemDynamicAt(model_index)) {
      // TODO(atwilson): Update the icon as well (http://crbug.com/66508).
      SetMenuItemLabel(menu_index, model_index,
                       model_->GetLabelAt(model_index));
    }
    NativeMenuWin* submenu = item->submenu.get();
    if (submenu)
      submenu->UpdateStates();
    ++model_index;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuWin, private:

bool NativeMenuWin::IsSeparatorItemAt(size_t menu_index) const {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  GetMenuItemInfo(menu_, menu_index, MF_BYPOSITION, &mii);
  return !!(mii.fType & MF_SEPARATOR);
}

void NativeMenuWin::AddMenuItemAt(size_t menu_index, size_t model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
  mii.fType = MFT_STRING;

  std::unique_ptr<ItemData> item_data = std::make_unique<ItemData>();
  item_data->label = std::u16string();
  ui::MenuModel::ItemType type = model_->GetTypeAt(model_index);
  if (type == ui::MenuModel::TYPE_SUBMENU) {
    item_data->submenu = std::make_unique<NativeMenuWin>(
        model_->GetSubmenuModelAt(model_index), nullptr);
    item_data->submenu->Rebuild(nullptr);
    mii.fMask |= MIIM_SUBMENU;
    mii.hSubMenu = item_data->submenu->menu_;
    GetNativeMenuWinFromHMENU(mii.hSubMenu)->parent_ = this;
  } else {
    if (type == ui::MenuModel::TYPE_RADIO)
      mii.fType |= MFT_RADIOCHECK;
    mii.wID = static_cast<UINT>(model_->GetCommandIdAt(model_index));
  }
  item_data->native_menu_win = this;
  item_data->model_index = model_index;
  mii.dwItemData = reinterpret_cast<ULONG_PTR>(item_data.get());
  items_.insert(items_.begin() + static_cast<ptrdiff_t>(model_index),
                std::move(item_data));
  UpdateMenuItemInfoForString(&mii, model_index,
                              model_->GetLabelAt(model_index));
  InsertMenuItem(menu_, menu_index, TRUE, &mii);
}

void NativeMenuWin::AddSeparatorItemAt(size_t menu_index, size_t model_index) {
  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_FTYPE;
  mii.fType = MFT_SEPARATOR;
  // Insert a dummy entry into our label list so we can index directly into it
  // using item indices if need be.
  items_.insert(items_.begin() + static_cast<ptrdiff_t>(model_index),
                std::make_unique<ItemData>());
  InsertMenuItem(menu_, static_cast<UINT>(menu_index), TRUE, &mii);
}

void NativeMenuWin::SetMenuItemState(size_t menu_index,
                                     bool enabled,
                                     bool checked,
                                     bool is_default) {
  if (IsSeparatorItemAt(menu_index))
    return;

  UINT state = enabled ? MFS_ENABLED : MFS_DISABLED;
  if (checked)
    state |= MFS_CHECKED;
  if (is_default)
    state |= MFS_DEFAULT;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STATE;
  mii.fState = state;
  SetMenuItemInfo(menu_, static_cast<UINT>(menu_index), MF_BYPOSITION, &mii);
}

void NativeMenuWin::SetMenuItemLabel(size_t menu_index,
                                     size_t model_index,
                                     const std::u16string& label) {
  if (IsSeparatorItemAt(menu_index))
    return;

  MENUITEMINFO mii = {0};
  mii.cbSize = sizeof(mii);
  UpdateMenuItemInfoForString(&mii, model_index, label);
  SetMenuItemInfo(menu_, static_cast<UINT>(menu_index), MF_BYPOSITION, &mii);
}

void NativeMenuWin::UpdateMenuItemInfoForString(MENUITEMINFO* mii,
                                                size_t model_index,
                                                const std::u16string& label) {
  std::u16string formatted = label;
  ui::MenuModel::ItemType type = model_->GetTypeAt(model_index);
  // Strip out any tabs, otherwise they get interpreted as accelerators and can
  // lead to weird behavior.
  base::ReplaceSubstringsAfterOffset(&formatted, 0, u"\t", u" ");
  if (type != ui::MenuModel::TYPE_SUBMENU) {
    // Add accelerator details to the label if provided.
    ui::Accelerator accelerator(ui::VKEY_UNKNOWN, ui::EF_NONE);
    if (model_->GetAcceleratorAt(model_index, &accelerator)) {
      formatted += u"\t";
      formatted += accelerator.GetShortcutText();
    }
  }

  // Update the owned string, since Windows will want us to keep this new
  // version around.
  items_[model_index]->label = formatted;

  // Give Windows a pointer to the label string.
  mii->fMask |= MIIM_STRING;
  mii->dwTypeData = base::as_writable_wcstr(items_[model_index]->label);
}

void NativeMenuWin::ResetNativeMenu() {
  if (IsWindow(sysmenu_hwnd_)) {
    if (menu_) {
      GetSystemMenu(sysmenu_hwnd_, TRUE);
    }
    menu_ = GetSystemMenu(sysmenu_hwnd_, FALSE);
  } else {
    if (menu_) {
      DestroyMenu(menu_);
    }
    menu_ = CreatePopupMenu();
    // Rather than relying on the return value of TrackPopupMenuEx, which is
    // always a command identifier, instead we tell the menu to notify us via
    // our host window and the WM_MENUCOMMAND message.
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE | MIM_MENUDATA;
    mi.dwStyle = MNS_NOTIFYBYPOS;
    mi.dwMenuData = reinterpret_cast<ULONG_PTR>(this);
    SetMenuInfo(menu_, &mi);
  }
}

}  // namespace views
