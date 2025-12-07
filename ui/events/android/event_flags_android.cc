// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/event_flags_android.h"

#include <android/input.h>

#include "ui/events/event_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace ui {

EventFlags EventFlagsFromAndroidMetaState(int meta_state) {
  int flags = EF_NONE;

  if ((meta_state & AMETA_SHIFT_ON) != 0) {
    flags |= EF_SHIFT_DOWN;
  }
  if ((meta_state & AMETA_CTRL_ON) != 0) {
    flags |= EF_CONTROL_DOWN;
  }
  if ((meta_state & AMETA_ALT_ON) != 0) {
    flags |= EF_ALT_DOWN;
  }
  if ((meta_state & AMETA_META_ON) != 0) {
    flags |= EF_COMMAND_DOWN;
  }
  if ((meta_state & AMETA_CAPS_LOCK_ON) != 0) {
    flags |= EF_CAPS_LOCK_ON;
  }

  return flags;
}

int AndroidMetaStateFromEventFlags(EventFlags event_flags) {
  int meta_state = AMETA_NONE;

  if (event_flags & EF_SHIFT_DOWN) {
    meta_state |= AMETA_SHIFT_ON;
  }
  if (event_flags & EF_CONTROL_DOWN) {
    meta_state |= AMETA_CTRL_ON;
  }
  if (event_flags & EF_ALT_DOWN) {
    meta_state |= AMETA_ALT_ON;
  }
  if (event_flags & EF_COMMAND_DOWN) {
    meta_state |= AMETA_META_ON;
  }
  if (event_flags & EF_CAPS_LOCK_ON) {
    meta_state |= AMETA_CAPS_LOCK_ON;
  }

  return meta_state;
}

EventFlags EventFlagsFromAndroidButtonState(int button_state) {
  int flags = EF_NONE;

  if ((button_state & JNI_MotionEvent::BUTTON_BACK) != 0) {
    flags |= EF_BACK_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_FORWARD) != 0) {
    flags |= EF_FORWARD_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_PRIMARY) != 0) {
    flags |= EF_LEFT_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_SECONDARY) != 0) {
    flags |= EF_RIGHT_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_TERTIARY) != 0) {
    flags |= EF_MIDDLE_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_PRIMARY) != 0) {
    flags |= EF_LEFT_MOUSE_BUTTON;
  }
  if ((button_state & JNI_MotionEvent::BUTTON_STYLUS_SECONDARY) != 0) {
    flags |= EF_RIGHT_MOUSE_BUTTON;
  }

  return flags;
}

}  // namespace ui
