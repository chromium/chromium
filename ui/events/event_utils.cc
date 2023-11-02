// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <limits>
#include <map>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/string_helpers.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/events/win/events_win_utils.h"
#endif

namespace ui {

namespace {

int g_custom_event_types = ET_LAST;

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
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED:
      event = std::make_unique<KeyEvent>(native_event);
      break;

    case ET_MOUSE_PRESSED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
      event = std::make_unique<MouseEvent>(native_event);
      break;

    case ET_MOUSEWHEEL:
      event = std::make_unique<MouseWheelEvent>(native_event);
      break;

    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL:
      event = std::make_unique<ScrollEvent>(native_event);
      break;

    case ET_TOUCH_RELEASED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_MOVED:
    case ET_TOUCH_CANCELLED:
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
    // On Mac, ET_SCROLL and ET_MOUSEWHEEL represent the same class of events.
    case ET_SCROLL:
#endif
    case ET_MOUSEWHEEL:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kMouseWheelEventName, delta);
      // Do not record traces for wheel events to avoid spam.
      return;
    case ET_TOUCH_MOVED:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchMovedEventName, delta);
      // Do not record traces for move events to avoid spam.
      return;
    case ET_TOUCH_PRESSED:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchPressedEventName, delta);
      RecordEventLatencyTrace(kTouchPressedEventName, time_stamp, current_time);
      return;
    case ET_TOUCH_RELEASED:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchReleasedEventName, delta);
      RecordEventLatencyTrace(kTouchReleasedEventName, time_stamp,
                              current_time);
      return;
    case ET_TOUCH_CANCELLED:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kTouchCancelledEventName, delta);
      RecordEventLatencyTrace(kTouchCancelledEventName, time_stamp,
                              current_time);
      return;
    case ET_KEY_PRESSED:
      UMA_HISTOGRAM_EVENT_LATENCY_TIMES(kKeyPressedEventName, delta);
      RecordEventLatencyTrace(kKeyPressedEventName, time_stamp, current_time);
      return;
    case ET_MOUSE_PRESSED:
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

base::StringPiece EventTypeName(EventType type) {
  if (type >= ET_LAST)
    return "";

#define CASE_TYPE(t) \
  case t:            \
    return #t

  switch (type) {
    CASE_TYPE(ET_UNKNOWN);
    CASE_TYPE(ET_MOUSE_PRESSED);
    CASE_TYPE(ET_MOUSE_DRAGGED);
    CASE_TYPE(ET_MOUSE_RELEASED);
    CASE_TYPE(ET_MOUSE_MOVED);
    CASE_TYPE(ET_MOUSE_ENTERED);
    CASE_TYPE(ET_MOUSE_EXITED);
    CASE_TYPE(ET_KEY_PRESSED);
    CASE_TYPE(ET_KEY_RELEASED);
    CASE_TYPE(ET_MOUSEWHEEL);
    CASE_TYPE(ET_MOUSE_CAPTURE_CHANGED);
    CASE_TYPE(ET_TOUCH_RELEASED);
    CASE_TYPE(ET_TOUCH_PRESSED);
    CASE_TYPE(ET_TOUCH_MOVED);
    CASE_TYPE(ET_TOUCH_CANCELLED);
    CASE_TYPE(ET_DROP_TARGET_EVENT);
    CASE_TYPE(ET_GESTURE_SCROLL_BEGIN);
    CASE_TYPE(ET_GESTURE_SCROLL_END);
    CASE_TYPE(ET_GESTURE_SCROLL_UPDATE);
    CASE_TYPE(ET_GESTURE_SHOW_PRESS);
    CASE_TYPE(ET_GESTURE_TAP);
    CASE_TYPE(ET_GESTURE_TAP_DOWN);
    CASE_TYPE(ET_GESTURE_TAP_CANCEL);
    CASE_TYPE(ET_GESTURE_BEGIN);
    CASE_TYPE(ET_GESTURE_END);
    CASE_TYPE(ET_GESTURE_TWO_FINGER_TAP);
    CASE_TYPE(ET_GESTURE_PINCH_BEGIN);
    CASE_TYPE(ET_GESTURE_PINCH_END);
    CASE_TYPE(ET_GESTURE_PINCH_UPDATE);
    CASE_TYPE(ET_GESTURE_SHORT_PRESS);
    CASE_TYPE(ET_GESTURE_LONG_PRESS);
    CASE_TYPE(ET_GESTURE_LONG_TAP);
    CASE_TYPE(ET_GESTURE_SWIPE);
    CASE_TYPE(ET_GESTURE_TAP_UNCONFIRMED);
    CASE_TYPE(ET_GESTURE_DOUBLE_TAP);
    CASE_TYPE(ET_SCROLL);
    CASE_TYPE(ET_SCROLL_FLING_START);
    CASE_TYPE(ET_SCROLL_FLING_CANCEL);
    CASE_TYPE(ET_CANCEL_MODE);
    CASE_TYPE(ET_UMA_DATA);
    case ET_LAST:
      NOTREACHED();
      return "";
      // Don't include default, so that we get an error when new type is added.
  }
#undef CASE_TYPE

  NOTREACHED();
  return "";
}

std::vector<base::StringPiece> EventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names;
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

std::vector<base::StringPiece> KeyEventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names = EventFlagsNames(event_flags);
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

  return names;
}

std::vector<base::StringPiece> MouseEventFlagsNames(int event_flags) {
  std::vector<base::StringPiece> names = EventFlagsNames(event_flags);
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
