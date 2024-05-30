// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include <windows.h>

#include "components/input/native_web_keyboard_event.h"
#include "ui/events/event.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    const input::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  if (event.skip_if_unhandled) {
    return false;
  }

  // Any unhandled keyboard/character messages should be defproced.
  // This allows stuff like F10, etc to work correctly.
  const CHROME_MSG& message(event.os_event->native_event());
  ::DefWindowProc(message.hwnd, message.message, message.wParam,
                  message.lParam);
  return true;
}

}  // namespace views
