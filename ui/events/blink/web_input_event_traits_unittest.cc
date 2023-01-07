// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_traits.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace ui {
namespace {

using WebInputEventTraitsTest = testing::Test;

// Very basic smoke test to ensure stringification doesn't explode.
TEST_F(WebInputEventTraitsTest, ToString) {
  WebKeyboardEvent key(WebInputEvent::Type::kRawKeyDown,
                       WebInputEvent::kNoModifiers,
                       WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_FALSE(WebInputEventTraits::ToString(key).empty());

  WebMouseEvent mouse(WebInputEvent::Type::kMouseMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_FALSE(WebInputEventTraits::ToString(mouse).empty());

  WebMouseWheelEvent mouse_wheel(WebInputEvent::Type::kMouseWheel,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_FALSE(WebInputEventTraits::ToString(mouse_wheel).empty());

  WebGestureEvent gesture(WebInputEvent::Type::kGesturePinchBegin,
                          WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests());
  gesture.SetPositionInWidget(gfx::PointF(1, 1));
  EXPECT_FALSE(WebInputEventTraits::ToString(gesture).empty());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.touches_length = 1;
  EXPECT_FALSE(WebInputEventTraits::ToString(touch).empty());
}

}  // namespace
}  // namespace ui
