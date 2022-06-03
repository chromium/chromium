// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_

#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class ScrollEvent;

// If a valid event cannot be created, the returned event type will be UNKNOWN.
blink::WebMouseEvent MakeWebMouseEvent(const MouseEvent& event);
blink::WebMouseWheelEvent MakeWebMouseWheelEvent(const MouseWheelEvent& event);
blink::WebMouseWheelEvent MakeWebMouseWheelEvent(const ScrollEvent& event);
blink::WebKeyboardEvent MakeWebKeyboardEvent(const KeyEvent& event);
blink::WebGestureEvent MakeWebGestureEvent(const GestureEvent& event);
blink::WebGestureEvent MakeWebGestureEvent(const ScrollEvent& event);
blink::WebGestureEvent MakeWebGestureEventFlingCancel(
    const blink::WebMouseWheelEvent& wheel_event);

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_
