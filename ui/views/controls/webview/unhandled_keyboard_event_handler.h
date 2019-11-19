// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/webview/webview_export.h"

namespace content {
struct NativeWebKeyboardEvent;
}

namespace views {
class FocusManager;

// This class handles unhandled keyboard messages coming back from the renderer
// process.
class WEBVIEW_EXPORT UnhandledKeyboardEventHandler {
 public:
  UnhandledKeyboardEventHandler();
  ~UnhandledKeyboardEventHandler();

  bool HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event,
                           FocusManager* focus_manager);

 private:
  // Platform specific handling for unhandled keyboard events.
  static bool HandleNativeKeyboardEvent(gfx::NativeEvent event,
                                        FocusManager* focus_manager);

  // Whether to ignore the next Char keyboard event.
  // If a RawKeyDown event was handled as a shortcut key, then we're done
  // handling it and should eat any Char event that the translate phase may
  // have produced from it. (Handling this event may cause undesirable effects,
  // such as a beep if DefWindowProc() has no default handling for the given
  // Char.)
  bool ignore_next_char_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(UnhandledKeyboardEventHandler);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_UNHANDLED_KEYBOARD_EVENT_HANDLER_H_
