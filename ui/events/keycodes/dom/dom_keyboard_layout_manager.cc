// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"

#include <utility>


namespace ui {

DomKeyboardLayoutManager::DomKeyboardLayoutManager() = default;

DomKeyboardLayoutManager::~DomKeyboardLayoutManager() = default;

DomKeyboardLayout* DomKeyboardLayoutManager::GetLayout(int layout_group_id) {
  if (layouts_.find(layout_group_id) == layouts_.end()) {
    layout_order_.push_back(layout_group_id);
    layouts_.emplace(layout_group_id, std::make_unique<DomKeyboardLayout>());
  }
  return layouts_[layout_group_id].get();
}

DomKeyboardLayout* DomKeyboardLayoutManager::GetFirstAsciiCapableLayout() {
  for (const auto i : layout_order_) {
    if (GetLayout(i)->IsAsciiCapable())
      return GetLayout(i);
  }
  return GetLayout(0);
}

}  // namespace ui
