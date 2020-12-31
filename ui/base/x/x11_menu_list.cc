// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_menu_list.h"

#include <algorithm>

#include "base/memory/singleton.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto_util.h"

namespace ui {

// static
XMenuList* XMenuList::GetInstance() {
  return base::Singleton<XMenuList>::get();
}

XMenuList::XMenuList()
    : menu_type_atom_(x11::GetAtom("_NET_WM_WINDOW_TYPE_MENU")) {}

XMenuList::~XMenuList() {
  menus_.clear();
}

void XMenuList::MaybeRegisterMenu(x11::Window menu) {
  x11::Atom value;
  if (!GetProperty(menu, x11::GetAtom("_NET_WM_WINDOW_TYPE"), &value) ||
      value != menu_type_atom_) {
    return;
  }
  menus_.push_back(menu);
}

void XMenuList::MaybeUnregisterMenu(x11::Window menu) {
  auto iter = std::find(menus_.begin(), menus_.end(), menu);
  if (iter == menus_.end())
    return;
  menus_.erase(iter);
}

void XMenuList::InsertMenuWindows(std::vector<x11::Window>* stack) {
  stack->insert(stack->begin(), menus_.begin(), menus_.end());
}

}  // namespace ui
