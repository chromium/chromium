// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MANAGER_H_
#define UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "ui/events/keycodes/dom/dom_keyboard_layout.h"

namespace ui {

class DomKeyboardLayoutManager final {
 public:
  DomKeyboardLayoutManager();

  DomKeyboardLayoutManager(const DomKeyboardLayoutManager&) = delete;
  DomKeyboardLayoutManager& operator=(const DomKeyboardLayoutManager&) = delete;

  ~DomKeyboardLayoutManager();

  // Get the layout with the given id, or create a new one if it doesn't
  // already exist in the list of layouts.
  // When adding multiple layouts, they should be added in priority order
  // (as determined by the underlying platform).
  DomKeyboardLayout* GetLayout(int layout_group_id);

  DomKeyboardLayout* GetFirstAsciiCapableLayout();

 private:
  std::vector<int> layout_order_;

  std::map<int, std::unique_ptr<DomKeyboardLayout>> layouts_;
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_DOM_KEYBOARD_LAYOUT_MANAGER_H_
