// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_

#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/latency/latency_info.h"

namespace blink {
class WebGestureEvent;
}

namespace ui {

using WebScopedInputEvent = std::unique_ptr<blink::WebInputEvent>;

// Utility class for performing operations on and with WebInputEvents.
class WebInputEventTraits {
 public:
  static std::string ToString(const blink::WebInputEvent& event);
  static bool ShouldBlockEventStream(const blink::WebInputEvent& event);

  // Return uniqueTouchEventId for WebTouchEvent, otherwise return 0.
  static uint32_t GetUniqueTouchEventId(const blink::WebInputEvent& event);
  static LatencyInfo CreateLatencyInfoForWebGestureEvent(
      const blink::WebGestureEvent& event);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_TRAITS_H_
