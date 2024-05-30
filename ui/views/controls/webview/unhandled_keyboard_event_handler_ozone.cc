// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include "components/input/native_web_keyboard_event.h"
#include "ui/events/event.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_utils.h"
#include "ui/views/focus/focus_manager.h"

namespace views {

// static
bool UnhandledKeyboardEventHandler::HandleNativeKeyboardEvent(
    const input::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  auto& key_event = *event.os_event->AsKeyEvent();
  if (!event.skip_if_unhandled) {
    // Try to re-send via FocusManager.
    // Note: FocusManager::OnKeyEvent returns true iff the given event
    // needs to continue to propagated. So, negate the condition to calculate
    // whether it is consumed.
    if (!focus_manager->OnKeyEvent(key_event))
      return true;
  }

  // Send it back to the platform via Ozone.
  if (auto* util = ui::OzonePlatform::GetInstance()->GetPlatformUtils()) {
    util->OnUnhandledKeyEvent(key_event);
    return true;
  }

  return false;
}

}  // namespace views
