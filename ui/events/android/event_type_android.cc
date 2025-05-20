// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/event_type_android.h"

#include <android/input.h>

#include "base/notreached.h"

namespace ui {

EventType EventTypeFromAndroidKeyEventAction(int key_event_action) {
  switch (key_event_action) {
    case AKEY_EVENT_ACTION_DOWN:
      return EventType::kKeyPressed;
    case AKEY_EVENT_ACTION_UP:
      return EventType::kKeyReleased;
    default:
      return EventType::kUnknown;
  }
}

int AndroidKeyEventActionFromEventType(EventType event_type) {
  switch (event_type) {
    case EventType::kKeyPressed:
      return AKEY_EVENT_ACTION_DOWN;
    case EventType::kKeyReleased:
      return AKEY_EVENT_ACTION_UP;
    default:
      NOTREACHED();
  }
}

}  // namespace ui
