// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_event_util.h"

#include <stddef.h>

#include <algorithm>
#include <bitset>
#include <limits>
#include <memory>

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_pointer_event.h"
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace ui {
namespace {

const int kInvalidTouchIndex = -1;

WebInputEvent::Type ToWebTouchEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::Action::DOWN:
      return WebInputEvent::kTouchStart;
    case MotionEvent::Action::MOVE:
      return WebInputEvent::kTouchMove;
    case MotionEvent::Action::UP:
      return WebInputEvent::kTouchEnd;
    case MotionEvent::Action::CANCEL:
      return WebInputEvent::kTouchCancel;
    case MotionEvent::Action::POINTER_DOWN:
      return WebInputEvent::kTouchStart;
    case MotionEvent::Action::POINTER_UP:
      return WebInputEvent::kTouchEnd;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::kUndefined;
}

// Note that the action index is meaningful only in the context of
// |Action::POINTER_UP| and |Action::POINTER_DOWN|; other actions map directly
// to WebTouchPoint::State.
WebTouchPoint::State ToWebTouchPointState(const MotionEvent& event,
                                          size_t pointer_index) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      return WebTouchPoint::kStatePressed;
    case MotionEvent::Action::MOVE:
      return WebTouchPoint::kStateMoved;
    case MotionEvent::Action::UP:
      return WebTouchPoint::kStateReleased;
    case MotionEvent::Action::CANCEL:
      return WebTouchPoint::kStateCancelled;
    case MotionEvent::Action::POINTER_DOWN:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::kStatePressed
                 : WebTouchPoint::kStateStationary;
    case MotionEvent::Action::POINTER_UP:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::kStateReleased
                 : WebTouchPoint::kStateStationary;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action.";
  return WebTouchPoint::kStateUndefined;
}

WebPointerProperties::PointerType ToWebPointerType(
    MotionEvent::ToolType tool_type) {
  switch (tool_type) {
    case MotionEvent::ToolType::UNKNOWN:
      return WebPointerProperties::PointerType::kUnknown;
    case MotionEvent::ToolType::FINGER:
      return WebPointerProperties::PointerType::kTouch;
    case MotionEvent::ToolType::STYLUS:
      return WebPointerProperties::PointerType::kPen;
    case MotionEvent::ToolType::MOUSE:
      return WebPointerProperties::PointerType::kMouse;
    case MotionEvent::ToolType::ERASER:
      return WebPointerProperties::PointerType::kEraser;
  }
  NOTREACHED() << "Invalid MotionEvent::ToolType = " << tool_type;
  return WebPointerProperties::PointerType::kUnknown;
}

WebPointerProperties::PointerType ToWebPointerType(
    EventPointerType event_pointer_type) {
  switch (event_pointer_type) {
    case EventPointerType::POINTER_TYPE_UNKNOWN:
      return WebPointerProperties::PointerType::kUnknown;
    case EventPointerType::POINTER_TYPE_MOUSE:
      return WebPointerProperties::PointerType::kMouse;
    case EventPointerType::POINTER_TYPE_PEN:
      return WebPointerProperties::PointerType::kPen;
    case EventPointerType::POINTER_TYPE_TOUCH:
      return WebPointerProperties::PointerType::kTouch;
    case EventPointerType::POINTER_TYPE_ERASER:
      return WebPointerProperties::PointerType::kEraser;
    default:
      NOTREACHED() << "Invalid EventPointerType = "
                   << static_cast<int>(event_pointer_type);
      return WebPointerProperties::PointerType::kUnknown;
  }
}

WebPointerProperties::Button ToWebPointerButton(int android_button_state) {
  if (android_button_state & MotionEvent::BUTTON_PRIMARY)
    return WebPointerProperties::Button::kLeft;
  else if (android_button_state & MotionEvent::BUTTON_SECONDARY)
    return WebPointerProperties::Button::kRight;
  else if (android_button_state & MotionEvent::BUTTON_TERTIARY)
    return WebPointerProperties::Button::kMiddle;
  else if (android_button_state & MotionEvent::BUTTON_BACK)
    return WebPointerProperties::Button::kBack;
  else if (android_button_state & MotionEvent::BUTTON_FORWARD)
    return WebPointerProperties::Button::kForward;
  else if (android_button_state & MotionEvent::BUTTON_STYLUS_PRIMARY)
    return WebPointerProperties::Button::kLeft;
  else if (android_button_state & MotionEvent::BUTTON_STYLUS_SECONDARY)
    return WebPointerProperties::Button::kRight;
  else
    return WebPointerProperties::Button::kNoButton;
}

WebTouchPoint CreateWebTouchPoint(const MotionEvent& event,
                                  size_t pointer_index) {
  WebTouchPoint touch;

  SetWebPointerPropertiesFromMotionEventData(
      touch, event.GetPointerId(pointer_index),
      event.GetPressure(pointer_index), event.GetOrientation(pointer_index),
      event.GetTiltX(pointer_index), event.GetTiltY(pointer_index),
      event.GetTwist(pointer_index), event.GetTangentialPressure(pointer_index),
      0 /* no button changed */, event.GetToolType(pointer_index));

  touch.state = ToWebTouchPointState(event, pointer_index);
  touch.SetPositionInWidget(event.GetX(pointer_index),
                            event.GetY(pointer_index));
  touch.SetPositionInScreen(event.GetRawX(pointer_index),
                            event.GetRawY(pointer_index));

  // A note on touch ellipse specifications:
  //
  // Android MotionEvent provides the major and minor axes of the touch ellipse,
  // as well as the orientation of the major axis clockwise from vertical, in
  // radians. See:
  // http://developer.android.com/reference/android/view/MotionEvent.html
  //
  // The proposed extension to W3C Touch Events specifies the touch ellipse
  // using two radii along x- & y-axes and a positive acute rotation angle in
  // degrees. See:
  // http://dvcs.w3.org/hg/webevents/raw-file/default/touchevents.html

  float major_radius = event.GetTouchMajor(pointer_index) / 2.f;
  float minor_radius = event.GetTouchMinor(pointer_index) / 2.f;
  float orientation_deg = gfx::RadToDeg(event.GetOrientation(pointer_index));

  DCHECK_GE(major_radius, 0);
  DCHECK_GE(minor_radius, 0);
  DCHECK_GE(major_radius, minor_radius);
  // Orientation lies in [-180, 180] for a stylus, and [-90, 90] for other
  // touchscreen inputs. There are exceptions on Android when a device is
  // rotated, yielding touch orientations in the range of [-180, 180].
  // Regardless, normalise to [-90, 90), allowing a small tolerance to account
  // for floating point conversion.
  // TODO(e_hakkinen): Also pass unaltered stylus orientation, avoiding loss of
  // quadrant information, see crbug.com/493728.
  DCHECK_GT(orientation_deg, -180.01f);
  DCHECK_LT(orientation_deg, 180.01f);
  if (orientation_deg >= 90.f)
    orientation_deg -= 180.f;
  else if (orientation_deg < -90.f)
    orientation_deg += 180.f;
  if (orientation_deg >= 0) {
    // The case orientation_deg == 0 is handled here on purpose: although the
    // 'else' block is equivalent in this case, we want to pass the 0 value
    // unchanged (and 0 is the default value for many devices that don't
    // report elliptical touches).
    touch.radius_x = minor_radius;
    touch.radius_y = major_radius;
    touch.rotation_angle = orientation_deg;
  } else {
    touch.radius_x = major_radius;
    touch.radius_y = minor_radius;
    touch.rotation_angle = orientation_deg + 90;
  }

  return touch;
}

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

