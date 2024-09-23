// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_traits.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

using base::StringAppendF;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace ui {
namespace {

void ApppendEventDetails(const WebKeyboardEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n WinCode: %d\n NativeCode: %d\n IsSystem: %d\n"
                " Text: %s\n UnmodifiedText: %s\n}",
                event.windows_key_code, event.native_key_code,
                event.is_system_key,
                reinterpret_cast<const char*>(event.text.data()),
                reinterpret_cast<const char*>(event.unmodified_text.data()));
}

void ApppendEventDetails(const WebMouseEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Button: %d\n Pos: (%f, %f)\n"
                " GlobalPos: (%f, %f)\n Movement: (%d, %d)\n Clicks: %d\n}",
                static_cast<int>(event.button), event.PositionInWidget().x(),
                event.PositionInWidget().y(), event.PositionInScreen().x(),
                event.PositionInScreen().y(), event.movement_x,
                event.movement_y, event.click_count);
}

void ApppendEventDetails(const WebMouseWheelEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Delta: (%f, %f)\n WheelTicks: (%f, %f)\n Accel: (%f, %f)\n"
                " DeltaUnits: %d\n Phase: (%d, %d)",
                event.delta_x, event.delta_y, event.wheel_ticks_x,
                event.wheel_ticks_y, event.acceleration_ratio_x,
                event.acceleration_ratio_y, static_cast<int>(event.delta_units),
                event.phase, event.momentum_phase);
}

void ApppendEventDetails(const WebGestureEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Pos: (%f, %f)\n GlobalPos: (%f, %f)\n SourceDevice: %d\n"
                " RawData: (%f, %f)\n}",
                event.PositionInWidget().x(), event.PositionInWidget().y(),
                event.PositionInScreen().x(), event.PositionInScreen().y(),
                static_cast<int>(event.SourceDevice()),
                event.data.scroll_update.delta_x,
                event.data.scroll_update.delta_y);
}

void ApppendTouchPointDetails(const WebTouchPoint& point, std::string* result) {
  StringAppendF(result,
                "  (ID: %d, State: %d, ScreenPos: (%f, %f), Pos: (%f, %f),"
                " Radius: (%f, %f), Rot: %f, Force: %f,"
                " Tilt: (%f, %f), Twist: %d, TangentialPressure: %f),\n",
                point.id, static_cast<int>(point.state),
                point.PositionInScreen().x(), point.PositionInScreen().y(),
                point.PositionInWidget().x(), point.PositionInWidget().y(),
                point.radius_x, point.radius_y, point.rotation_angle,
                point.force, point.tilt_x, point.tilt_y, point.twist,
                point.tangential_pressure);
}

void ApppendEventDetails(const WebTouchEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Touches: %u, DispatchType: %d, CausesScrolling: %d,"
                " Hovering: %d, uniqueTouchEventId: %u\n[\n",
                event.touches_length, static_cast<int>(event.dispatch_type),
                event.moved_beyond_slop_region, event.hovering,
                event.unique_touch_event_id);
  for (unsigned i = 0; i < event.touches_length; ++i)
    ApppendTouchPointDetails(event.touches[i], result);
  result->append(" ]\n}");
}

void ApppendEventDetails(const WebPointerEvent& event, std::string* result) {
  StringAppendF(
      result,
      "{\n Id: %d\n Button: %d\n Pos: (%f, %f)\n"
      " GlobalPos: (%f, %f)\n Movement: (%d, %d)\n width: %f\n height: "
      "%f\n Pressure: %f\n TangentialPressure: %f\n Rotation: %f\n Tilt: "
      "(%f, %f)\n}",
      event.id, static_cast<int>(event.button), event.PositionInWidget().x(),
      event.PositionInWidget().y(), event.PositionInScreen().x(),
      event.PositionInScreen().y(), event.movement_x, event.movement_y,
      event.width, event.height, event.force, event.tangential_pressure,
      event.rotation_angle, event.tilt_x, event.tilt_y);
}

template <typename EventType>
std::string Execute(const WebInputEvent& event) {
  std::string result = base::StringPrintf(
      "%s (Time: %lf, Modifiers: %d)\n",
      WebInputEvent::GetName(event.GetType()),
      event.TimeStamp().since_origin().InSecondsF(), event.GetModifiers());
  ApppendEventDetails(static_cast<const EventType&>(event), &result);
  return result;
}

}  // namespace

std::string WebInputEventTraits::ToString(const WebInputEvent& event) {
  const WebInputEvent::Type type = event.GetType();
  if (WebInputEvent::IsPointerEventType(type)) {
    return Execute<WebPointerEvent>(event);
  }
  if (WebInputEvent::IsMouseEventType(type)) {
    return Execute<WebMouseEvent>(event);
  }
  if (type == WebInputEvent::Type::kMouseWheel) {
    return Execute<WebMouseWheelEvent>(event);
  }
  if (WebInputEvent::IsKeyboardEventType(type)) {
    return Execute<WebKeyboardEvent>(event);
  }
  if (WebInputEvent::IsTouchEventType(type)) {
    return Execute<WebTouchEvent>(event);
  }
  CHECK(WebInputEvent::IsGestureEventType(type));
  return Execute<WebGestureEvent>(event);
}

bool WebInputEventTraits::ShouldBlockEventStream(const WebInputEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::Type::kContextMenu:
    case WebInputEvent::Type::kGestureScrollEnd:
    case WebInputEvent::Type::kGestureShowPress:
    case WebInputEvent::Type::kGestureTapUnconfirmed:
    case WebInputEvent::Type::kGestureTapDown:
    case WebInputEvent::Type::kGestureTapCancel:
    case WebInputEvent::Type::kGesturePinchBegin:
    case WebInputEvent::Type::kGesturePinchUpdate:
    case WebInputEvent::Type::kGesturePinchEnd:
      return false;

    case WebInputEvent::Type::kGestureScrollBegin:
      return true;

    // TouchCancel and TouchScrollStarted should always be non-blocking.
    case WebInputEvent::Type::kTouchCancel:
    case WebInputEvent::Type::kTouchScrollStarted:
      DCHECK_NE(WebInputEvent::DispatchType::kBlocking,
                static_cast<const WebTouchEvent&>(event).dispatch_type);
      return false;

    // Touch start and touch end indicate whether they are non-blocking
    // (aka uncancelable) on the event.
    case WebInputEvent::Type::kTouchStart:
    case WebInputEvent::Type::kTouchEnd:
      return static_cast<const WebTouchEvent&>(event).dispatch_type ==
             WebInputEvent::DispatchType::kBlocking;

    case WebInputEvent::Type::kTouchMove:
      // Non-blocking touch moves can be ack'd right away.
      return static_cast<const WebTouchEvent&>(event).dispatch_type ==
             WebInputEvent::DispatchType::kBlocking;

    case WebInputEvent::Type::kMouseWheel:
      return static_cast<const WebMouseWheelEvent&>(event).dispatch_type ==
             WebInputEvent::DispatchType::kBlocking;

    default:
      return true;
  }
}

uint32_t WebInputEventTraits::GetUniqueTouchEventId(
    const WebInputEvent& event) {
  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    return static_cast<const WebTouchEvent&>(event).unique_touch_event_id;
  }
  return 0U;
}

}  // namespace ui
