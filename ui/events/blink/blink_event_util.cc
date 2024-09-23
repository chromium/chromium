// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_event_util.h"

#include <stddef.h>

#include <algorithm>
#include <bitset>
#include <limits>
#include <memory>

#include "base/numerics/angle_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/types/event_type.h"
#include "ui/events/velocity_tracker/motion_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/events/android/gesture_event_android.h"
#include "ui/events/android/gesture_event_type.h"
#endif

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace ui {
namespace {

WebInputEvent::Type ToWebTouchEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::Action::DOWN:
      return WebInputEvent::Type::kTouchStart;
    case MotionEvent::Action::MOVE:
      return WebInputEvent::Type::kTouchMove;
    case MotionEvent::Action::UP:
      return WebInputEvent::Type::kTouchEnd;
    case MotionEvent::Action::CANCEL:
      return WebInputEvent::Type::kTouchCancel;
    case MotionEvent::Action::POINTER_DOWN:
      return WebInputEvent::Type::kTouchStart;
    case MotionEvent::Action::POINTER_UP:
      return WebInputEvent::Type::kTouchEnd;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::Type::kUndefined;
}

// Note that the action index is meaningful only in the context of
// |Action::POINTER_UP| and |Action::POINTER_DOWN|; other actions map directly
// to WebTouchPoint::State.
WebTouchPoint::State ToWebTouchPointState(const MotionEvent& event,
                                          size_t pointer_index) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      return WebTouchPoint::State::kStatePressed;
    case MotionEvent::Action::MOVE:
      return WebTouchPoint::State::kStateMoved;
    case MotionEvent::Action::UP:
      return WebTouchPoint::State::kStateReleased;
    case MotionEvent::Action::CANCEL:
      return WebTouchPoint::State::kStateCancelled;
    case MotionEvent::Action::POINTER_DOWN:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::State::kStatePressed
                 : WebTouchPoint::State::kStateStationary;
    case MotionEvent::Action::POINTER_UP:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::State::kStateReleased
                 : WebTouchPoint::State::kStateStationary;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::HOVER_ENTER:
    case MotionEvent::Action::HOVER_EXIT:
    case MotionEvent::Action::HOVER_MOVE:
    case MotionEvent::Action::BUTTON_PRESS:
    case MotionEvent::Action::BUTTON_RELEASE:
      break;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid MotionEvent::Action.";
  return WebTouchPoint::State::kStateUndefined;
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
  NOTREACHED_IN_MIGRATION() << "Invalid MotionEvent::ToolType = " << tool_type;
  return WebPointerProperties::PointerType::kUnknown;
}

