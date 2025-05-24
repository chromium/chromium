// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_EVENT_TYPE_ANDROID_H_
#define UI_EVENTS_ANDROID_EVENT_TYPE_ANDROID_H_

#include "ui/events/events_export.h"
#include "ui/events/types/event_type.h"

namespace ui {

// Converts android key event action to EventType.
EVENTS_EXPORT EventType
EventTypeFromAndroidKeyEventAction(int key_event_action);

// Converts EventType for a key event to android key event action.
EVENTS_EXPORT int AndroidKeyEventActionFromEventType(EventType event_type);

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_EVENT_TYPE_ANDROID_H_
