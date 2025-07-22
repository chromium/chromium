// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/events_android_utils.h"

#include <android/input.h>

#include "base/notreached.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/event_type_android.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

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

#define ACTION_CASE(x)              \
  case JNI_MotionEvent::ACTION_##x: \
    return MotionEvent::Action::x

MotionEvent::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    ACTION_CASE(DOWN);
    ACTION_CASE(UP);
    ACTION_CASE(MOVE);
    ACTION_CASE(CANCEL);
    ACTION_CASE(POINTER_DOWN);
    ACTION_CASE(POINTER_UP);
    ACTION_CASE(HOVER_ENTER);
    ACTION_CASE(HOVER_EXIT);
    ACTION_CASE(HOVER_MOVE);
    ACTION_CASE(BUTTON_PRESS);
    ACTION_CASE(BUTTON_RELEASE);
    default:
      NOTREACHED() << "Invalid Android MotionEvent action: " << android_action;
  }
}

#undef ACTION_CASE

MotionEvent::ToolType FromAndroidToolType(int android_tool_type) {
  switch (android_tool_type) {
    case JNI_MotionEvent::TOOL_TYPE_UNKNOWN:
      return MotionEvent::ToolType::UNKNOWN;
    case JNI_MotionEvent::TOOL_TYPE_FINGER:
      return MotionEvent::ToolType::FINGER;
    case JNI_MotionEvent::TOOL_TYPE_STYLUS:
      return MotionEvent::ToolType::STYLUS;
    case JNI_MotionEvent::TOOL_TYPE_MOUSE:
      return MotionEvent::ToolType::MOUSE;
    case JNI_MotionEvent::TOOL_TYPE_ERASER:
      return MotionEvent::ToolType::ERASER;
    default:
      NOTREACHED() << "Invalid Android MotionEvent tool type: "
                   << android_tool_type;
  }
}

}  // namespace ui
