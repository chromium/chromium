// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_traits.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"

using base::StringAppendF;
using base::SStringPrintf;
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
                event.is_system_key, reinterpret_cast<const char*>(event.text),
                reinterpret_cast<const char*>(event.unmodified_text));
}

void ApppendEventDetails(const WebMouseEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Button: %d\n Pos: (%f, %f)\n"
                " GlobalPos: (%f, %f)\n Movement: (%d, %d)\n Clicks: %d\n}",
                static_cast<int>(event.button), event.PositionInWidget().x,
                event.PositionInWidget().y, event.PositionInScreen().x,
                event.PositionInScreen().y, event.movement_x, event.movement_y,
                event.click_count);
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
                " RawData: (%f, %f, %f, %f)\n}",
                event.PositionInWidget().x, event.PositionInWidget().y,
                event.PositionInScreen().x, event.PositionInScreen().y,
                event.SourceDevice(), event.data.scroll_update.delta_x,
                event.data.scroll_update.delta_y,
                event.data.scroll_update.velocity_x,
                event.data.scroll_update.velocity_y);
}

void ApppendTouchPointDetails(const WebTouchPoint& point, std::string* result) {
  StringAppendF(result,
                "  (ID: %d, State: %d, ScreenPos: (%f, %f), Pos: (%f, %f),"
                " Radius: (%f, %f), Rot: %f, Force: %f,"
                " Tilt: (%d, %d), Twist: %d, TangentialPressure: %f),\n",
                point.id, point.state, point.PositionInScreen().x,
                point.PositionInScreen().y, point.PositionInWidget().x,
                point.PositionInWidget().y, point.radius_x, point.radius_y,
                point.rotation_angle, point.force, point.tilt_x, point.tilt_y,
                point.twist, point.tangential_pressure);
}

void ApppendEventDetails(const WebTouchEvent& event, std::string* result) {
  StringAppendF(result,
                "{\n Touches: %u, DispatchType: %d, CausesScrolling: %d,"
                " Hovering: %d, uniqueTouchEventId: %u\n[\n",
                event.touches_length, event.dispatch_type,
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
      "(%d, %d)\n}",
      event.id, static_cast<int>(event.button), event.PositionInWidget().x,
      event.PositionInWidget().y, event.PositionInScreen().x,
      event.PositionInScreen().y, event.movement_x, event.movement_y,
      event.width, event.height, event.force, event.tangential_pressure,
      event.rotation_angle, event.tilt_x, event.tilt_y);
}

struct WebInputEventDelete {
  template <class EventType>
  bool Execute(WebInputEvent* event, void*) const {
    if (!event)
      return false;
    DCHECK_EQ(sizeof(EventType), event->size());
    delete static_cast<EventType*>(event);
    return true;
  }
};

struct WebInputEventToString {
  template <class EventType>
  bool Execute(const WebInputEvent& event, std::string* result) const {
    SStringPrintf(result, "%s (Time: %lf, Modifiers: %d)\n",
                  WebInputEvent::GetName(event.GetType()),
                  event.TimeStamp().since_origin().InSecondsF(),
                  event.GetModifiers());
    const EventType& typed_event = static_cast<const EventType&>(event);
    ApppendEventDetails(typed_event, result);
    return true;
  }
};

struct WebInputEventSize {
  template <class EventType>
  bool Execute(WebInputEvent::Type /* type */, size_t* type_size) const {
    *type_size = sizeof(EventType);
    return true;
  }
};

struct WebInputEventClone {
  template <class EventType>
  bool Execute(const WebInputEvent& event,
               WebScopedInputEvent* scoped_event) const {
    DCHECK_EQ(sizeof(EventType), event.size());
    *scoped_event = WebScopedInputEvent(
        new EventType(static_cast<const EventType&>(event)));
    return true;
  }
};

template <typename Operator, typename ArgIn, typename ArgOut>
bool Apply(Operator op,
           WebInputEvent::Type type,
           const ArgIn& arg_in,
           ArgOut* arg_out) {
  if (WebInputEvent::IsPointerEventType(type))
    return op.template Execute<WebPointerEvent>(arg_in, arg_out);
  else if (WebInputEvent::IsMouseEventType(type))
    return op.template Execute<WebMouseEvent>(arg_in, arg_out);
  else if (type == WebInputEvent::kMouseWheel)
    return op.template Execute<WebMouseWheelEvent>(arg_in, arg_out);
  else if (WebInputEvent::IsKeyboardEventType(type))
    return op.template Execute<WebKeyboardEvent>(arg_in, arg_out);
  else if (WebInputEvent::IsTouchEventType(type))
    return op.template Execute<WebTouchEvent>(arg_in, arg_out);
  else if (WebInputEvent::IsGestureEventType(type))
    return op.template Execute<WebGestureEvent>(arg_in, arg_out);

  NOTREACHED() << "Unknown webkit event type " << type;
  return false;
}

}  // namespace

