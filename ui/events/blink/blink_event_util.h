// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
#define UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_

#include <memory>

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/events/types/scroll_types.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace gfx {
class PointF;
class Vector2dF;
}

namespace ui {
enum class DomCode : uint32_t;
class GestureEventAndroid;
struct GestureEventData;
struct GestureEventDetails;
class MotionEvent;

// The scroll percentage per mousewheel tick. Used to determine scroll delta
// if percent based scrolling is enabled.
const float kScrollPercentPerLineOrChar = 0.05f;

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
    float scale,
    std::optional<int64_t> trace_id = std::nullopt);

// Transforms coordinates and other properties of |event|, by
// 1) translating / shifting by |delta| and
// 2) scaling by |scale|.
// If |event| does not need to change, returns nullptr.
// Otherwise, returns the transformed version of |event|.
std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2dF& delta,
    float scale,
    std::optional<int64_t> trace_id = std::nullopt);

blink::WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action);

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

float IfNanUseMaxFloat(float value);

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(
    ui::DomCode code);

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

#if BUILDFLAG(IS_ANDROID)
// Convenience method that converts an instance to blink event.
std::unique_ptr<blink::WebGestureEvent>
CreateWebGestureEventFromGestureEventAndroid(const GestureEventAndroid& event);
#endif

}  // namespace ui

#endif  // UI_EVENTS_BLINK_BLINK_EVENT_UTIL_H_