// Returns |kInvalidTouchIndex| iff |event| lacks a touch with an ID of |id|.
int GetIndexOfTouchID(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].id == id)
      return i;
  }
  return kInvalidTouchIndex;
}

WebInputEvent::DispatchType MergeDispatchTypes(
    WebInputEvent::DispatchType type_1,
    WebInputEvent::DispatchType type_2) {
  static_assert(WebInputEvent::DispatchType::kBlocking <
                    WebInputEvent::DispatchType::kEventNonBlocking,
                "Enum not ordered correctly");
  static_assert(WebInputEvent::DispatchType::kEventNonBlocking <
                    WebInputEvent::DispatchType::kListenersNonBlockingPassive,
                "Enum not ordered correctly");
  static_assert(
      WebInputEvent::DispatchType::kListenersNonBlockingPassive <
          WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling,
      "Enum not ordered correctly");
  return static_cast<WebInputEvent::DispatchType>(
      std::min(static_cast<int>(type_1), static_cast<int>(type_2)));
}

bool CanCoalesce(const WebPointerEvent& event_to_coalesce,
                 const WebPointerEvent& event) {
  return (event.GetType() == WebInputEvent::kPointerMove ||
          event.GetType() == WebInputEvent::kPointerRawUpdate) &&
         event.GetType() == event_to_coalesce.GetType() &&
         event.GetModifiers() == event_to_coalesce.GetModifiers() &&
         event.id == event_to_coalesce.id &&
         event.pointer_type == event_to_coalesce.pointer_type;
}

void Coalesce(const WebPointerEvent& event_to_coalesce,
              WebPointerEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // Accumulate movement deltas.
  int x = event->movement_x;
  int y = event->movement_y;
  *event = event_to_coalesce;
  event->movement_x += x;
  event->movement_y += y;
}

bool CanCoalesce(const WebMouseEvent& event_to_coalesce,
                 const WebMouseEvent& event) {
  // Since we start supporting the stylus input and they are constructed as
  // mouse events or touch events, we should check the ID and pointer type when
  // coalescing mouse events.
  return event.GetType() == WebInputEvent::kMouseMove &&
         event.GetType() == event_to_coalesce.GetType() &&
         event.GetModifiers() == event_to_coalesce.GetModifiers() &&
         event.id == event_to_coalesce.id &&
         event.pointer_type == event_to_coalesce.pointer_type;
}

void Coalesce(const WebMouseEvent& event_to_coalesce, WebMouseEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // Accumulate movement deltas.
  int x = event->movement_x;
  int y = event->movement_y;
  *event = event_to_coalesce;
  event->movement_x += x;
  event->movement_y += y;
}

bool HaveConsistentPhase(const WebMouseWheelEvent& event_to_coalesce,
                         const WebMouseWheelEvent& event) {
  if (event.has_synthetic_phase != event_to_coalesce.has_synthetic_phase)
    return false;

  if (event.phase == event_to_coalesce.phase &&
      event.momentum_phase == event_to_coalesce.momentum_phase) {
    return true;
  }

  if (event.has_synthetic_phase) {
    // It is alright to coalesce a wheel event with synthetic phaseChanged to
    // its previous one with synthetic phaseBegan.
    return (event.phase == WebMouseWheelEvent::kPhaseBegan &&
            event_to_coalesce.phase == WebMouseWheelEvent::kPhaseChanged);
  }
  return false;
}

bool CanCoalesce(const WebMouseWheelEvent& event_to_coalesce,
                 const WebMouseWheelEvent& event) {
  return event.GetModifiers() == event_to_coalesce.GetModifiers() &&
         event.delta_units == event_to_coalesce.delta_units &&
         HaveConsistentPhase(event_to_coalesce, event) &&
         event.resending_plugin_id == event_to_coalesce.resending_plugin_id;
}

void Coalesce(const WebMouseWheelEvent& event_to_coalesce,
              WebMouseWheelEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  float unaccelerated_x =
      GetUnacceleratedDelta(event->delta_x, event->acceleration_ratio_x) +
      GetUnacceleratedDelta(event_to_coalesce.delta_x,
                            event_to_coalesce.acceleration_ratio_x);
  float unaccelerated_y =
      GetUnacceleratedDelta(event->delta_y, event->acceleration_ratio_y) +
      GetUnacceleratedDelta(event_to_coalesce.delta_y,
                            event_to_coalesce.acceleration_ratio_y);
  float old_deltaX = event->delta_x;
  float old_deltaY = event->delta_y;
  float old_wheelTicksX = event->wheel_ticks_x;
  float old_wheelTicksY = event->wheel_ticks_y;
  float old_movementX = event->movement_x;
  float old_movementY = event->movement_y;
  WebMouseWheelEvent::Phase old_phase = event->phase;
  WebInputEvent::DispatchType old_dispatch_type = event->dispatch_type;
  *event = event_to_coalesce;
  event->delta_x += old_deltaX;
  event->delta_y += old_deltaY;
  event->wheel_ticks_x += old_wheelTicksX;
  event->wheel_ticks_y += old_wheelTicksY;
  event->movement_x += old_movementX;
  event->movement_y += old_movementY;
  event->acceleration_ratio_x =
      GetAccelerationRatio(event->delta_x, unaccelerated_x);
  event->acceleration_ratio_y =
      GetAccelerationRatio(event->delta_y, unaccelerated_y);
  event->dispatch_type =
      MergeDispatchTypes(old_dispatch_type, event_to_coalesce.dispatch_type);
  if (event_to_coalesce.has_synthetic_phase &&
      event_to_coalesce.phase != old_phase) {
    // Coalesce  a wheel event with synthetic phase changed to a wheel event
    // with synthetic phase began.
    DCHECK_EQ(WebMouseWheelEvent::kPhaseChanged, event_to_coalesce.phase);
    DCHECK_EQ(WebMouseWheelEvent::kPhaseBegan, old_phase);
    event->phase = WebMouseWheelEvent::kPhaseBegan;
  }
}