WebPointerProperties::PointerType ToWebPointerType(
    EventPointerType event_pointer_type) {
  switch (event_pointer_type) {
    case EventPointerType::kUnknown:
      return WebPointerProperties::PointerType::kUnknown;
    case EventPointerType::kMouse:
      return WebPointerProperties::PointerType::kMouse;
    case EventPointerType::kPen:
      return WebPointerProperties::PointerType::kPen;
    case EventPointerType::kTouch:
      return WebPointerProperties::PointerType::kTouch;
    case EventPointerType::kEraser:
      return WebPointerProperties::PointerType::kEraser;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid EventPointerType = "
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
  touch.device_id = event.GetSourceDeviceId(pointer_index);

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
  float orientation_deg = base::RadToDeg(event.GetOrientation(pointer_index));

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

}  // namespace

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
  result.dispatch_type = result.GetType() == WebInputEvent::Type::kTouchCancel
                             ? WebInputEvent::DispatchType::kEventNonBlocking
                             : WebInputEvent::DispatchType::kBlocking;
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
  WebGestureDevice source_device = WebGestureDevice::kUninitialized;
  switch (details.device_type()) {
    case GestureDeviceType::DEVICE_TOUCHSCREEN:
      source_device = WebGestureDevice::kTouchscreen;
      break;
    case GestureDeviceType::DEVICE_TOUCHPAD:
      source_device = WebGestureDevice::kTouchpad;
      break;
    case GestureDeviceType::DEVICE_UNKNOWN:
      NOTREACHED_IN_MIGRATION() << "Unknown device type is not allowed";
      break;
  }
  WebGestureEvent gesture(WebInputEvent::Type::kUndefined,
                          EventFlagsToWebEventModifiers(flags), timestamp,
                          source_device);

  gesture.SetPositionInWidget(location);
  gesture.SetPositionInScreen(raw_location);

  gesture.is_source_touch_event_set_blocking =
      details.is_source_touch_event_set_blocking();
  gesture.primary_pointer_type =
      ToWebPointerType(details.primary_pointer_type());
  gesture.primary_unique_touch_event_id =
      details.primary_unique_touch_event_id();
  gesture.unique_touch_event_id = unique_touch_event_id;
  gesture.GetModifiableEventLatencyMetadata() =
      details.GetEventLatencyMetadata();

  switch (details.type()) {
    case EventType::kGestureShowPress:
      gesture.SetType(WebInputEvent::Type::kGestureShowPress);
      gesture.data.show_press.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.show_press.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureDoubleTap:
      gesture.SetType(WebInputEvent::Type::kGestureDoubleTap);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.tap.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      gesture.SetNeedsWheelEvent(source_device == WebGestureDevice::kTouchpad);
      break;
    case EventType::kGestureTap:
      gesture.SetType(WebInputEvent::Type::kGestureTap);
      DCHECK_GE(details.tap_count(), 1);
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.tap.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureTapUnconfirmed:
      gesture.SetType(WebInputEvent::Type::kGestureTapUnconfirmed);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tap_count = details.tap_count();
      gesture.data.tap.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.tap.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureShortPress:
      gesture.SetType(WebInputEvent::Type::kGestureShortPress);
      gesture.data.long_press.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.long_press.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureLongPress:
      gesture.SetType(WebInputEvent::Type::kGestureLongPress);
      gesture.data.long_press.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.long_press.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureLongTap:
      gesture.SetType(WebInputEvent::Type::kGestureLongTap);
      gesture.data.long_press.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.long_press.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureTwoFingerTap:
      gesture.SetType(blink::WebInputEvent::Type::kGestureTwoFingerTap);
      gesture.data.two_finger_tap.first_finger_width =
          IfNanUseMaxFloat(details.first_finger_width());
      gesture.data.two_finger_tap.first_finger_height =
          IfNanUseMaxFloat(details.first_finger_height());
      break;
    case EventType::kGestureScrollBegin:
      gesture.SetType(WebInputEvent::Type::kGestureScrollBegin);
      gesture.data.scroll_begin.pointer_count = details.touch_points();
      gesture.data.scroll_begin.delta_x_hint =
          IfNanUseMaxFloat(details.scroll_x_hint());
      gesture.data.scroll_begin.delta_y_hint =
          IfNanUseMaxFloat(details.scroll_y_hint());
      gesture.data.scroll_begin.delta_hint_units = details.scroll_begin_units();
      gesture.data.scroll_begin.inertial_phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum;
      break;
    case EventType::kGestureScrollUpdate:
      gesture.SetType(WebInputEvent::Type::kGestureScrollUpdate);
      gesture.data.scroll_update.delta_x = IfNanUseMaxFloat(details.scroll_x());
      gesture.data.scroll_update.delta_y = IfNanUseMaxFloat(details.scroll_y());
      gesture.data.scroll_update.delta_units = details.scroll_update_units();
      gesture.data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum;
      break;
    case EventType::kGestureScrollEnd:
      gesture.SetType(WebInputEvent::Type::kGestureScrollEnd);
      gesture.data.scroll_end.inertial_phase =
          WebGestureEvent::InertialPhaseState::kNonMomentum;
      break;
    case EventType::kScrollFlingStart:
      gesture.SetType(WebInputEvent::Type::kGestureFlingStart);
      gesture.data.fling_start.velocity_x =
          IfNanUseMaxFloat(details.velocity_x());
      gesture.data.fling_start.velocity_y =
          IfNanUseMaxFloat(details.velocity_y());
      break;
    case EventType::kScrollFlingCancel:
      gesture.SetType(WebInputEvent::Type::kGestureFlingCancel);
      break;
    case EventType::kGesturePinchBegin:
      gesture.SetType(WebInputEvent::Type::kGesturePinchBegin);
      gesture.SetNeedsWheelEvent(source_device == WebGestureDevice::kTouchpad);
      break;
    case EventType::kGesturePinchUpdate:
      gesture.SetType(WebInputEvent::Type::kGesturePinchUpdate);
      gesture.data.pinch_update.scale = details.scale();
      gesture.SetNeedsWheelEvent(source_device == WebGestureDevice::kTouchpad);
      break;
    case EventType::kGesturePinchEnd:
      gesture.SetType(WebInputEvent::Type::kGesturePinchEnd);
      gesture.SetNeedsWheelEvent(source_device == WebGestureDevice::kTouchpad);
      break;
    case EventType::kGestureTapCancel:
      gesture.SetType(WebInputEvent::Type::kGestureTapCancel);
      break;
    case EventType::kGestureTapDown:
      gesture.SetType(WebInputEvent::Type::kGestureTapDown);
      gesture.data.tap_down.tap_down_count = details.tap_down_count();
      gesture.data.tap_down.width =
          IfNanUseMaxFloat(details.bounding_box_f().width());
      gesture.data.tap_down.height =
          IfNanUseMaxFloat(details.bounding_box_f().height());
      break;
    case EventType::kGestureBegin:
      gesture.SetType(WebInputEvent::Type::kGestureBegin);
      break;
    case EventType::kGestureEnd:
      gesture.SetType(WebInputEvent::Type::kGestureEnd);
      break;
    case EventType::kGestureSwipe:
      // The caller is responsible for discarding these gestures appropriately.
      gesture.SetType(WebInputEvent::Type::kUndefined);
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "EventType provided wasn't a valid gesture event: "
          << base::to_underlying(details.type());
  }

  return gesture;
}

