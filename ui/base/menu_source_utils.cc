// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/menu_source_utils.h"

#include "ui/events/event.h"

namespace ui {

MenuSourceType GetMenuSourceTypeForEvent(const Event& event) {
  if (event.IsKeyEvent())
    return MENU_SOURCE_KEYBOARD;
  if (event.IsTouchEvent() || event.IsGestureEvent())
    return MENU_SOURCE_TOUCH;
  return MENU_SOURCE_MOUSE;
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)
MenuSourceType GetMenuSourceType(int event_flags) {
  if (event_flags & EF_MOUSE_BUTTON) {
    return MENU_SOURCE_MOUSE;
  }
  if (event_flags & EF_FROM_TOUCH) {
    return MENU_SOURCE_TOUCH;
  }
  return MENU_SOURCE_KEYBOARD;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace ui