bool CanCoalesce(const WebTouchEvent& event_to_coalesce,
                 const WebTouchEvent& event) {
  if (event.GetType() != event_to_coalesce.GetType() ||
      event.GetType() != WebInputEvent::kTouchMove ||
      event.GetModifiers() != event_to_coalesce.GetModifiers() ||
      event.touches_length != event_to_coalesce.touches_length ||
      event.touches_length > WebTouchEvent::kTouchesLengthCap)
    return false;

  static_assert(WebTouchEvent::kTouchesLengthCap <= sizeof(int32_t) * 8U,
                "suboptimal kTouchesLengthCap size");
  // Ensure that we have a 1-to-1 mapping of pointer ids between touches.
  std::bitset<WebTouchEvent::kTouchesLengthCap> unmatched_event_touches(
      (1 << event.touches_length) - 1);
  for (unsigned i = 0; i < event_to_coalesce.touches_length; ++i) {
    int event_touch_index =
        GetIndexOfTouchID(event, event_to_coalesce.touches[i].id);
    if (event_touch_index == kInvalidTouchIndex)
      return false;
    if (!unmatched_event_touches[event_touch_index])
      return false;
    if (event.touches[event_touch_index].pointer_type !=
        event_to_coalesce.touches[i].pointer_type)
      return false;
    unmatched_event_touches[event_touch_index] = false;
  }
  return unmatched_event_touches.none();
}

void Coalesce(const WebTouchEvent& event_to_coalesce, WebTouchEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // The WebTouchPoints include absolute position information. So it is
  // sufficient to simply replace the previous event with the new event->
  // However, it is necessary to make sure that all the points have the
  // correct state, i.e. the touch-points that moved in the last event, but
  // didn't change in the current event, will have Stationary state. It is
  // necessary to change them back to Moved state.
  WebTouchEvent old_event = *event;
  *event = event_to_coalesce;
  for (unsigned i = 0; i < event->touches_length; ++i) {
    int i_old = GetIndexOfTouchID(old_event, event->touches[i].id);
    if (old_event.touches[i_old].state == blink::WebTouchPoint::kStateMoved) {
      event->touches[i].state = blink::WebTouchPoint::kStateMoved;
      event->touches[i].movement_x += old_event.touches[i_old].movement_x;
      event->touches[i].movement_y += old_event.touches[i_old].movement_y;
    }
  }
  event->moved_beyond_slop_region |= old_event.moved_beyond_slop_region;
  event->dispatch_type = MergeDispatchTypes(old_event.dispatch_type,
                                            event_to_coalesce.dispatch_type);
  event->unique_touch_event_id = old_event.unique_touch_event_id;
}

bool CanCoalesce(const WebGestureEvent& event_to_coalesce,
                 const WebGestureEvent& event) {
  if (event.GetType() != event_to_coalesce.GetType() ||
      event.resending_plugin_id != event_to_coalesce.resending_plugin_id ||
      event.SourceDevice() != event_to_coalesce.SourceDevice() ||
      event.GetModifiers() != event_to_coalesce.GetModifiers())
    return false;

  if (event.GetType() == WebInputEvent::kGestureScrollUpdate)
    return true;

  // GesturePinchUpdate scales can be combined only if they share a focal point,
  // e.g., with double-tap drag zoom.
  // Due to the imprecision of OOPIF coordinate conversions, the positions may
  // not be exactly equal, so we only require approximate equality.
  constexpr float kAnchorTolerance = 1.f;
  if (event.GetType() == WebInputEvent::kGesturePinchUpdate &&
      (std::abs(event.PositionInWidget().x -
                event_to_coalesce.PositionInWidget().x) < kAnchorTolerance) &&
      (std::abs(event.PositionInWidget().y -
                event_to_coalesce.PositionInWidget().y) < kAnchorTolerance)) {
    return true;
  }

  return false;
}

void Coalesce(const WebGestureEvent& event_to_coalesce,
              WebGestureEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  if (event->GetType() == WebInputEvent::kGestureScrollUpdate) {
    event->data.scroll_update.delta_x +=
        event_to_coalesce.data.scroll_update.delta_x;
    event->data.scroll_update.delta_y +=
        event_to_coalesce.data.scroll_update.delta_y;
  } else if (event->GetType() == WebInputEvent::kGesturePinchUpdate) {
    event->data.pinch_update.scale *= event_to_coalesce.data.pinch_update.scale;
    // Ensure the scale remains bounded above 0 and below Infinity so that
    // we can reliably perform operations like log on the values.
    if (event->data.pinch_update.scale < numeric_limits<float>::min())
      event->data.pinch_update.scale = numeric_limits<float>::min();
    else if (event->data.pinch_update.scale > numeric_limits<float>::max())
      event->data.pinch_update.scale = numeric_limits<float>::max();
  }
}

// Returns the transform matrix corresponding to the gesture event.
gfx::Transform GetTransformForEvent(const WebGestureEvent& gesture_event) {
  gfx::Transform gesture_transform;
  if (gesture_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.data.scroll_update.delta_x,
                                gesture_event.data.scroll_update.delta_y);
  } else if (gesture_event.GetType() == WebInputEvent::kGesturePinchUpdate) {
    float scale = gesture_event.data.pinch_update.scale;
    gesture_transform.Translate(-gesture_event.PositionInWidget().x,
                                -gesture_event.PositionInWidget().y);
    gesture_transform.Scale(scale, scale);
    gesture_transform.Translate(gesture_event.PositionInWidget().x,
                                gesture_event.PositionInWidget().y);
  } else {
    NOTREACHED() << "Invalid event type for transform retrieval: "
                 << WebInputEvent::GetName(gesture_event.GetType());
  }
  return gesture_transform;
}

}  // namespace

bool CanCoalesce(const blink::WebInputEvent& event_to_coalesce,
                 const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsPointerEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsPointerEventType(event.GetType())) {
    return CanCoalesce(
        static_cast<const blink::WebPointerEvent&>(event_to_coalesce),
        static_cast<const blink::WebPointerEvent&>(event));
  }
  if (blink::WebInputEvent::IsGestureEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    return CanCoalesce(
        static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
        static_cast<const blink::WebGestureEvent&>(event));
  }
  if (blink::WebInputEvent::IsMouseEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    return CanCoalesce(
        static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseEvent&>(event));
  }
  if (blink::WebInputEvent::IsTouchEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return CanCoalesce(
        static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
        static_cast<const blink::WebTouchEvent&>(event));
  }
  if (event_to_coalesce.GetType() == blink::WebInputEvent::kMouseWheel &&
      event.GetType() == blink::WebInputEvent::kMouseWheel) {
    return CanCoalesce(
        static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseWheelEvent&>(event));
  }
  return false;
}

