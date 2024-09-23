// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TYPES_EVENT_TYPE_H_
#define UI_EVENTS_TYPES_EVENT_TYPE_H_

namespace ui {

// Event types.
enum class EventType {
  kUnknown = 0,
  kMousePressed,
  kMouseDragged,
  kMouseReleased,
  kMouseMoved,
  kMouseEntered,
  kMouseExited,
  kKeyPressed,
  kKeyReleased,
  kMousewheel,
  kMouseCaptureChanged,  // Event has no location.
  kTouchReleased,
  kTouchPressed,
  // NOTE: This corresponds to a drag and is always preceded by an
  // kTouchPressed. GestureRecognizers generally ignore kTouchMoved events
  // without a corresponding kTouchPressed.
  kTouchMoved,
  kTouchCancelled,
  kDropTargetEvent,

  // GestureEvent types
  kGestureScrollBegin,
  kGestureTypeStart = kGestureScrollBegin,
  kGestureScrollEnd,
  kGestureScrollUpdate,
  kGestureTap,
  kGestureTapDown,
  kGestureTapCancel,
  kGestureTapUnconfirmed,  // User tapped, but the tap delay hasn't expired.
  kGestureDoubleTap,
  kGestureBegin,  // The first event sent when each finger is pressed.
  kGestureEnd,    // Sent for each released finger.
  kGestureTwoFingerTap,
  kGesturePinchBegin,
  kGesturePinchEnd,
  kGesturePinchUpdate,
  kGestureShortPress,
  kGestureLongPress,
  kGestureLongTap,
  // A kGestureSwipe can happen at the end of a touch sequence involving one or
  // more fingers if the finger velocity was high enough when the first finger
  // was released.
  kGestureSwipe,
  kGestureShowPress,

  // Scroll support.
  // TODO(davemoore): We need to unify these events w/ touch and gestures.
  kScroll,
  kScrollFlingStart,
  kScrollFlingCancel,
  kGestureTypeEnd = kScrollFlingCancel,

  // Sent by the system to indicate any modal type operations, such as drag and
  // drop or menus, should stop.
  kCancelMode,

  // Sent by the CrOS gesture library for interesting patterns that we want
  // to track with the UMA system.
  kUmaData,

  // Must always be last. User namespace starts above this value.
  // See ui::RegisterCustomEventType().
  kLast
};

}  // namespace ui

#endif  // UI_EVENTS_TYPES_EVENT_TYPE_H_
