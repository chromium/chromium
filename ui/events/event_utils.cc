// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ui {

namespace {
int g_custom_event_types = ET_LAST;
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

bool IsValidTimebase(base::TimeTicks now, base::TimeTicks timestamp) {
  int64_t delta = (now - timestamp).InMilliseconds();
  return delta >= 0 && delta <= 60 * 1000;
}

void ValidateEventTimeClock(base::TimeTicks* timestamp) {
  // Some fraction of devices, across all platforms provide bogus event
  // timestamps. See https://crbug.com/650338#c1. Correct timestamps which are
  // clearly bogus.
  // TODO(861855): Replace this with an approach that doesn't require an extra
  // read of the current time per event.
  base::TimeTicks now = EventTimeForNow();
  if (!IsValidTimebase(now, *timestamp))
    *timestamp = now;
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
  base::TimeTicks time_stamp = EventTimeFromNative(native_event);
  base::TimeDelta delta = current_time - time_stamp;

  EventType type = EventTypeFromNative(native_event);
  switch (type) {
#if defined(OS_MACOSX)
    // On Mac, ET_SCROLL and ET_MOUSEWHEEL represent the same class of events.
    case ET_SCROLL:
#endif
    case ET_MOUSEWHEEL:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.MOUSE_WHEEL",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_MOVED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_MOVED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_PRESSED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_PRESSED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    case ET_TOUCH_RELEASED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Latency.OS.TOUCH_RELEASED",
          base::saturated_cast<int>(delta.InMicroseconds()), 1, 1000000, 50);
      return;
    default:
      return;
  }
}

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
  located_event->set_root_location_f(location_in_pixel_in_host);
}

const char* EventTypeName(EventType type) {
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

}  // namespace ui
