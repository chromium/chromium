// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_WIN_H_
#define UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_WIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace ui {
class MenuModel;
}

namespace views {

class MenuInsertionDelegateWin;

// A wrapper around a native Windows menu.
class VIEWS_EXPORT NativeMenuWin {
 public:
  // Construct a NativeMenuWin, with a model and delegate. If |sysmenu_hwnd|
  // is non-null, the NativeMenuWin wraps the system menu for that window.
  // The caller owns the model and the delegate.
  NativeMenuWin(ui::MenuModel* model, HWND sysmenu_hwnd);

  NativeMenuWin(const NativeMenuWin&) = delete;
  NativeMenuWin& operator=(const NativeMenuWin&) = delete;

  ~NativeMenuWin();

  void Rebuild(MenuInsertionDelegateWin* delegate);
  void UpdateStates();

 private:
  // IMPORTANT: Note about indices.
  //            Functions in this class deal in two index spaces:
  //            1. menu_index - the index of an item within the actual Windows
  //               native menu.
  //            2. model_index - the index of the item within our model.
  //            These two are most often but not always the same value! The
  //            notable exception is when this object is used to wrap the
  //            Windows System Menu. In this instance, the model indices start
  //            at 0, but the insertion index into the existing menu is not.
  //            It is important to take this into consideration when editing the
  //            code in the functions in this class.

  // Returns true if the item at the specified index is a separator.
  bool IsSeparatorItemAt(size_t menu_index) const;

  // Add items. See note above about indices.
  void AddMenuItemAt(size_t menu_index, size_t model_index);
  void AddSeparatorItemAt(size_t menu_index, size_t model_index);

  // Sets the state of the item at the specified index.
  void SetMenuItemState(size_t menu_index,
                        bool enabled,
                        bool checked,
                        bool is_default);

  // Sets the label of the item at the specified index.
  void SetMenuItemLabel(size_t menu_index,
                        size_t model_index,
                        const std::u16string& label);

  // Updates the local data structure with the correctly formatted version of
  // |label| at the specified model_index, and adds string data to |mii| if
  // the menu is not owner-draw. That's a mouthful. This function exists because
  // of the peculiarities of the Windows menu API.
  void UpdateMenuItemInfoForString(MENUITEMINFO* mii,
                                   size_t model_index,
                                   const std::u16string& label);

  // Resets the native menu stored in |menu_| by destroying any old menu then
  // creating a new empty one.
  void ResetNativeMenu();

  // Our attached model and delegate.
  raw_ptr<ui::MenuModel> model_;

  HMENU menu_ = nullptr;

  // An object that collects all of the data associated with an individual menu
  // item.
  struct ItemData;
  std::vector<std::unique_ptr<ItemData>> items_;

  // The HWND this menu is the system menu for, or null if the menu is not a
  // system menu.
  HWND sysmenu_hwnd_;

  // The index of the first item in the model in the menu.
  size_t first_item_index_ = 0;

  // If we're a submenu, this is our parent.
  raw_ptr<NativeMenuWin> parent_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_WIN_H_
