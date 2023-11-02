// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include "content/public/browser/native_web_keyboard_event.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  if (event.skip_in_browser)
    return false;

  auto os_event = event.os_event;
  auto* host = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      [os_event window]);
  if (host)
    return host->RedispatchKeyEvent(os_event);
  return false;
}

}  // namespace views
