// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event.h"

#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/blink/web_input_event_builders_win.h"
#endif

namespace ui {

namespace {

gfx::PointF GetScreenLocationFromEvent(const LocatedEvent& event) {
  return event.target() ? event.target()->GetScreenLocationF(event)
                        : event.root_location_f();
}

// Creates a WebGestureEvent from a GestureEvent. Note that it does not
// populate the event coordinates (i.e. |x|, |y|, |globalX|, and |globalY|). So
// the caller must populate these fields.
blink::WebGestureEvent MakeWebGestureEventFromUIEvent(
    const GestureEvent& event) {
  return CreateWebGestureEvent(event.details(), event.time_stamp(),
                               event.location_f(), event.root_location_f(),
                               event.flags(), event.unique_touch_event_id());
}

}  // namespace

#if BUILDFLAG(IS_WIN)
// On Windows, we can just use the builtin WebKit factory methods to fully
// construct our pre-translated events.

blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const PlatformEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  return WebMouseEventBuilder::Build(native_event.hwnd, native_event.message,
                                     native_event.wParam, native_event.lParam,
                                     time_stamp, pointer_type);
}

blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const PlatformEvent& native_event,
    const base::TimeTicks& time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  return WebMouseWheelEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, time_stamp, pointer_type);
}
#endif  // BUILDFLAG(IS_WIN)

blink::WebKeyboardEvent MakeWebKeyboardEventFromUiEvent(const KeyEvent& event) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kUndefined;
  switch (event.type()) {
    case EventType::kKeyPressed:
      type = event.is_char() ? blink::WebInputEvent::Type::kChar
                             : blink::WebInputEvent::Type::kRawKeyDown;
      break;
    case EventType::kKeyReleased:
      type = blink::WebInputEvent::Type::kKeyUp;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  blink::WebKeyboardEvent webkit_event(
      type,
      EventFlagsToWebEventModifiers(event.flags()) |
          DomCodeToWebInputEventModifiers(event.code()),
      event.time_stamp());

  if (webkit_event.GetModifiers() & blink::WebInputEvent::kAltKey)
    webkit_event.is_system_key = true;
  webkit_event.windows_key_code = event.key_code();
  webkit_event.native_key_code =
      KeycodeConverter::DomCodeToNativeKeycode(event.code());
  webkit_event.dom_code = static_cast<int>(event.code());
  webkit_event.dom_key = static_cast<int>(event.GetDomKey());
  webkit_event.unmodified_text[0] = event.GetUnmodifiedText();
  webkit_event.text[0] = event.GetText();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const ScrollEvent& event) {
  blink::WebMouseWheelEvent webkit_event(
      blink::WebInputEvent::Type::kMouseWheel,
      EventFlagsToWebEventModifiers(event.flags()), event.time_stamp());

  webkit_event.button = blink::WebMouseEvent::Button::kNoButton;
  webkit_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;

  float offset_ordinal_x = event.x_offset_ordinal();
  float offset_ordinal_y = event.y_offset_ordinal();
  webkit_event.delta_x = event.x_offset();
  webkit_event.delta_y = event.y_offset();

  if (offset_ordinal_x != 0.f && webkit_event.delta_x != 0.f)
    webkit_event.acceleration_ratio_x = offset_ordinal_x / webkit_event.delta_x;
  webkit_event.wheel_ticks_x =
      webkit_event.delta_x / MouseWheelEvent::kWheelDelta;
  webkit_event.wheel_ticks_y =
      webkit_event.delta_y / MouseWheelEvent::kWheelDelta;
  if (offset_ordinal_y != 0.f && webkit_event.delta_y != 0.f)
    webkit_event.acceleration_ratio_y = offset_ordinal_y / webkit_event.delta_y;

  webkit_event.pointer_type = event.pointer_details().pointer_type;

  switch (event.scroll_event_phase()) {
    case ui::ScrollEventPhase::kNone:
      webkit_event.phase = blink::WebMouseWheelEvent::kPhaseNone;
      break;
    case ui::ScrollEventPhase::kBegan:
      webkit_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
      break;
    case ui::ScrollEventPhase::kUpdate:
      webkit_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
      break;
    case ui::ScrollEventPhase::kEnd:
      webkit_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  switch (event.momentum_phase()) {
    case ui::EventMomentumPhase::NONE:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseNone;
      break;
    case ui::EventMomentumPhase::BEGAN:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseBegan;
      break;
    case ui::EventMomentumPhase::MAY_BEGIN:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseMayBegin;
      break;
    case ui::EventMomentumPhase::INERTIAL_UPDATE:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseChanged;
      break;
    case ui::EventMomentumPhase::END:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseEnded;
      break;
    case ui::EventMomentumPhase::BLOCKED:
      webkit_event.momentum_phase = blink::WebMouseWheelEvent::kPhaseBlocked;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return webkit_event;
}

blink::WebGestureEvent MakeWebGestureEventFromUiEvent(
    const ScrollEvent& event) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kUndefined;
  switch (event.type()) {
    case EventType::kScrollFlingStart:
      type = blink::WebInputEvent::Type::kGestureFlingStart;
      break;
    case EventType::kScrollFlingCancel:
      type = blink::WebInputEvent::Type::kGestureFlingCancel;
      break;
    case EventType::kScroll:
      NOTREACHED_IN_MIGRATION()
          << "Invalid gesture type: " << base::to_underlying(event.type());
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown gesture type: " << base::to_underlying(event.type());
  }

  blink::WebGestureEvent webkit_event(
      type, EventFlagsToWebEventModifiers(event.flags()), event.time_stamp(),
      blink::WebGestureDevice::kTouchpad);
  if (event.type() == EventType::kScrollFlingStart) {
    webkit_event.data.fling_start.velocity_x = event.x_offset();
    webkit_event.data.fling_start.velocity_y = event.y_offset();
  }

  return webkit_event;
}

blink::WebMouseEvent MakeWebMouseEventFromUiEvent(const MouseEvent& event);
blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const MouseWheelEvent& event);

