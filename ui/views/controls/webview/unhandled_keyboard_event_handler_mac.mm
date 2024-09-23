// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include "components/input/native_web_keyboard_event.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    const input::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  if (event.skip_if_unhandled) {
    return false;
  }

  NSEvent* ns_event = event.os_event.Get();
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(ns_event.window);
  if (host) {
    return host->RedispatchKeyEvent(ns_event);
  }
  return false;
}

}  // namespace views
