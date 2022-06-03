// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TYPES_EVENT_TYPE_H_
#define UI_EVENTS_TYPES_EVENT_TYPE_H_

namespace ui {

// Event types. (prefixed because of a conflict with windows headers)
enum EventType {
  ET_UNKNOWN = 0,
  ET_MOUSE_PRESSED,
  ET_MOUSE_DRAGGED,
  ET_MOUSE_RELEASED,
  ET_MOUSE_MOVED,
  ET_MOUSE_ENTERED,
  ET_MOUSE_EXITED,
  ET_KEY_PRESSED,
  ET_KEY_RELEASED,
  ET_MOUSEWHEEL,
  ET_MOUSE_CAPTURE_CHANGED,  // Event has no location.
  ET_TOUCH_RELEASED,
  ET_TOUCH_PRESSED,
  // NOTE: This corresponds to a drag and is always preceded by an
  // ET_TOUCH_PRESSED. GestureRecognizers generally ignore ET_TOUCH_MOVED events
  // without a corresponding ET_TOUCH_PRESSED.
  ET_TOUCH_MOVED,
  ET_TOUCH_CANCELLED,
  ET_DROP_TARGET_EVENT,

  // GestureEvent types
  ET_GESTURE_SCROLL_BEGIN,
  ET_GESTURE_TYPE_START = ET_GESTURE_SCROLL_BEGIN,
  ET_GESTURE_SCROLL_END,
  ET_GESTURE_SCROLL_UPDATE,
  ET_GESTURE_TAP,
  ET_GESTURE_TAP_DOWN,
  ET_GESTURE_TAP_CANCEL,
  ET_GESTURE_TAP_UNCONFIRMED,  // User tapped, but the tap delay hasn't expired.
  ET_GESTURE_DOUBLE_TAP,
  ET_GESTURE_BEGIN,  // The first event sent when each finger is pressed.
  ET_GESTURE_END,    // Sent for each released finger.
  ET_GESTURE_TWO_FINGER_TAP,
  ET_GESTURE_PINCH_BEGIN,
  ET_GESTURE_PINCH_END,
  ET_GESTURE_PINCH_UPDATE,
  ET_GESTURE_LONG_PRESS,
  ET_GESTURE_LONG_TAP,
  // A SWIPE gesture can happen at the end of a touch sequence involving one or
  // more fingers if the finger velocity was high enough when the first finger
  // was released.
  ET_GESTURE_SWIPE,
  ET_GESTURE_SHOW_PRESS,

  // Scroll support.
  // TODO[davemoore] we need to unify these events w/ touch and gestures.
  ET_SCROLL,
  ET_SCROLL_FLING_START,
  ET_SCROLL_FLING_CANCEL,
  ET_GESTURE_TYPE_END = ET_SCROLL_FLING_CANCEL,

  // Sent by the system to indicate any modal type operations, such as drag and
  // drop or menus, should stop.
  ET_CANCEL_MODE,

  // Sent by the CrOS gesture library for interesting patterns that we want
  // to track with the UMA system.
  ET_UMA_DATA,

  // Must always be last. User namespace starts above this value.
  // See ui::RegisterCustomEventType().
  ET_LAST
};

}  // namespace ui

#endif  // UI_EVENTS_TYPES_EVENT_TYPE_H_
