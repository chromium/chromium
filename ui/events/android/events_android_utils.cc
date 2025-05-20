// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/events_android_utils.h"

#include <android/input.h>

#include "base/notreached.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/event_type_android.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace ui {

PlatformEvent NativeEventFromEvent(Event& event) {
  if (event.HasNativeEvent()) {
    return event.native_event();
  }

  if (event.IsKeyEvent()) {
    KeyEvent* key_event = event.AsKeyEvent();

    int action = AndroidKeyEventActionFromEventType(key_event->type());
    int key_code = AndroidKeyCodeFromKeyboardCode(key_event->key_code());
    int meta_state = AndroidMetaStateFromEventFlags(key_event->flags());

    return PlatformEventAndroid(KeyEventAndroid(action, key_code, meta_state));
  }

  // Support other event types as needed.
  NOTREACHED();
}

}  // namespace ui
