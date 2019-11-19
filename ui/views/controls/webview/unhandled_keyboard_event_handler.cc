// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

#include "ui/content_accelerators/accelerator_util.h"
#include "ui/views/focus/focus_manager.h"

namespace views {

UnhandledKeyboardEventHandler::UnhandledKeyboardEventHandler() = default;

UnhandledKeyboardEventHandler::~UnhandledKeyboardEventHandler() = default;

bool UnhandledKeyboardEventHandler::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    FocusManager* focus_manager) {
  if (!focus_manager) {
    NOTREACHED();
    return false;
  }
  // Previous calls to TranslateMessage can generate Char events as well as
  // RawKeyDown events, even if the latter triggered an accelerator.  In these
  // cases, we discard the Char events.
  if (event.GetType() == blink::WebInputEvent::kChar &&
      ignore_next_char_event_) {
    ignore_next_char_event_ = false;
    return false;
  }
  // It's necessary to reset this flag, because a RawKeyDown event may not
  // always generate a Char event.
  ignore_next_char_event_ = false;

  if (event.GetType() == blink::WebInputEvent::kRawKeyDown) {
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

    // This is tricky: we want to set ignore_next_char_event_ if
    // ProcessAccelerator returns true. But ProcessAccelerator might delete
    // |this| if the accelerator is a "close tab" one. So we speculatively
    // set the flag and fix it if no event was handled.
    ignore_next_char_event_ = true;

    if (focus_manager->ProcessAccelerator(accelerator))
      return true;

    // ProcessAccelerator didn't handle the accelerator, so we know both
    // that |this| is still valid, and that we didn't want to set the flag.
    ignore_next_char_event_ = false;
  }

  if (event.os_event && !event.skip_in_browser)
    return HandleNativeKeyboardEvent(event.os_event, focus_manager);

  return false;
}

}  // namespace views