// General approach:
//
// Event only carries a subset of possible event data provided to UI by the host
// platform. WebKit utilizes a larger subset of that information, and includes
// some built in cracking functionality that we rely on to obtain this
// information cleanly and consistently.
//
// The only place where an Event's data differs from what the underlying
// PlatformEvent would provide is position data. We would like to provide
// coordinates relative to its hosting window, rather than the top level
// platform window. The event target is used to get the screen coordinates.
//
// The approach is to fully construct a blink::WebInputEvent from the
// Event's PlatformEvent, and then replace the coordinate fields with
// the translated values from the Event (and EventTarget).
//
// The exception is mouse events on linux. The MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// MouseEvent. This will not be necessary once only XInput2 is supported.
//

blink::WebMouseEvent MakeWebMouseEvent(const MouseEvent& event) {
  // Construct an untranslated event from the platform event data.
  blink::WebMouseEvent webkit_event =
#if BUILDFLAG(IS_WIN)
      // On Windows we have WM_ events coming from desktop and pure Events
      // coming from metro mode.
      event.native_event().message && (event.type() != EventType::kMouseExited)
          ? MakeUntranslatedWebMouseEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                event.pointer_details().pointer_type)
          : MakeWebMouseEventFromUiEvent(event);
#else
      MakeWebMouseEventFromUiEvent(event);
#endif
  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.SetPositionInWidget(event.x(), event.y());
  if (event.flags() & ui::EF_UNADJUSTED_MOUSE) {
    webkit_event.movement_x = event.movement().x();
    webkit_event.movement_y = event.movement().y();
    webkit_event.is_raw_movement_event = true;
  }

#if BUILDFLAG(IS_WIN)
  if (event.native_event().message && event.type() != EventType::kMouseExited) {
    return webkit_event;
  }
