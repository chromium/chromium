// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_types.h"

#include "ui/events/event.h"

namespace ui {

MenuSourceType GetMenuSourceTypeForEvent(const ui::Event& event) {
  ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
  if (event.IsKeyEvent())
    source_type = ui::MENU_SOURCE_KEYBOARD;
  if (event.IsTouchEvent() || event.IsGestureEvent())
    source_type = ui::MENU_SOURCE_TOUCH;
  return source_type;
}

}  // namespace ui
