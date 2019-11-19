// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_MENU_LIST_H_
#define UI_BASE_X_X11_MENU_LIST_H_

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/gfx/x/x11_types.h"

// A process wide singleton cache for X menus.
namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace ui {

// Keeps track of created and destroyed top level menu windows.
class COMPONENT_EXPORT(UI_BASE_X) XMenuList {
 public:
  static XMenuList* GetInstance();

  // Checks if |menu| has _NET_WM_WINDOW_TYPE property set to
  // "_NET_WM_WINDOW_TYPE_MENU" atom and if so caches it.
  void MaybeRegisterMenu(XID menu);

  // Finds |menu| in cache and if found removes it.
  void MaybeUnregisterMenu(XID menu);

  // Inserts cached menu XIDs at the beginning of |stack|.
  void InsertMenuWindowXIDs(std::vector<XID>* stack);

 private:
  friend struct base::DefaultSingletonTraits<XMenuList>;
  XMenuList();
  ~XMenuList();

  std::vector<XID> menus_;
  XAtom menu_type_atom_;
  DISALLOW_COPY_AND_ASSIGN(XMenuList);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_MENU_LIST_H_