float IfNanUseMaxFloat(float value) {
  if (std::isnan(value))
    return std::numeric_limits<float>::max();
  return value;
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
    float scale,
    std::optional<int64_t> trace_id) {
  return TranslateAndScaleWebInputEvent(event, gfx::Vector2dF(0, 0), scale,
                                        trace_id);
}

std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2dF& delta,
    float scale,
    std::optional<int64_t> trace_id) {
  std::unique_ptr<blink::WebInputEvent> scaled_event;
  if (scale == 1.f && delta.IsZero()) {
    return scaled_event;
  }
  if (event.GetType() == blink::WebMouseEvent::Type::kMouseWheel) {
    blink::WebMouseWheelEvent* wheel_event = new blink::WebMouseWheelEvent;
    scaled_event.reset(wheel_event);
    *wheel_event = static_cast<const blink::WebMouseWheelEvent&>(event);
    wheel_event->SetPositionInWidget(
        gfx::ScalePoint(wheel_event->PositionInWidget() + delta, scale));
    if (wheel_event->delta_units != ui::ScrollGranularity::kScrollByPage) {
      wheel_event->delta_x *= scale;
      wheel_event->delta_y *= scale;
      wheel_event->wheel_ticks_x *= scale;
      wheel_event->wheel_ticks_y *= scale;
    }
  } else if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    blink::WebMouseEvent* mouse_event = new blink::WebMouseEvent;
    scaled_event.reset(mouse_event);
    *mouse_event = static_cast<const blink::WebMouseEvent&>(event);
    mouse_event->SetPositionInWidget(
        gfx::ScalePoint(mouse_event->PositionInWidget() + delta, scale));
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
      touch_event->touches[i].SetPositionInWidget(gfx::ScalePoint(
          touch_event->touches[i].PositionInWidget() + delta, scale));
      touch_event->touches[i].radius_x *= scale;
      touch_event->touches[i].radius_y *= scale;
    }
  } else if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    blink::WebGestureEvent* gesture_event = new blink::WebGestureEvent;
    scaled_event.reset(gesture_event);
    *gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    gesture_event->SetPositionInWidget(
        gfx::ScalePoint(gesture_event->PositionInWidget() + delta, scale));
    switch (gesture_event->GetType()) {
      case blink::WebInputEvent::Type::kGestureScrollUpdate:
        if (gesture_event->data.scroll_update.delta_units ==
                ui::ScrollGranularity::kScrollByPixel ||
            gesture_event->data.scroll_update.delta_units ==
                ui::ScrollGranularity::kScrollByPrecisePixel) {
          gesture_event->data.scroll_update.delta_x *= scale;
          gesture_event->data.scroll_update.delta_y *= scale;
        }
        break;
      case blink::WebInputEvent::Type::kGestureScrollBegin:
        if (gesture_event->data.scroll_begin.delta_hint_units ==
                ui::ScrollGranularity::kScrollByPixel ||
            gesture_event->data.scroll_begin.delta_hint_units ==
                ui::ScrollGranularity::kScrollByPrecisePixel) {
          gesture_event->data.scroll_begin.delta_x_hint *= scale;
          gesture_event->data.scroll_begin.delta_y_hint *= scale;
        }
        break;

      case blink::WebInputEvent::Type::kGesturePinchUpdate:
        // Scale in pinch gesture is DSF agnostic.
        break;

      case blink::WebInputEvent::Type::kGestureDoubleTap:
      case blink::WebInputEvent::Type::kGestureTap:
      case blink::WebInputEvent::Type::kGestureTapUnconfirmed:
        gesture_event->data.tap.width *= scale;
        gesture_event->data.tap.height *= scale;
        break;

      case blink::WebInputEvent::Type::kGestureTapDown:
        gesture_event->data.tap_down.width *= scale;
        gesture_event->data.tap_down.height *= scale;
        break;

      case blink::WebInputEvent::Type::kGestureShowPress:
        gesture_event->data.show_press.width *= scale;
        gesture_event->data.show_press.height *= scale;
        break;

      case blink::WebInputEvent::Type::kGestureLongPress:
      case blink::WebInputEvent::Type::kGestureLongTap:
        gesture_event->data.long_press.width *= scale;
        gesture_event->data.long_press.height *= scale;
        break;

      case blink::WebInputEvent::Type::kGestureTwoFingerTap:
        gesture_event->data.two_finger_tap.first_finger_width *= scale;
        gesture_event->data.two_finger_tap.first_finger_height *= scale;
        break;

      case blink::WebInputEvent::Type::kGestureFlingStart:
        gesture_event->data.fling_start.velocity_x *= scale;
        gesture_event->data.fling_start.velocity_y *= scale;
        break;

      // These event does not have location data.
      case blink::WebInputEvent::Type::kGesturePinchBegin:
      case blink::WebInputEvent::Type::kGesturePinchEnd:
      case blink::WebInputEvent::Type::kGestureTapCancel:
      case blink::WebInputEvent::Type::kGestureFlingCancel:
      case blink::WebInputEvent::Type::kGestureScrollEnd:
        break;

      // TODO(oshima): Find out if ContextMenu needs to be scaled.
      default:
        break;
    }

    if (gesture_event->GetType() ==
            blink::WebInputEvent::Type::kGestureScrollUpdate &&
        trace_id.has_value()) {
      TRACE_EVENT("input,input.scrolling", "TranslateAndScaleWebInputEvent",
                  [trace_id_value = *trace_id,
                   delta_x = gesture_event->data.scroll_update.delta_x,
                   delta_y = gesture_event->data.scroll_update.delta_y](
                      perfetto::EventContext& ctx) {
                    auto* event =
                        ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                    auto* scroll_data = event->set_scroll_deltas();
                    scroll_data->set_trace_id(trace_id_value);
                    scroll_data->set_original_delta_x(delta_x);
                    scroll_data->set_original_delta_y(delta_y);
                  });
    }
  }
  return scaled_event;
}

WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::Action::DOWN:
    case MotionEvent::Action::BUTTON_PRESS:
      return WebInputEvent::Type::kMouseDown;
    case MotionEvent::Action::MOVE:
    case MotionEvent::Action::HOVER_MOVE:
      return WebInputEvent::Type::kMouseMove;
    case MotionEvent::Action::HOVER_ENTER:
      return WebInputEvent::Type::kMouseEnter;
    case MotionEvent::Action::HOVER_EXIT:
      return WebInputEvent::Type::kMouseLeave;
    case MotionEvent::Action::UP:
    case MotionEvent::Action::BUTTON_RELEASE:
      return WebInputEvent::Type::kMouseUp;
    case MotionEvent::Action::NONE:
    case MotionEvent::Action::CANCEL:
    case MotionEvent::Action::POINTER_DOWN:
    case MotionEvent::Action::POINTER_UP:
      break;
  }
  DUMP_WILL_BE_NOTREACHED() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::Type::kUndefined;
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

bool IsContinuousGestureEvent(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::Type::kGestureScrollUpdate:
    case blink::WebGestureEvent::Type::kGesturePinchUpdate:
      return true;
    default:
      return false;
  }
}

EventPointerType WebPointerTypeToEventPointerType(
    WebPointerProperties::PointerType type) {
  switch (type) {
    case WebPointerProperties::PointerType::kMouse:
      return EventPointerType::kMouse;
    case WebPointerProperties::PointerType::kPen:
      return EventPointerType::kPen;
    case WebPointerProperties::PointerType::kEraser:
      return EventPointerType::kEraser;
    case WebPointerProperties::PointerType::kTouch:
      return EventPointerType::kTouch;
    case WebPointerProperties::PointerType::kUnknown:
      return EventPointerType::kUnknown;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid pointer type";
  return EventPointerType::kUnknown;
}

blink::WebGestureEvent ScrollBeginFromScrollUpdate(
    const blink::WebGestureEvent& gesture_update) {
  DCHECK(gesture_update.GetType() == WebInputEvent::Type::kGestureScrollUpdate);

  WebGestureEvent scroll_begin(gesture_update);
  scroll_begin.SetType(WebInputEvent::Type::kGestureScrollBegin);

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

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<WebGestureEvent> CreateWebGestureEventFromGestureEventAndroid(
    const GestureEventAndroid& event) {
  WebInputEvent::Type event_type = WebInputEvent::Type::kUndefined;
  switch (event.type()) {
    case GESTURE_EVENT_TYPE_PINCH_BEGIN:
      event_type = WebInputEvent::Type::kGesturePinchBegin;
      break;
    case GESTURE_EVENT_TYPE_PINCH_BY:
      event_type = WebInputEvent::Type::kGesturePinchUpdate;
      break;
    case GESTURE_EVENT_TYPE_PINCH_END:
      event_type = WebInputEvent::Type::kGesturePinchEnd;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_START:
      event_type = WebInputEvent::Type::kGestureScrollBegin;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_BY:
      event_type = WebInputEvent::Type::kGestureScrollUpdate;
      break;
    case GESTURE_EVENT_TYPE_SCROLL_END:
      event_type = WebInputEvent::Type::kGestureScrollEnd;
      break;
    case GESTURE_EVENT_TYPE_FLING_START:
      event_type = WebInputEvent::Type::kGestureFlingStart;
      break;
    case GESTURE_EVENT_TYPE_FLING_CANCEL:
      event_type = WebInputEvent::Type::kGestureFlingCancel;
      break;
    case GESTURE_EVENT_TYPE_DOUBLE_TAP:
      event_type = WebInputEvent::Type::kGestureDoubleTap;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown gesture event type";
      return std::make_unique<WebGestureEvent>();
  }
  auto web_event = std::make_unique<WebGestureEvent>(
      event_type, WebInputEvent::kNoModifiers,
      base::TimeTicks() + base::Milliseconds(event.time()));
  // NOTE: Source gesture events are synthetic ones that simulate
  // gesture from keyboard (zoom in/out) for now. Should populate Blink
  // event's fields better when extended to handle more cases.
  web_event->SetPositionInWidget(event.location());
  web_event->SetPositionInScreen(event.screen_location());
  web_event->SetSourceDevice(WebGestureDevice::kTouchscreen);
  if (event.synthetic_scroll())
    web_event->SetSourceDevice(WebGestureDevice::kSyntheticAutoscroll);
  if (event_type == WebInputEvent::Type::kGesturePinchUpdate) {
    web_event->data.pinch_update.scale = event.scale();
  } else if (event_type == WebInputEvent::Type::kGestureScrollBegin) {
    web_event->data.scroll_begin.delta_x_hint = event.delta_x();
    web_event->data.scroll_begin.delta_y_hint = event.delta_y();
    web_event->data.scroll_begin.target_viewport = event.target_viewport();
  } else if (event_type == WebInputEvent::Type::kGestureScrollUpdate) {
    web_event->data.scroll_update.delta_x = event.delta_x();
    web_event->data.scroll_update.delta_y = event.delta_y();
  } else if (event_type == WebInputEvent::Type::kGestureFlingStart) {
    web_event->data.fling_start.velocity_x = event.velocity_x();
    web_event->data.fling_start.velocity_y = event.velocity_y();
    web_event->data.fling_start.target_viewport = event.target_viewport();
  } else if (event_type == WebInputEvent::Type::kGestureFlingCancel) {
    web_event->data.fling_cancel.prevent_boosting = event.prevent_boosting();
    if (event.synthetic_scroll())
      web_event->data.fling_cancel.target_viewport = true;
  } else if (event_type == WebInputEvent::Type::kGestureDoubleTap) {
    // Set the tap count to 1 even for DoubleTap, in order to be consistent with
    // double tap behavior on a mobile viewport. See https://crbug.com/234986
    // for context.
    web_event->data.tap.tap_count = 1;
  }

  return web_event;
}
#endif

}  // namespace ui
