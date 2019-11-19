// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
#define UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/types/scroll_types.h"

namespace gfx {
class PointF;
class Vector2d;
}

namespace ui {
enum class DomCode;
class GestureEventAndroid;
struct GestureEventData;
struct GestureEventDetails;
class MotionEvent;

bool CanCoalesce(const blink::WebInputEvent& event_to_coalesce,
                 const blink::WebInputEvent& event);

void Coalesce(const blink::WebInputEvent& event_to_coalesce,
              blink::WebInputEvent* event);

bool IsCompatibleScrollorPinch(const blink::WebGestureEvent& new_event,
                               const blink::WebGestureEvent& event_in_queue);

// Coalesces 3 GestureScroll/PinchUpdate into 2 events.
// Returns <GestureScrollUpdate, GesturePinchUpdate>.
std::pair<blink::WebGestureEvent, blink::WebGestureEvent>
CoalesceScrollAndPinch(const blink::WebGestureEvent* second_last_event,
                       const blink::WebGestureEvent& last_event,
                       const blink::WebGestureEvent& new_event);

blink::WebTouchEvent CreateWebTouchEventFromMotionEvent(
    const MotionEvent& event,
    bool may_cause_scrolling,
    bool hovering);

blink::WebGestureEvent CreateWebGestureEvent(const GestureEventDetails& details,
                                             base::TimeTicks timestamp,
                                             const gfx::PointF& location,
                                             const gfx::PointF& raw_location,
                                             int flags,
                                             uint32_t unique_touch_event_id);

// Convenience wrapper for |CreateWebGestureEvent| using the supplied |data|.
blink::WebGestureEvent CreateWebGestureEventFromGestureEventData(
    const GestureEventData& data);

int EventFlagsToWebEventModifiers(int flags);

std::unique_ptr<blink::WebInputEvent> ScaleWebInputEvent(
    const blink::WebInputEvent& event,
    float scale);

// Transforms coordinates and other properties of |event|, by
// 1) translating / shifting by |delta| and
// 2) scaling by |scale|.
// If |event| does not need to change, returns nullptr.
// Otherwise, returns the transformed version of |event|.
std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2d& delta,
    float scale);

blink::WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action);

EventType WebEventTypeToEventType(blink::WebInputEvent::Type type);

void SetWebPointerPropertiesFromMotionEventData(
    blink::WebPointerProperties& webPointerProperties,
    int pointer_id,
    float pressure,
    float orientation_rad,
    float tilt_x,
    float tilt_y,
    float twist,
    float tangential_pressure,
    int android_buttons_changed,
    MotionEvent::ToolType tool_type);

int WebEventModifiersToEventFlags(int modifiers);

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code);

bool IsGestureScrollOrPinch(blink::WebInputEvent::Type);

bool IsGestureScroll(blink::WebInputEvent::Type);

bool IsContinuousGestureEvent(blink::WebInputEvent::Type);

EventPointerType WebPointerTypeToEventPointerType(
    blink::WebPointerProperties::PointerType type);

inline const blink::WebGestureEvent& ToWebGestureEvent(
    const blink::WebInputEvent& event) {
  DCHECK(blink::WebInputEvent::IsGestureEventType(event.GetType()));
  return static_cast<const blink::WebGestureEvent&>(event);
}

blink::WebGestureEvent ScrollBeginFromScrollUpdate(
    const blink::WebGestureEvent& scroll_update);

// Generate a scroll gesture event (begin, update, or end), based on the
// parameters passed in. Populates the data field of the created
// WebGestureEvent based on the type.
std::unique_ptr<blink::WebGestureEvent> GenerateInjectedScrollGesture(
    blink::WebInputEvent::Type type,
    base::TimeTicks timestamp,
    blink::WebGestureDevice device,
    blink::WebFloatPoint position_in_widget,
    gfx::Vector2dF scroll_delta,
    input_types::ScrollGranularity granularity);

// Returns the position in the widget if it exists for the passed in event type
blink::WebFloatPoint PositionInWidgetFromInputEvent(
    const blink::WebInputEvent& event);

#if defined(OS_ANDROID)
// Convenience method that converts an instance to blink event.
std::unique_ptr<blink::WebGestureEvent>
CreateWebGestureEventFromGestureEventAndroid(const GestureEventAndroid& event);
#endif

}  // namespace ui

#endif  // UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