void WebInputEventDeleter::operator()(WebInputEvent* event) const {
  if (!event)
    return;
  void* temp = nullptr;
  Apply(WebInputEventDelete(), event->GetType(), event, temp);
}

std::string WebInputEventTraits::ToString(const WebInputEvent& event) {
  std::string result;
  Apply(WebInputEventToString(), event.GetType(), event, &result);
  return result;
}

size_t WebInputEventTraits::GetSize(WebInputEvent::Type type) {
  size_t size = 0;
  Apply(WebInputEventSize(), type, type, &size);
  return size;
}

WebScopedInputEvent WebInputEventTraits::Clone(const WebInputEvent& event) {
  WebScopedInputEvent scoped_event;
  Apply(WebInputEventClone(), event.GetType(), event, &scoped_event);
  return scoped_event;
}

bool WebInputEventTraits::ShouldBlockEventStream(const WebInputEvent& event) {
  switch (event.GetType()) {
    case WebInputEvent::kContextMenu:
    case WebInputEvent::kGestureScrollEnd:
    case WebInputEvent::kGestureShowPress:
    case WebInputEvent::kGestureTapUnconfirmed:
    case WebInputEvent::kGestureTapDown:
    case WebInputEvent::kGestureTapCancel:
    case WebInputEvent::kGesturePinchBegin:
    case WebInputEvent::kGesturePinchUpdate:
    case WebInputEvent::kGesturePinchEnd:
      return false;

    case WebInputEvent::kGestureScrollBegin:
      return true;

    // TouchCancel and TouchScrollStarted should always be non-blocking.
    case WebInputEvent::kTouchCancel:
    case WebInputEvent::kTouchScrollStarted:
      DCHECK_NE(WebInputEvent::kBlocking,
                static_cast<const WebTouchEvent&>(event).dispatch_type);
      return false;

    // Touch start and touch end indicate whether they are non-blocking
    // (aka uncancelable) on the event.
    case WebInputEvent::kTouchStart:
    case WebInputEvent::kTouchEnd:
      return static_cast<const WebTouchEvent&>(event).dispatch_type ==
             WebInputEvent::kBlocking;

    case WebInputEvent::kTouchMove:
      // Non-blocking touch moves can be ack'd right away.
      return static_cast<const WebTouchEvent&>(event).dispatch_type ==
             WebInputEvent::kBlocking;

    case WebInputEvent::kMouseWheel:
      return static_cast<const WebMouseWheelEvent&>(event).dispatch_type ==
             WebInputEvent::kBlocking;

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

// static
LatencyInfo WebInputEventTraits::CreateLatencyInfoForWebGestureEvent(
    const WebGestureEvent& event) {
  SourceEventType source_event_type = SourceEventType::UNKNOWN;
  if (event.SourceDevice() == blink::WebGestureDevice::kTouchpad) {
    source_event_type = SourceEventType::WHEEL;
    if (event.GetType() >= blink::WebInputEvent::kGesturePinchTypeFirst &&
        event.GetType() <= blink::WebInputEvent::kGesturePinchTypeLast) {
      source_event_type = SourceEventType::TOUCHPAD;
    }
  } else if (event.SourceDevice() == blink::WebGestureDevice::kTouchscreen) {
    blink::WebGestureEvent::InertialPhaseState inertial_phase_state =
        blink::WebGestureEvent::InertialPhaseState::kUnknownMomentum;

    switch (event.GetType()) {
      case blink::WebInputEvent::kGestureScrollBegin:
        inertial_phase_state = event.data.scroll_begin.inertial_phase;
        break;
      case blink::WebInputEvent::kGestureScrollUpdate:
        inertial_phase_state = event.data.scroll_update.inertial_phase;
        break;
      case blink::WebInputEvent::kGestureScrollEnd:
        inertial_phase_state = event.data.scroll_end.inertial_phase;
        break;
      default:
        break;
    }
    bool is_in_inertial_phase =
        inertial_phase_state ==
        blink::WebGestureEvent::InertialPhaseState::kMomentum;
    source_event_type = is_in_inertial_phase ? SourceEventType::INERTIAL
                                             : SourceEventType::TOUCH;
  }
  LatencyInfo latency_info(source_event_type);
  return latency_info;
}

}  // namespace ui
