// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_

#include <windows.h>

#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"

namespace ui {

class WebMouseEventBuilder {
 public:
  static blink::WebMouseEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      base::TimeTicks time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

class WebMouseWheelEventBuilder {
 public:
  static blink::WebMouseWheelEvent Build(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      base::TimeTicks time_stamp,
      blink::WebPointerProperties::PointerType pointer_type);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_BUILDERS_WIN_H_
