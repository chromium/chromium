// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    gfx::NativeEvent event,
    FocusManager* focus_manager) {
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow([event window]);
  if (host)
    return host->RedispatchKeyEvent(event);
  return false;
}

}  // namespace views
