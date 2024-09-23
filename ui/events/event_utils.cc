// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <limits>
#include <map>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/events/win/events_win_utils.h"
#endif

namespace ui {

namespace {

int g_custom_event_types = base::to_underlying(EventType::kLast);

#define UMA_HISTOGRAM_EVENT_LATENCY_TIMES(name, sample)           \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::Milliseconds(1), \
                             base::Minutes(1), 50)

// Record a trace if the `current_time` - `time_stamp` is above a threshold.
// `name` must be a static string so that the trace is not privacy filtered.
void RecordEventLatencyTrace(perfetto::StaticString name,
                             base::TimeTicks time_stamp,
                             base::TimeTicks current_time) {
  // 20 msec will catch the 90th percentile of most events on most platforms,
  // the 95th percentile of a few (eg. touch events on Windows, mouse events on
  // Mac).
  constexpr base::TimeDelta kMinJank = base::Milliseconds(20);
  if (current_time - time_stamp >= kMinJank) {
    // Nest the event in the current process, using the global trace id only for
    // uniqueness.
    const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                                perfetto::ProcessTrack::Current());
    TRACE_EVENT_BEGIN("latency", name, track, time_stamp);
    TRACE_EVENT_END("latency", track, current_time);
  }
}

}  // namespace

std::unique_ptr<Event> EventFromNative(const PlatformEvent& native_event) {
  std::unique_ptr<Event> event;
  EventType type = EventTypeFromNative(native_event);
  switch(type) {
    case EventType::kKeyPressed:
    case EventType::kKeyReleased:
      event = std::make_unique<KeyEvent>(native_event);
      break;

    case EventType::kMousePressed:
    case EventType::kMouseDragged:
    case EventType::kMouseReleased:
    case EventType::kMouseMoved:
    case EventType::kMouseEntered:
    case EventType::kMouseExited:
      event = std::make_unique<MouseEvent>(native_event);
      break;

    case EventType::kMousewheel:
      event = std::make_unique<MouseWheelEvent>(native_event);
      break;

    case EventType::kScrollFlingStart:
    case EventType::kScrollFlingCancel:
    case EventType::kScroll:
      event = std::make_unique<ScrollEvent>(native_event);
      break;

    case EventType::kTouchReleased:
    case EventType::kTouchPressed:
    case EventType::kTouchMoved:
    case EventType::kTouchCancelled:
      event = std::make_unique<TouchEvent>(native_event);
      break;

    default:
      break;
  }
  return event;
}

int RegisterCustomEventType() {
  return ++g_custom_event_types;
}

bool ShouldDefaultToNaturalScroll() {
  return GetInternalDisplayTouchSupport() ==
         display::Display::TouchSupport::AVAILABLE;
}

display::Display::TouchSupport GetInternalDisplayTouchSupport() {
  display::Screen* screen = display::Screen::GetScreen();
  // No screen in some unit tests.
  if (!screen)
    return display::Display::TouchSupport::UNKNOWN;
  const std::vector<display::Display>& displays = screen->GetAllDisplays();
  for (auto it = displays.begin(); it != displays.end(); ++it) {
    if (it->IsInternal())
      return it->touch_support();
  }
  return display::Display::TouchSupport::UNAVAILABLE;
}

void ComputeEventLatencyOS(const PlatformEvent& native_event) {
  base::TimeTicks current_time = EventTimeForNow();
  base::TimeTicks time_stamp =
      EventLatencyTimeFromNative(native_event, current_time);
  EventType type = EventTypeFromNative(native_event);
  ComputeEventLatencyOS(type, time_stamp, current_time);
}

void ComputeEventLatencyOS(EventType type,
                           base::TimeTicks time_stamp,
                           base::TimeTicks current_time) {
  static constexpr char kKeyPressedEventName[] =
      "Event.Latency.OS2.KEY_PRESSED";
  static constexpr char kMousePressedEventName[] =
      "Event.Latency.OS2.MOUSE_PRESSED";
  static constexpr char kMouseWheelEventName[] =
      "Event.Latency.OS2.MOUSE_WHEEL";
  static constexpr char kTouchCancelledEventName[] =
      "Event.Latency.OS2.TOUCH_CANCELLED";
  static constexpr char kTouchMovedEventName[] =
      "Event.Latency.OS2.TOUCH_MOVED";
  static constexpr char kTouchPressedEventName[] =
      "Event.Latency.OS2.TOUCH_PRESSED";
  static constexpr char kTouchReleasedEventName[] =
      "Event.Latency.OS2.TOUCH_RELEASED";

  base::TimeDelta delta = current_time - time_stamp;

  switch (type) {
#if BUILDFLAG(IS_APPLE)
    // On Mac, EventType::kScroll and EventType::kMousewheel represent the same
    // class of events.
    case EventType::kScroll:
#endif
    case EventType::kMousewheel:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kMouseWheelEventName, delta);
      // Do not record traces for wheel events to avoid spam.
      return;
    case EventType::kTouchMoved:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchMovedEventName, delta);
      // Do not record traces for move events to avoid spam.
      return;
    case EventType::kTouchPressed:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchPressedEventName, delta);
      RecordEventLatencyTrace(kTouchPressedEventName, time_stamp, current_time);
      return;
    case EventType::kTouchReleased:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchReleasedEventName, delta);
      RecordEventLatencyTrace(kTouchReleasedEventName, time_stamp,
                              current_time);
      return;
    case EventType::kTouchCancelled:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchCancelledEventName, delta);
      RecordEventLatencyTrace(kTouchCancelledEventName, time_stamp,
                              current_time);
      return;
    case EventType::kKeyPressed:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kKeyPressedEventName, delta);
      RecordEventLatencyTrace(kKeyPressedEventName, time_stamp, current_time);
      return;
    case EventType::kMousePressed:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kMousePressedEventName, delta);
      RecordEventLatencyTrace(kMousePressedEventName, time_stamp, current_time);
      return;
    default:
      return;
  }
}

