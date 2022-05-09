// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/events/event.h"
#include "ui/views/focus/focus_manager.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  if (event.skip_in_browser)
    return false;

  return !focus_manager->OnKeyEvent(*(event.os_event->AsKeyEvent()));
}

}  // namespace views