#endif

  const gfx::PointF screen_point = GetScreenLocationFromEvent(event);
  webkit_event.SetPositionInScreen(screen_point.x(), screen_point.y());

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(const MouseWheelEvent& event) {
#if BUILDFLAG(IS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                event.pointer_details().pointer_type)
          : MakeWebMouseWheelEventFromUiEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromUiEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.SetPositionInWidget(event.x(), event.y());

  const gfx::PointF screen_point = GetScreenLocationFromEvent(event);
  webkit_event.SetPositionInScreen(screen_point.x(), screen_point.y());

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEvent(const ScrollEvent& event) {
#if BUILDFLAG(IS_WIN)
  // Construct an untranslated event from the platform event data.
  blink::WebMouseWheelEvent webkit_event =
      event.native_event().message
          ? MakeUntranslatedWebMouseWheelEventFromNativeEvent(
                event.native_event(), event.time_stamp(),
                event.pointer_details().pointer_type)
          : MakeWebMouseWheelEventFromUiEvent(event);
#else
  blink::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromUiEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.SetPositionInWidget(event.x(), event.y());

  const gfx::PointF screen_point = GetScreenLocationFromEvent(event);
  webkit_event.SetPositionInScreen(screen_point.x(), screen_point.y());

  return webkit_event;
}

blink::WebKeyboardEvent MakeWebKeyboardEvent(const KeyEvent& event) {
  // TODO(wez): Work out how this comment relates to the code below.
  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event.native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
  blink::WebKeyboardEvent webkit_event = MakeWebKeyboardEventFromUiEvent(event);
#if BUILDFLAG(IS_WIN)
  if (event.HasNativeEvent()) {
    const PlatformEvent& native_event = event.native_event();

    // System key events are explicitly distinguished, under Windows.
    webkit_event.is_system_key = native_event.message == WM_SYSCHAR ||
                                 native_event.message == WM_SYSKEYDOWN ||
                                 native_event.message == WM_SYSKEYUP;

    // Copy the OEM scancode, including flag bits, directly from the event.
    webkit_event.native_key_code = static_cast<int>(native_event.lParam);
  }
#endif
  return webkit_event;
}

blink::WebGestureEvent MakeWebGestureEvent(const GestureEvent& event) {
  blink::WebGestureEvent gesture_event = MakeWebGestureEventFromUIEvent(event);

  gesture_event.SetPositionInWidget(event.location_f());

  const gfx::PointF screen_point = GetScreenLocationFromEvent(event);
  gesture_event.SetPositionInScreen(screen_point);

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEvent(const ScrollEvent& event) {
  blink::WebGestureEvent gesture_event = MakeWebGestureEventFromUiEvent(event);
  gesture_event.SetPositionInWidget(event.location_f());

  const gfx::PointF screen_point = GetScreenLocationFromEvent(event);
  gesture_event.SetPositionInScreen(screen_point);

  return gesture_event;
}

blink::WebGestureEvent MakeWebGestureEventFlingCancel(
    const blink::WebMouseWheelEvent& wheel_event) {
  blink::WebGestureEvent gesture_event(
      blink::WebInputEvent::Type::kGestureFlingCancel,
      blink::WebInputEvent::kNoModifiers, wheel_event.TimeStamp(),
      blink::WebGestureDevice::kTouchpad);
  // Coordinates need to be transferred to the fling cancel gesture only
  // for Surface-targeting to ensure that it is targeted to the correct
  // RenderWidgetHost.
  gesture_event.SetPositionInWidget(wheel_event.PositionInWidget());
  gesture_event.SetPositionInScreen(wheel_event.PositionInScreen());
  // All other fields are ignored on a GestureFlingCancel event.
  return gesture_event;
}

blink::WebMouseEvent MakeWebMouseEventFromUiEvent(const MouseEvent& event) {
  blink::WebInputEvent::Type type = blink::WebInputEvent::Type::kUndefined;
  int click_count = 0;
  switch (event.type()) {
    case EventType::kMousePressed:
      type = blink::WebInputEvent::Type::kMouseDown;
      click_count = event.GetClickCount();
      break;
    case EventType::kMouseReleased:
      type = blink::WebInputEvent::Type::kMouseUp;
      click_count = event.GetClickCount();
      break;
    case EventType::kMouseExited: {
      // When MOUSE_EXITED is created for intermediate windows that the
      // pointer crosses through, change these into mouse move events.
      const Event::Properties* props = event.properties();
      if (props && props->contains(kPropertyMouseCrossedIntermediateWindow)) {
        type = blink::WebInputEvent::Type::kMouseMove;
      } else {
        static bool s_send_leave =
            base::FeatureList::IsEnabled(features::kSendMouseLeaveEvents);
        type = s_send_leave ? blink::WebInputEvent::Type::kMouseLeave
                            : blink::WebInputEvent::Type::kMouseMove;
      }
      break;
    }
    case EventType::kMouseEntered:
    case EventType::kMouseMoved:
    case EventType::kMouseDragged:
      type = blink::WebInputEvent::Type::kMouseMove;
      break;
    default:
      NOTIMPLEMENTED() << "Received unexpected event: "
                       << base::to_underlying(event.type());
      break;
  }

  blink::WebMouseEvent webkit_event(
      type, EventFlagsToWebEventModifiers(event.flags()), event.time_stamp(),
      event.pointer_details().id);
  webkit_event.button = blink::WebMouseEvent::Button::kNoButton;
  int button_flags = event.flags();
  if (event.type() == EventType::kMousePressed ||
      event.type() == EventType::kMouseReleased) {
    // We want to use changed_button_flags() for mouse pressed & released.
    // These flags can be used only if they are set which is not always the case
    // (see e.g. GetChangedMouseButtonFlagsFromNative() in events_win.cc).
    if (event.changed_button_flags())
      button_flags = event.changed_button_flags();
  }

  // TODO(mustaq): This |if| ordering look suspicious. Replacing with if-else &
  // changing the order to L/R/M/B/F breaks
  // pointerevent_pointermove_on_chorded_mouse_button-manual.html! Investigate.
  if (button_flags & EF_BACK_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::kBack;
  if (button_flags & EF_FORWARD_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::kForward;
  if (button_flags & EF_LEFT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::kLeft;
  if (button_flags & EF_MIDDLE_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::kMiddle;
  if (button_flags & EF_RIGHT_MOUSE_BUTTON)
    webkit_event.button = blink::WebMouseEvent::Button::kRight;

  webkit_event.click_count = click_count;
  webkit_event.tilt_x = event.pointer_details().tilt_x;
  webkit_event.tilt_y = event.pointer_details().tilt_y;
  webkit_event.force = event.pointer_details().force;
  webkit_event.tangential_pressure =
      event.pointer_details().tangential_pressure;
  webkit_event.twist = event.pointer_details().twist;
  webkit_event.id = event.pointer_details().id;
  webkit_event.pointer_type = event.pointer_details().pointer_type;
  webkit_event.device_id = event.source_device_id();

  return webkit_event;
}

blink::WebMouseWheelEvent MakeWebMouseWheelEventFromUiEvent(
    const MouseWheelEvent& event) {
  blink::WebMouseWheelEvent webkit_event(
      blink::WebInputEvent::Type::kMouseWheel,
      EventFlagsToWebEventModifiers(event.flags()), event.time_stamp());

  webkit_event.button = blink::WebMouseEvent::Button::kNoButton;

  webkit_event.delta_x = event.x_offset();
  webkit_event.delta_y = event.y_offset();

  DCHECK(!(event.flags() & ui::EF_PRECISION_SCROLLING_DELTA &&
           event.flags() & ui::EF_SCROLL_BY_PAGE));

  if (event.flags() & ui::EF_PRECISION_SCROLLING_DELTA) {
    webkit_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  } else if (event.flags() & ui::EF_SCROLL_BY_PAGE) {
    webkit_event.delta_units = ui::ScrollGranularity::kScrollByPage;
  }

  webkit_event.wheel_ticks_x =
      webkit_event.delta_x / MouseWheelEvent::kWheelDelta;
  webkit_event.wheel_ticks_y =
      webkit_event.delta_y / MouseWheelEvent::kWheelDelta;

  // Set deltas to be percent based if percent based scrolling is enabled.
  // If percent based scrolling is enabled on Windows, percent based
  // mousewheel events are built in the Windows web input event builder.
  // Percent based scrolling is not supported on Mac because the current
  // roadmap for scroll personality work is reserved for Windows and Linux.
  // Page based scrolling isn't specified in terms of pixels so we don't convert
  // deltas to a percentage here - it's resolved into percent, then pixels,
  // in the renderer.
  // TODO(yshalivskyy) Currently, for page based scrolling we always scroll
  // by one page dismissing delta_y/delta_x values. https://crbug.com/1196092
  if (features::IsPercentBasedScrollingEnabled() &&
      webkit_event.delta_units != ui::ScrollGranularity::kScrollByPage &&
      webkit_event.delta_units !=
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    webkit_event.delta_units = ui::ScrollGranularity::kScrollByPercentage;
    webkit_event.delta_y *=
        (kScrollPercentPerLineOrChar / MouseWheelEvent::kWheelDelta);
    webkit_event.delta_x *=
        (kScrollPercentPerLineOrChar / MouseWheelEvent::kWheelDelta);
  }

  webkit_event.tilt_x = event.pointer_details().tilt_x;
  webkit_event.tilt_y = event.pointer_details().tilt_y;
  webkit_event.force = event.pointer_details().force;
  webkit_event.pointer_type = event.pointer_details().pointer_type;

  return webkit_event;
}

}  // namespace ui