#if BUILDFLAG(IS_WIN)

void ComputeEventLatencyOSFromTOUCHINPUT(EventType event_type,
                                         TOUCHINPUT touch_input,
                                         base::TimeTicks current_time) {
  base::TimeTicks time_stamp =
      EventLatencyTimeFromTickClock(touch_input.dwTime, current_time);
  ComputeEventLatencyOS(event_type, time_stamp, current_time);
}

void ComputeEventLatencyOSFromPOINTER_INFO(EventType event_type,
                                           POINTER_INFO pointer_info,
                                           base::TimeTicks current_time) {
  base::TimeTicks time_stamp;
  if (pointer_info.PerformanceCount) {
    if (!base::TimeTicks::IsHighResolution()) {
      // The tick clock will be incompatible with |event_time|.
      return;
    }
    time_stamp =
        EventLatencyTimeFromPerformanceCounter(pointer_info.PerformanceCount);
  } else if (pointer_info.dwTime) {
    time_stamp =
        EventLatencyTimeFromTickClock(pointer_info.dwTime, current_time);
  } else {
    // Bad POINTER_INFO with no timestamp.
    return;
  }
  ComputeEventLatencyOS(event_type, time_stamp, current_time);
}

#endif  // BUILDFLAG(IS_WIN)

void ConvertEventLocationToTargetWindowLocation(
    const gfx::Point& target_window_origin,
    const gfx::Point& current_window_origin,
    ui::LocatedEvent* located_event) {
  if (current_window_origin == target_window_origin)
    return;

  DCHECK(located_event);
  gfx::Vector2d offset = current_window_origin - target_window_origin;
  gfx::PointF location_in_pixel_in_host =
      located_event->location_f() + gfx::Vector2dF(offset);
  located_event->set_location_f(location_in_pixel_in_host);
}

std::string_view EventTypeName(EventType type) {
  if (type >= EventType::kLast) {
    return "";
  }

#define CASE_TYPE(t) \
  case t:            \
    return #t

  switch (type) {
    CASE_TYPE(EventType::kUnknown);
    CASE_TYPE(EventType::kMousePressed);
    CASE_TYPE(EventType::kMouseDragged);
    CASE_TYPE(EventType::kMouseReleased);
    CASE_TYPE(EventType::kMouseMoved);
    CASE_TYPE(EventType::kMouseEntered);
    CASE_TYPE(EventType::kMouseExited);
    CASE_TYPE(EventType::kKeyPressed);
    CASE_TYPE(EventType::kKeyReleased);
    CASE_TYPE(EventType::kMousewheel);
    CASE_TYPE(EventType::kMouseCaptureChanged);
    CASE_TYPE(EventType::kTouchReleased);
    CASE_TYPE(EventType::kTouchPressed);
    CASE_TYPE(EventType::kTouchMoved);
    CASE_TYPE(EventType::kTouchCancelled);
    CASE_TYPE(EventType::kDropTargetEvent);
    CASE_TYPE(EventType::kGestureScrollBegin);
    CASE_TYPE(EventType::kGestureScrollEnd);
    CASE_TYPE(EventType::kGestureScrollUpdate);
    CASE_TYPE(EventType::kGestureShowPress);
    CASE_TYPE(EventType::kGestureTap);
    CASE_TYPE(EventType::kGestureTapDown);
    CASE_TYPE(EventType::kGestureTapCancel);
    CASE_TYPE(EventType::kGestureBegin);
    CASE_TYPE(EventType::kGestureEnd);
    CASE_TYPE(EventType::kGestureTwoFingerTap);
    CASE_TYPE(EventType::kGesturePinchBegin);
    CASE_TYPE(EventType::kGesturePinchEnd);
    CASE_TYPE(EventType::kGesturePinchUpdate);
    CASE_TYPE(EventType::kGestureShortPress);
    CASE_TYPE(EventType::kGestureLongPress);
    CASE_TYPE(EventType::kGestureLongTap);
    CASE_TYPE(EventType::kGestureSwipe);
    CASE_TYPE(EventType::kGestureTapUnconfirmed);
    CASE_TYPE(EventType::kGestureDoubleTap);
    CASE_TYPE(EventType::kScroll);
    CASE_TYPE(EventType::kScrollFlingStart);
    CASE_TYPE(EventType::kScrollFlingCancel);
    CASE_TYPE(EventType::kCancelMode);
    CASE_TYPE(EventType::kUmaData);
    case EventType::kLast:
      NOTREACHED_IN_MIGRATION();
      return "";
      // Don't include default, so that we get an error when new type is added.
  }
#undef CASE_TYPE

  NOTREACHED_IN_MIGRATION();
  return "";
}

