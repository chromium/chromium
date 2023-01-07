// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_GESTURE_EVENT_TYPE_H_
#define UI_EVENTS_ANDROID_GESTURE_EVENT_TYPE_H_

namespace ui {

// This file contains a list of GestureEventType's usable by GestureEventAndroid
// providing a direct mapping to and from their corresponding
// blink::WebGestureEvent types.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
enum GestureEventType {
  GESTURE_EVENT_TYPE_SHOW_PRESS,
  GESTURE_EVENT_TYPE_DOUBLE_TAP,
  GESTURE_EVENT_TYPE_SINGLE_TAP_UP,
  GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED,
  GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED,
  GESTURE_EVENT_TYPE_LONG_PRESS,
  GESTURE_EVENT_TYPE_SCROLL_START,
  GESTURE_EVENT_TYPE_SCROLL_BY,
  GESTURE_EVENT_TYPE_SCROLL_END,
  GESTURE_EVENT_TYPE_FLING_START,
  GESTURE_EVENT_TYPE_FLING_CANCEL,
  GESTURE_EVENT_TYPE_FLING_END,
  GESTURE_EVENT_TYPE_PINCH_BEGIN,
  GESTURE_EVENT_TYPE_PINCH_BY,
  GESTURE_EVENT_TYPE_PINCH_END,
  GESTURE_EVENT_TYPE_TAP_CANCEL,
  GESTURE_EVENT_TYPE_LONG_TAP,
  GESTURE_EVENT_TYPE_TAP_DOWN,
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_GESTURE_EVENT_TYPE_H_