void Coalesce(const blink::WebInputEvent& event_to_coalesce,
              blink::WebInputEvent* event) {
  if (blink::WebInputEvent::IsPointerEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsPointerEventType(event->GetType())) {
    Coalesce(static_cast<const blink::WebPointerEvent&>(event_to_coalesce),
             static_cast<blink::WebPointerEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::IsGestureEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsGestureEventType(event->GetType())) {
    Coalesce(static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
             static_cast<blink::WebGestureEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::IsMouseEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsMouseEventType(event->GetType())) {
    Coalesce(static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::IsTouchEventType(event_to_coalesce.GetType()) &&
      blink::WebInputEvent::IsTouchEventType(event->GetType())) {
    Coalesce(static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
             static_cast<blink::WebTouchEvent*>(event));
    return;
  }
  if (event_to_coalesce.GetType() == blink::WebInputEvent::kMouseWheel &&
      event->GetType() == blink::WebInputEvent::kMouseWheel) {
    Coalesce(static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseWheelEvent*>(event));
  }
}

// Whether |event_in_queue| is a touchscreen GesturePinchUpdate or
// GestureScrollUpdate and has the same modifiers/source as the new
// scroll/pinch event. Compatible touchscreen scroll and pinch event pairs
// can be logically coalesced.
bool IsCompatibleScrollorPinch(const WebGestureEvent& new_event,
                               const WebGestureEvent& event_in_queue) {
  DCHECK(new_event.GetType() == WebInputEvent::kGestureScrollUpdate ||
         new_event.GetType() == WebInputEvent::kGesturePinchUpdate)
      << "Invalid event type for pinch/scroll coalescing: "
      << WebInputEvent::GetName(new_event.GetType());
  DLOG_IF(WARNING, new_event.TimeStamp() < event_in_queue.TimeStamp())
      << "Event time not monotonic?\n";
  return (event_in_queue.GetType() == WebInputEvent::kGestureScrollUpdate ||
          event_in_queue.GetType() == WebInputEvent::kGesturePinchUpdate) &&
         event_in_queue.GetModifiers() == new_event.GetModifiers() &&
         event_in_queue.SourceDevice() ==
             blink::WebGestureDevice::kTouchscreen &&
         new_event.SourceDevice() == blink::WebGestureDevice::kTouchscreen;
}

std::pair<WebGestureEvent, WebGestureEvent> CoalesceScrollAndPinch(
    const WebGestureEvent* second_last_event,
    const WebGestureEvent& last_event,
    const WebGestureEvent& new_event) {
  DCHECK(!CanCoalesce(new_event, last_event))
      << "New event can't be coalesced with the last event in queue directly.";
  DCHECK(IsContinuousGestureEvent(new_event.GetType()));
  DCHECK(IsCompatibleScrollorPinch(new_event, last_event));
  DCHECK(!second_last_event ||
         IsCompatibleScrollorPinch(new_event, *second_last_event));

  WebGestureEvent scroll_event(WebInputEvent::kGestureScrollUpdate,
                               new_event.GetModifiers(), new_event.TimeStamp(),
                               new_event.SourceDevice());
  WebGestureEvent pinch_event;
  scroll_event.primary_pointer_type = new_event.primary_pointer_type;
  pinch_event = scroll_event;
  pinch_event.SetType(WebInputEvent::kGesturePinchUpdate);
  pinch_event.SetPositionInWidget(new_event.GetType() ==
                                          WebInputEvent::kGesturePinchUpdate
                                      ? new_event.PositionInWidget()
                                      : last_event.PositionInWidget());

  gfx::Transform combined_scroll_pinch = GetTransformForEvent(last_event);
  if (second_last_event) {
    combined_scroll_pinch.PreconcatTransform(
        GetTransformForEvent(*second_last_event));
  }
  combined_scroll_pinch.ConcatTransform(GetTransformForEvent(new_event));

  float combined_scale =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 0));
  float combined_scroll_pinch_x =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 3));
  float combined_scroll_pinch_y =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(1, 3));
  scroll_event.data.scroll_update.delta_x =
      (combined_scroll_pinch_x + pinch_event.PositionInWidget().x) /
          combined_scale -
      pinch_event.PositionInWidget().x;
  scroll_event.data.scroll_update.delta_y =
      (combined_scroll_pinch_y + pinch_event.PositionInWidget().y) /
          combined_scale -
      pinch_event.PositionInWidget().y;
  pinch_event.data.pinch_update.scale = combined_scale;

  return std::make_pair(scroll_event, pinch_event);
}

