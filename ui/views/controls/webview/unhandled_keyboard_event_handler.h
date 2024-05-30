// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/webview/webview_export.h"

namespace input {
struct NativeWebKeyboardEvent;
}

namespace views {
class FocusManager;

// This class handles unhandled keyboard messages coming back from the renderer
// process.
class WEBVIEW_EXPORT UnhandledKeyboardEventHandler {
 public:
  UnhandledKeyboardEventHandler();

  UnhandledKeyboardEventHandler(const UnhandledKeyboardEventHandler&) = delete;
  UnhandledKeyboardEventHandler& operator=(
      const UnhandledKeyboardEventHandler&) = delete;

  ~UnhandledKeyboardEventHandler();

  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event,
                           FocusManager* focus_manager);

 private:
  // Platform specific handling for unhandled keyboard events.
  static bool HandleNativeKeyboardEvent(
      const input::NativeWebKeyboardEvent& event,
      FocusManager* focus_manager);

  // Whether to ignore the next Char keyboard event.
  // If a RawKeyDown event was handled as a shortcut key, then we're done
  // handling it and should eat any Char event that the translate phase may
  // have produced from it. (Handling this event may cause undesirable effects,
  // such as a beep if DefWindowProc() has no default handling for the given
  // Char.)
  bool ignore_next_char_event_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
