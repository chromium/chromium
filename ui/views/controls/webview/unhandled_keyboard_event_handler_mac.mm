// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#import "base/mac/foundation_util.h"
#import "ui/views_bridge_mac/native_widget_mac_nswindow.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    gfx::NativeEvent event,
    FocusManager* focus_manager) {
  [[base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>([event window])
      commandDispatcher] redispatchKeyEvent:event];
  return true;
}

}  // namespace views