blink::WebTouchEvent CreateWebTouchEventFromMotionEvent(
    const MotionEvent& event,
    bool moved_beyond_slop_region,
    bool hovering) {
  static_assert(static_cast<int>(MotionEvent::MAX_TOUCH_POINT_COUNT) ==
                    static_cast<int>(blink::WebTouchEvent::kTouchesLengthCap),
                "inconsistent maximum number of active touch points");

  blink::WebTouchEvent result(ToWebTouchEventType(event.GetAction()),
                              EventFlagsToWebEventModifiers(event.GetFlags()),
                              event.GetEventTime());
  result.dispatch_type = result.GetType() == WebInputEvent::kTouchCancel
                             ? WebInputEvent::kEventNonBlocking
                             : WebInputEvent::kBlocking;
  result.moved_beyond_slop_region = moved_beyond_slop_region;
  result.hovering = hovering;

  // TODO(mustaq): MotionEvent flags seems unrelated, should use
  // metaState instead?

  DCHECK_NE(event.GetUniqueEventId(), 0U);
  result.unique_touch_event_id = event.GetUniqueEventId();
  result.touches_length =
      std::min(static_cast<unsigned>(event.GetPointerCount()),
               static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap));
  DCHECK_GT(result.touches_length, 0U);

  for (size_t i = 0; i < result.touches_length; ++i)
    result.touches[i] = CreateWebTouchPoint(event, i);

  return result;
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & EF_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::kShiftKey;
  if (flags & EF_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::kControlKey;
  if (flags & EF_ALT_DOWN)
    modifiers |= blink::WebInputEvent::kAltKey;
  if (flags & EF_COMMAND_DOWN)
    modifiers |= blink::WebInputEvent::kMetaKey;
  if (flags & EF_ALTGR_DOWN)
    modifiers |= blink::WebInputEvent::kAltGrKey;
  if (flags & EF_NUM_LOCK_ON)
    modifiers |= blink::WebInputEvent::kNumLockOn;
  if (flags & EF_CAPS_LOCK_ON)
    modifiers |= blink::WebInputEvent::kCapsLockOn;
  if (flags & EF_SCROLL_LOCK_ON)
    modifiers |= blink::WebInputEvent::kScrollLockOn;
  if (flags & EF_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::kLeftButtonDown;
  if (flags & EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::kMiddleButtonDown;
  if (flags & EF_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::kRightButtonDown;
  if (flags & EF_BACK_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::kBackButtonDown;
  if (flags & EF_FORWARD_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::kForwardButtonDown;
  if (flags & EF_IS_REPEAT)
    modifiers |= blink::WebInputEvent::kIsAutoRepeat;
  if (flags & EF_TOUCH_ACCESSIBILITY)
    modifiers |= blink::WebInputEvent::kIsTouchAccessibility;

  return modifiers;
}

WebGestureEvent CreateWebGestureEvent(const GestureEventDetails& details,
                                      base::TimeTicks timestamp,
                                      const gfx::PointF& location,
                                      const gfx::PointF& raw_location,
                                      int flags,
                                      uint32_t unique_touch_event_id) {
  blink::WebGestureDevice source_device =
      blink::WebGestureDevice::kUninitialized;
  switch (details.device_type()) {
    case GestureDeviceType::DEVICE_TOUCHSCREEN:
      source_device = blink::WebGestureDevice::kTouchscreen;
      break;
    case GestureDeviceType::DEVICE_TOUCHPAD:
      source_device = blink::WebGestureDevice::kTouchpad;
      break;
    case GestureDeviceType::DEVICE_UNKNOWN:
      NOTREACHED() << "Unknown device type is not allowed";
      break;
  }
  WebGestureEvent gesture(WebInputEvent::kUndefined,
                          EventFlagsToWebEventModifiers(flags), timestamp,
                          source_device);

  gesture.SetPositionInWidget(location);
  gesture.SetPositionInScreen(raw_location);

  gesture.is_source_touch_event_set_non_blocking =
      details.is_source_touch_event_set_non_blocking();
  gesture.primary_pointer_type =
      ToWebPointerType(details.primary_pointer_type());
  gesture.unique_touch_event_id = unique_touch_event_id;

  switch (details.type()) {
    case ET_GESTURE_SHOW_PRESS:
      gesture.SetType(WebInputEvent::kGestureShowPress);
      gesture.data.show_press.width = details.bounding_box_f().width();
      gesture.data.show_press.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_DOUBLE_TAP:
      gesture.SetType(WebInputEvent::kGestureDoubleTap);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      gesture.SetNeedsWheelEvent(source_device ==
                                 blink::WebGestureDevice::kTouchpad);
      break;
    case ET_GESTURE_TAP:
      gesture.SetType(WebInputEvent::kGestureTap);
      DCHECK_GE(details.tap_count(), 1);
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_TAP_UNCONFIRMED:
      gesture.SetType(WebInputEvent::kGestureTapUnconfirmed);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_LONG_PRESS:
      gesture.SetType(WebInputEvent::kGestureLongPress);
      gesture.data.long_press.width = details.bounding_box_f().width();
      gesture.data.long_press.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_LONG_TAP:
      gesture.SetType(WebInputEvent::kGestureLongTap);
      gesture.data.long_press.width = details.bounding_box_f().width();
      gesture.data.long_press.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_TWO_FINGER_TAP:
      gesture.SetType(blink::WebInputEvent::kGestureTwoFingerTap);
      gesture.data.two_finger_tap.first_finger_width =
          details.first_finger_width();
      gesture.data.two_finger_tap.first_finger_height =
          details.first_finger_height();
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      gesture.SetType(WebInputEvent::kGestureScrollBegin);
      gesture.data.scroll_begin.pointer_count = details.touch_points();
      gesture.data.scroll_begin.delta_x_hint = details.scroll_x_hint();
      gesture.data.scroll_begin.delta_y_hint = details.scroll_y_hint();
      gesture.data.scroll_begin.delta_hint_units = details.scroll_begin_units();
      break;
    case ET_GESTURE_SCROLL_UPDATE:
      gesture.SetType(WebInputEvent::kGestureScrollUpdate);
      gesture.data.scroll_update.delta_x = details.scroll_x();
      gesture.data.scroll_update.delta_y = details.scroll_y();
      gesture.data.scroll_update.delta_units = details.scroll_update_units();
      break;
    case ET_GESTURE_SCROLL_END:
      gesture.SetType(WebInputEvent::kGestureScrollEnd);
      break;
    case ET_SCROLL_FLING_START:
      gesture.SetType(WebInputEvent::kGestureFlingStart);
      gesture.data.fling_start.velocity_x = details.velocity_x();
      gesture.data.fling_start.velocity_y = details.velocity_y();
      break;
    case ET_SCROLL_FLING_CANCEL:
      gesture.SetType(WebInputEvent::kGestureFlingCancel);
      break;
    case ET_GESTURE_PINCH_BEGIN:
      gesture.SetType(WebInputEvent::kGesturePinchBegin);
      gesture.SetNeedsWheelEvent(source_device ==
                                 blink::WebGestureDevice::kTouchpad);
      break;
    case ET_GESTURE_PINCH_UPDATE:
      gesture.SetType(WebInputEvent::kGesturePinchUpdate);
      gesture.data.pinch_update.scale = details.scale();
      gesture.SetNeedsWheelEvent(source_device ==
                                 blink::WebGestureDevice::kTouchpad);
      break;
    case ET_GESTURE_PINCH_END:
      gesture.SetType(WebInputEvent::kGesturePinchEnd);
      gesture.SetNeedsWheelEvent(source_device ==
                                 blink::WebGestureDevice::kTouchpad);
      break;
    case ET_GESTURE_TAP_CANCEL:
      gesture.SetType(WebInputEvent::kGestureTapCancel);
      break;
    case ET_GESTURE_TAP_DOWN:
      gesture.SetType(WebInputEvent::kGestureTapDown);
      gesture.data.tap_down.width = details.bounding_box_f().width();
      gesture.data.tap_down.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_BEGIN:
    case ET_GESTURE_END:
    case ET_GESTURE_SWIPE:
      // The caller is responsible for discarding these gestures appropriately.
      gesture.SetType(WebInputEvent::kUndefined);
      break;
    default:
      NOTREACHED() << "EventType provided wasn't a valid gesture event: "
                   << details.type();
  }

  return gesture;
}

WebGestureEvent CreateWebGestureEventFromGestureEventData(
    const GestureEventData& data) {
  return CreateWebGestureEvent(data.details, data.time,
                               gfx::PointF(data.x, data.y),
                               gfx::PointF(data.raw_x, data.raw_y), data.flags,
                               data.unique_touch_event_id);
}

std::unique_ptr<blink::WebInputEvent> ScaleWebInputEvent(
    const blink::WebInputEvent& event,
    float scale) {
  return TranslateAndScaleWebInputEvent(event, gfx::Vector2d(0, 0), scale);
}

std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2d& delta,
    float scale) {
  std::unique_ptr<blink::WebInputEvent> scaled_event;
  if (scale == 1.f && delta.IsZero())
    return scaled_event;
  if (event.GetType() == blink::WebMouseEvent::kMouseWheel) {
    blink::WebMouseWheelEvent* wheel_event = new blink::WebMouseWheelEvent;
    scaled_event.reset(wheel_event);
    *wheel_event = static_cast<const blink::WebMouseWheelEvent&>(event);
    float x = (wheel_event->PositionInWidget().x + delta.x()) * scale;
    float y = (wheel_event->PositionInWidget().y + delta.y()) * scale;
    wheel_event->SetPositionInWidget(x, y);
    if (wheel_event->delta_units !=
        ui::input_types::ScrollGranularity::kScrollByPage) {
      wheel_event->delta_x *= scale;
      wheel_event->delta_y *= scale;
      wheel_event->wheel_ticks_x *= scale;
      wheel_event->wheel_ticks_y *= scale;
    }
  } else if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    blink::WebMouseEvent* mouse_event = new blink::WebMouseEvent;
    scaled_event.reset(mouse_event);
    *mouse_event = static_cast<const blink::WebMouseEvent&>(event);
    float x = (mouse_event->PositionInWidget().x + delta.x()) * scale;
    float y = (mouse_event->PositionInWidget().y + delta.y()) * scale;
    mouse_event->SetPositionInWidget(x, y);
    // Do not scale movement of raw movement events.
    if (!mouse_event->is_raw_movement_event) {
      mouse_event->movement_x *= scale;
      mouse_event->movement_y *= scale;
    }
  } else if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    blink::WebTouchEvent* touch_event = new blink::WebTouchEvent;
    scaled_event.reset(touch_event);
    *touch_event = static_cast<const blink::WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event->touches_length; i++) {
      touch_event->touches[i].SetPositionInWidget(
          (touch_event->touches[i].PositionInWidget().x + delta.x()) * scale,
          (touch_event->touches[i].PositionInWidget().y + delta.y()) * scale);
      touch_event->touches[i].radius_x *= scale;
      touch_event->touches[i].radius_y *= scale;
    }
  } else if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    blink::WebGestureEvent* gesture_event = new blink::WebGestureEvent;
    scaled_event.reset(gesture_event);
    *gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    gesture_event->SetPositionInWidget(blink::WebFloatPoint(
        (gesture_event->PositionInWidget().x + delta.x()) * scale,
        (gesture_event->PositionInWidget().y + delta.y()) * scale));
    switch (gesture_event->GetType()) {
      case blink::WebInputEvent::kGestureScrollUpdate:
        if (gesture_event->data.scroll_update.delta_units ==
                ui::input_types::ScrollGranularity::kScrollByPixel ||
            gesture_event->data.scroll_update.delta_units ==
                ui::input_types::ScrollGranularity::kScrollByPrecisePixel) {
          gesture_event->data.scroll_update.delta_x *= scale;
          gesture_event->data.scroll_update.delta_y *= scale;
        }
        break;
      case blink::WebInputEvent::kGestureScrollBegin:
        if (gesture_event->data.scroll_begin.delta_hint_units ==
                ui::input_types::ScrollGranularity::kScrollByPixel ||
            gesture_event->data.scroll_begin.delta_hint_units ==
                ui::input_types::ScrollGranularity::kScrollByPrecisePixel) {
          gesture_event->data.scroll_begin.delta_x_hint *= scale;
          gesture_event->data.scroll_begin.delta_y_hint *= scale;
        }
        break;

      case blink::WebInputEvent::kGesturePinchUpdate:
        // Scale in pinch gesture is DSF agnostic.
        break;

      case blink::WebInputEvent::kGestureDoubleTap:
      case blink::WebInputEvent::kGestureTap:
      case blink::WebInputEvent::kGestureTapUnconfirmed:
        gesture_event->data.tap.width *= scale;
        gesture_event->data.tap.height *= scale;
        break;

      case blink::WebInputEvent::kGestureTapDown:
        gesture_event->data.tap_down.width *= scale;
        gesture_event->data.tap_down.height *= scale;
        break;

      case blink::WebInputEvent::kGestureShowPress:
        gesture_event->data.show_press.width *= scale;
        gesture_event->data.show_press.height *= scale;
        break;

      case blink::WebInputEvent::kGestureLongPress:
      case blink::WebInputEvent::kGestureLongTap:
        gesture_event->data.long_press.width *= scale;
        gesture_event->data.long_press.height *= scale;
        break;

      case blink::WebInputEvent::kGestureTwoFingerTap:
        gesture_event->data.two_finger_tap.first_finger_width *= scale;
        gesture_event->data.two_finger_tap.first_finger_height *= scale;
        break;

      case blink::WebInputEvent::kGestureFlingStart:
        gesture_event->data.fling_start.velocity_x *= scale;
        gesture_event->data.fling_start.velocity_y *= scale;
        break;

      // These event does not have location data.
      case blink::WebInputEvent::kGesturePinchBegin:
      case blink::WebInputEvent::kGesturePinchEnd:
      case blink::WebInputEvent::kGestureTapCancel:
      case blink::WebInputEvent::kGestureFlingCancel:
      case blink::WebInputEvent::kGestureScrollEnd:
        break;

      // TODO(oshima): Find out if ContextMenu needs to be scaled.
      default:
        break;
    }
  }
  return scaled_event;
}

WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::Action::DOWN:
    case MotionEvent::Action::BUTTON_PRESS:
      return WebInputEvent::kMouseDown;
    case MotionEvent::Action::MOVE:
    case MotionEvent::Action::HOVER_MOVE:
      return WebInputEvent::kMouseMove;
    case MotionEvent::Action::HOVER_ENTER:
      return WebInputEvent::kMouseEnter;
    case MotionEvent::Action::HOVER_EXIT:
      return WebInputEvent::kMouseLeave;
    case MotionEvent::Action::UP:
    case MotionEvent::Action::BUTTON_RELEASE:
      return WebInputEvent::kMouseUp;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::CANCEL:
    case MotionEvent::Action::POINTER_DOWN:
    case MotionEvent::Action::POINTER_UP:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::kUndefined;
}

EventType WebEventTypeToEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kMouseDown:
      return ET_MOUSE_PRESSED;
    case WebInputEvent::kMouseUp:
      return ET_MOUSE_RELEASED;
    case WebInputEvent::kMouseMove:
      return ET_MOUSE_MOVED;
    case WebInputEvent::kMouseEnter:
      return ET_MOUSE_ENTERED;
    case WebInputEvent::kMouseLeave:
      return ET_MOUSE_EXITED;
    case WebInputEvent::kContextMenu:
      return ET_UNKNOWN;
    case WebInputEvent::kMouseWheel:
      return ET_MOUSEWHEEL;
    case WebInputEvent::kRawKeyDown:
      return ET_UNKNOWN;
    case WebInputEvent::kKeyDown:
      return ET_KEY_PRESSED;
    case WebInputEvent::kKeyUp:
      return ET_KEY_RELEASED;
    case WebInputEvent::kChar:
      return ET_UNKNOWN;
    case WebInputEvent::kGestureScrollBegin:
      return ET_GESTURE_SCROLL_BEGIN;
    case WebInputEvent::kGestureScrollEnd:
      return ET_GESTURE_SCROLL_END;
    case WebInputEvent::kGestureScrollUpdate:
      return ET_GESTURE_SCROLL_UPDATE;
    case WebInputEvent::kGestureFlingStart:
      return ET_SCROLL_FLING_START;
    case WebInputEvent::kGestureFlingCancel:
      return ET_SCROLL_FLING_CANCEL;
    case WebInputEvent::kGesturePinchBegin:
      return ET_GESTURE_PINCH_BEGIN;
    case WebInputEvent::kGesturePinchEnd:
      return ET_GESTURE_PINCH_END;
    case WebInputEvent::kGesturePinchUpdate:
      return ET_GESTURE_PINCH_UPDATE;
    case WebInputEvent::kGestureTapDown:
      return ET_GESTURE_TAP_DOWN;
    case WebInputEvent::kGestureShowPress:
      return ET_GESTURE_SHOW_PRESS;
    case WebInputEvent::kGestureTap:
      return ET_GESTURE_TAP;
    case WebInputEvent::kGestureTapCancel:
      return ET_GESTURE_TAP_CANCEL;
    case WebInputEvent::kGestureLongPress:
      return ET_GESTURE_LONG_PRESS;
    case WebInputEvent::kGestureLongTap:
      return ET_GESTURE_LONG_TAP;
    case WebInputEvent::kGestureTwoFingerTap:
      return ET_GESTURE_TWO_FINGER_TAP;
    case WebInputEvent::kGestureTapUnconfirmed:
      return ET_GESTURE_TAP_UNCONFIRMED;
    case WebInputEvent::kGestureDoubleTap:
      return ET_GESTURE_DOUBLE_TAP;
    case WebInputEvent::kTouchStart:
      return ET_TOUCH_PRESSED;
    case WebInputEvent::kTouchMove:
      return ET_TOUCH_MOVED;
    case WebInputEvent::kTouchEnd:
      return ET_TOUCH_RELEASED;
    case WebInputEvent::kTouchCancel:
      return ET_TOUCH_CANCELLED;
    case WebInputEvent::kTouchScrollStarted:
    case WebInputEvent::kPointerDown:
      return ET_TOUCH_PRESSED;
    case WebInputEvent::kPointerUp:
      return ET_TOUCH_RELEASED;
    case WebInputEvent::kPointerMove:
      return ET_TOUCH_MOVED;
    case WebInputEvent::kPointerCancel:
      return ET_TOUCH_CANCELLED;
    default:
      return ET_UNKNOWN;
  }
}