std::vector<std::string_view> EventFlagsNames(int event_flags) {
  std::vector<std::string_view> names;
  names.reserve(5);  // Seems like a good starting point.
  if (!event_flags) {
    names.push_back("NONE");
    return names;
  }

  if (event_flags & EF_IS_SYNTHESIZED)
    names.push_back("IS_SYNTHESIZED");
  if (event_flags & EF_SHIFT_DOWN)
    names.push_back("SHIFT_DOWN");
  if (event_flags & EF_CONTROL_DOWN)
    names.push_back("CONTROL_DOWN");
  if (event_flags & EF_ALT_DOWN)
    names.push_back("ALT_DOWN");
  if (event_flags & EF_COMMAND_DOWN)
    names.push_back("COMMAND_DOWN");
  if (event_flags & EF_ALTGR_DOWN)
    names.push_back("ALTGR_DOWN");
  if (event_flags & EF_MOD3_DOWN)
    names.push_back("MOD3_DOWN");
  if (event_flags & EF_FUNCTION_DOWN)
    names.push_back("FUNCTION_DOWN");
  if (event_flags & EF_NUM_LOCK_ON)
    names.push_back("NUM_LOCK_ON");
  if (event_flags & EF_CAPS_LOCK_ON)
    names.push_back("CAPS_LOCK_ON");
  if (event_flags & EF_SCROLL_LOCK_ON)
    names.push_back("SCROLL_LOCK_ON");
  if (event_flags & EF_LEFT_MOUSE_BUTTON)
    names.push_back("LEFT_MOUSE_BUTTON");
  if (event_flags & EF_MIDDLE_MOUSE_BUTTON)
    names.push_back("MIDDLE_MOUSE_BUTTON");
  if (event_flags & EF_RIGHT_MOUSE_BUTTON)
    names.push_back("RIGHT_MOUSE_BUTTON");
  if (event_flags & EF_BACK_MOUSE_BUTTON)
    names.push_back("BACK_MOUSE_BUTTON");
  if (event_flags & EF_FORWARD_MOUSE_BUTTON)
    names.push_back("FORWARD_MOUSE_BUTTON");

  return names;
}

std::vector<std::string_view> KeyEventFlagsNames(int event_flags) {
  std::vector<std::string_view> names = EventFlagsNames(event_flags);
  if (!event_flags)
    return names;

  if (event_flags & EF_IME_FABRICATED_KEY)
    names.push_back("IME_FABRICATED_KEY");
  if (event_flags & EF_IS_REPEAT)
    names.push_back("IS_REPEAT");
  if (event_flags & EF_FINAL)
    names.push_back("FINAL");
  if (event_flags & EF_IS_EXTENDED_KEY)
    names.push_back("IS_EXTENDED_KEY");
  if (event_flags & EF_IS_STYLUS_BUTTON)
    names.push_back("IS_STYLUS_BUTTON");
#if BUILDFLAG(IS_CHROMEOS)
  if (event_flags & EF_IS_CUSTOMIZED_FROM_BUTTON) {
    names.push_back("IS_CUSTOMIZED_FROM_BUTTON");
  }
#endif

  return names;
}

std::vector<std::string_view> MouseEventFlagsNames(int event_flags) {
  std::vector<std::string_view> names = EventFlagsNames(event_flags);
  if (!event_flags)
    return names;

  if (event_flags & EF_IS_DOUBLE_CLICK)
    names.push_back("IS_DOUBLE_CLICK");
  if (event_flags & EF_IS_TRIPLE_CLICK)
    names.push_back("IS_TRIPLE_CLICK");
  if (event_flags & EF_IS_NON_CLIENT)
    names.push_back("IS_NON_CLIENT");
  if (event_flags & EF_FROM_TOUCH)
    names.push_back("FROM_TOUCH");
  if (event_flags & EF_TOUCH_ACCESSIBILITY)
    names.push_back("TOUCH_ACCESSIBILITY");
  if (event_flags & EF_CURSOR_HIDE)
    names.push_back("CURSOR_HIDE");
  if (event_flags & EF_PRECISION_SCROLLING_DELTA)
    names.push_back("PRECISION_SCROLLING_DELTA");
  if (event_flags & EF_SCROLL_BY_PAGE)
    names.push_back("SCROLL_BY_PAGE");
  if (event_flags & EF_UNADJUSTED_MOUSE)
    names.push_back("UNADJUSTED_MOUSE");

  return names;
}

}  // namespace ui