void SetWebPointerPropertiesFromMotionEventData(
    WebPointerProperties& webPointerProperties,
    int pointer_id,
    float pressure,
    float orientation_rad,
    float tilt_x,
    float tilt_y,
    float twist,
    float tangential_pressure,
    int android_buttons_changed,
    MotionEvent::ToolType tool_type) {
  webPointerProperties.id = pointer_id;
  webPointerProperties.force = pressure;

  if (tool_type == MotionEvent::ToolType::STYLUS) {
    // A stylus points to a direction specified by orientation and tilts to
    // the opposite direction. Coordinate system is left-handed.
    webPointerProperties.tilt_x = tilt_x;
    webPointerProperties.tilt_y = tilt_y;
    webPointerProperties.twist = twist;
    webPointerProperties.tangential_pressure = tangential_pressure;
  } else {
    webPointerProperties.tilt_x = webPointerProperties.tilt_y = 0;
    webPointerProperties.twist = 0;
    webPointerProperties.tangential_pressure = 0;
  }

  webPointerProperties.button = ToWebPointerButton(android_buttons_changed);
  webPointerProperties.pointer_type = ToWebPointerType(tool_type);
}

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;

  if (modifiers & blink::WebInputEvent::kShiftKey)
    flags |= EF_SHIFT_DOWN;
  if (modifiers & blink::WebInputEvent::kControlKey)
    flags |= EF_CONTROL_DOWN;
  if (modifiers & blink::WebInputEvent::kAltKey)
    flags |= EF_ALT_DOWN;
  if (modifiers & blink::WebInputEvent::kAltGrKey)
    flags |= EF_ALTGR_DOWN;
  if (modifiers & blink::WebInputEvent::kMetaKey)
    flags |= EF_COMMAND_DOWN;
  if (modifiers & blink::WebInputEvent::kCapsLockOn)
    flags |= EF_CAPS_LOCK_ON;
  if (modifiers & blink::WebInputEvent::kNumLockOn)
    flags |= EF_NUM_LOCK_ON;
  if (modifiers & blink::WebInputEvent::kScrollLockOn)
    flags |= EF_SCROLL_LOCK_ON;
  if (modifiers & blink::WebInputEvent::kLeftButtonDown)
    flags |= EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kMiddleButtonDown)
    flags |= EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kRightButtonDown)
    flags |= EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kBackButtonDown)
    flags |= EF_BACK_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kForwardButtonDown)
    flags |= EF_FORWARD_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::kIsAutoRepeat)
    flags |= EF_IS_REPEAT;

  return flags;
}

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(DomCode code) {
  switch (KeycodeConverter::DomCodeToLocation(code)) {
    case DomKeyLocation::LEFT:
      return blink::WebInputEvent::kIsLeft;
    case DomKeyLocation::RIGHT:
      return blink::WebInputEvent::kIsRight;
    case DomKeyLocation::NUMPAD:
      return blink::WebInputEvent::kIsKeyPad;
    case DomKeyLocation::STANDARD:
      break;
  }
  return static_cast<blink::WebInputEvent::Modifiers>(0);
}

bool IsGestureScrollOrPinch(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::kGestureScrollBegin:
    case blink::WebGestureEvent::kGestureScrollUpdate:
    case blink::WebGestureEvent::kGestureScrollEnd:
    case blink::WebGestureEvent::kGesturePinchBegin:
    case blink::WebGestureEvent::kGesturePinchUpdate:
    case blink::WebGestureEvent::kGesturePinchEnd:
      return true;
    default:
      return false;
  }
}

bool IsGestureScroll(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::kGestureScrollBegin:
    case blink::WebGestureEvent::kGestureScrollUpdate:
    case blink::WebGestureEvent::kGestureScrollEnd:
      return true;
    default:
      return false;
  }
}

bool IsContinuousGestureEvent(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::kGestureScrollUpdate:
    case blink::WebGestureEvent::kGesturePinchUpdate:
      return true;
    default:
      return false;
  }
}

EventPointerType WebPointerTypeToEventPointerType(
    WebPointerProperties::PointerType type) {
  switch (type) {
    case WebPointerProperties::PointerType::kMouse:
      return EventPointerType::POINTER_TYPE_MOUSE;
    case WebPointerProperties::PointerType::kPen:
      return EventPointerType::POINTER_TYPE_PEN;
    case WebPointerProperties::PointerType::kEraser:
      return EventPointerType::POINTER_TYPE_ERASER;
    case WebPointerProperties::PointerType::kTouch:
      return EventPointerType::POINTER_TYPE_TOUCH;
    case WebPointerProperties::PointerType::kUnknown:
      return EventPointerType::POINTER_TYPE_UNKNOWN;
  }
  NOTREACHED() << "Invalid pointer type";
  return EventPointerType::POINTER_TYPE_UNKNOWN;
}

blink::WebGestureEvent ScrollBeginFromScrollUpdate(
    const blink::WebGestureEvent& gesture_update) {
  DCHECK(gesture_update.GetType() == WebInputEvent::kGestureScrollUpdate);

  WebGestureEvent scroll_begin(gesture_update);
  scroll_begin.SetType(WebInputEvent::kGestureScrollBegin);

  scroll_begin.data.scroll_begin.delta_x_hint =
      gesture_update.data.scroll_update.delta_x;
  scroll_begin.data.scroll_begin.delta_y_hint =
      gesture_update.data.scroll_update.delta_y;
  scroll_begin.data.scroll_begin.delta_hint_units =
      gesture_update.data.scroll_update.delta_units;
  scroll_begin.data.scroll_begin.target_viewport = false;
  scroll_begin.data.scroll_begin.inertial_phase =
      gesture_update.data.scroll_update.inertial_phase;
  scroll_begin.data.scroll_begin.synthetic = false;
  scroll_begin.data.scroll_begin.pointer_count = 0;
  scroll_begin.data.scroll_begin.scrollable_area_element_id = 0;

  return scroll_begin;
}

std::unique_ptr<blink::WebGestureEvent> GenerateInjectedScrollGesture(
    WebInputEvent::Type type,
    base::TimeTicks timestamp,
    blink::WebGestureDevice device,
    blink::WebFloatPoint position_in_widget,
    gfx::Vector2dF scroll_delta,
    input_types::ScrollGranularity granularity) {
  DCHECK(IsGestureScroll(type));
  std::unique_ptr<WebGestureEvent> generated_gesture_event =
      std::make_unique<WebGestureEvent>(type, WebInputEvent::kNoModifiers,
                                        timestamp, device);

  if (type == WebInputEvent::Type::kGestureScrollBegin) {
    // Gesture events expect the scroll delta to be flipped. Gesture events'
    // scroll deltas are interpreted as the finger's delta in relation to the
    // screen (which is the reverse of the scrolling direction).
    generated_gesture_event->data.scroll_begin.delta_x_hint = -scroll_delta.x();
    generated_gesture_event->data.scroll_begin.delta_y_hint = -scroll_delta.y();
    generated_gesture_event->data.scroll_begin.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    generated_gesture_event->data.scroll_begin.delta_hint_units = granularity;
  } else if (type == WebInputEvent::Type::kGestureScrollUpdate) {
    generated_gesture_event->data.scroll_update.delta_x = -scroll_delta.x();
    generated_gesture_event->data.scroll_update.delta_y = -scroll_delta.y();
    generated_gesture_event->data.scroll_update.inertial_phase =
        WebGestureEvent::InertialPhaseState::kNonMomentum;
    generated_gesture_event->data.scroll_update.delta_units = granularity;
  }

  generated_gesture_event->SetPositionInWidget(position_in_widget);
  return generated_gesture_event;
}

blink::WebFloatPoint PositionInWidgetFromInputEvent(
    const blink::WebInputEvent& event) {
  if (WebInputEvent::IsMouseEventType(event.GetType())) {
    return static_cast<const WebMouseEvent&>(event).PositionInWidget();
  } else if (WebInputEvent::IsGestureEventType(event.GetType())) {
    return static_cast<const WebGestureEvent&>(event).PositionInWidget();
  } else {
    return blink::WebFloatPoint(0, 0);
  }
}

#if defined(OS_ANDROID)
std::unique_ptr<WebGestureEvent> CreateWebGestureEventFromGestureEventAndroid(
    const GestureEventAndroid& event) {
  WebInputEvent::Type event_type = WebInputEvent::kUndefined;
  switch (event.type()) {
    case GESTURE_EVENT_TYPE_PINCH_BEGIN:
      event_type = WebInputEvent::kGesturePinchBegin;
      break;
    case GESTURE_EVENT_TYPE_PINCH_BY:
      event_type = WebInputEvent::kGesturePinchUpdate;
      break;
    case GESTURE_EVENT_TYPE_PINCH_END:
      event_type = WebInputEvent::kGesturePinchEnd;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_START:
      event_type = WebInputEvent::kGestureScrollBegin;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_BY:
      event_type = WebInputEvent::kGestureScrollUpdate;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_END:
      event_type = WebInputEvent::kGestureScrollEnd;
      break;
    case GESTURE_EVENT_TYPE_FLING_START:
      event_type = WebInputEvent::kGestureFlingStart;
      break;
    case GESTURE_EVENT_TYPE_FLING_CANCEL:
      event_type = WebInputEvent::kGestureFlingCancel;
      break;
    case GESTURE_EVENT_TYPE_DOUBLE_TAP:
      event_type = WebInputEvent::kGestureDoubleTap;
      break;
    default:
      NOTREACHED() << "Unknown gesture event type";
      return std::make_unique<WebGestureEvent>();
  }
  auto web_event = std::make_unique<WebGestureEvent>(
      event_type, WebInputEvent::kNoModifiers,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(event.time()));
  // NOTE: Source gesture events are synthetic ones that simulate
  // gesture from keyboard (zoom in/out) for now. Should populate Blink
  // event's fields better when extended to handle more cases.
  web_event->SetPositionInWidget(event.location());
  web_event->SetPositionInScreen(event.screen_location());
  web_event->SetSourceDevice(blink::WebGestureDevice::kTouchscreen);
  if (event.synthetic_scroll())
    web_event->SetSourceDevice(blink::WebGestureDevice::kSyntheticAutoscroll);
  if (event_type == WebInputEvent::kGesturePinchUpdate) {
    web_event->data.pinch_update.scale = event.scale();
  } else if (event_type == WebInputEvent::kGestureScrollBegin) {
    web_event->data.scroll_begin.delta_x_hint = event.delta_x();
    web_event->data.scroll_begin.delta_y_hint = event.delta_y();
    web_event->data.scroll_begin.target_viewport = event.target_viewport();
  } else if (event_type == WebInputEvent::kGestureScrollUpdate) {
    web_event->data.scroll_update.delta_x = event.delta_x();
    web_event->data.scroll_update.delta_y = event.delta_y();
  } else if (event_type == WebInputEvent::kGestureFlingStart) {
    web_event->data.fling_start.velocity_x = event.velocity_x();
    web_event->data.fling_start.velocity_y = event.velocity_y();
    web_event->data.fling_start.target_viewport = event.target_viewport();
  } else if (event_type == WebInputEvent::kGestureFlingCancel) {
    web_event->data.fling_cancel.prevent_boosting = event.prevent_boosting();
    if (event.synthetic_scroll())
      web_event->data.fling_cancel.target_viewport = true;
  } else if (event_type == WebInputEvent::kGestureDoubleTap) {
    // Set the tap count to 1 even for DoubleTap, in order to be consistent with
    // double tap behavior on a mobile viewport. See https://crbug.com/234986
    // for context.
    web_event->data.tap.tap_count = 1;
  }

  return web_event;
}
#endif

}  // namespace ui
