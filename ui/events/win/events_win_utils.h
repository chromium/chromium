// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_
#define UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_

#include <stdint.h>

#include "base/win/windows_types.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Vector2d;
}  // namespace gfx

namespace base {
class TickClock;
class TimeTicks;
}

// Utility functions for Windows specific events code.
namespace ui {

enum class DomCode : uint32_t;

// Get the EventType from a native event.
EVENTS_EXPORT EventType EventTypeFromMSG(const CHROME_MSG& native_event);

// Get the EventFlags from a native event.
EVENTS_EXPORT int EventFlagsFromMSG(const CHROME_MSG& native_event);

// Get the timestamp from a native event.
// Note: This is not a pure function meaning that multiple applications on the
// same native event may return different values.
EVENTS_EXPORT base::TimeTicks EventTimeFromMSG(const CHROME_MSG& native_event);

// Convert |event_time|, a count of milliseconds from the clock used by
// ::GetTickCount(), to a value comparable to the base::TimeTicks clock.
// |current_time| is a value returned in the recent past by EventTimeForNow,
// which will be compared to the current result of ::GetTickCount() for
// calibration.
EVENTS_EXPORT base::TimeTicks EventLatencyTimeFromTickClock(
    DWORD event_time,
    base::TimeTicks current_time);

// Convert |event_time|, a timestamp from the clock used by
// ::QueryPerformanceCounter(), to a value comparable to the base::TimeTicks
// clock. Must not be called if base::TimeTicks::IsHighResolution() returns
// false.
EVENTS_EXPORT base::TimeTicks EventLatencyTimeFromPerformanceCounter(
    UINT64 event_time);

// Get the location from a native event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
EVENTS_EXPORT gfx::Point EventLocationFromMSG(const CHROME_MSG& native_event);

// Gets the location in native system coordinate space.
EVENTS_EXPORT gfx::Point EventSystemLocationFromMSG(
    const CHROME_MSG& native_event);

// Returns the KeyboardCode from a native event.
EVENTS_EXPORT KeyboardCode KeyboardCodeFromMSG(const CHROME_MSG& native_event);

// Returns the DOM KeyboardEvent code (physical location in the
// keyboard) from a native event.
EVENTS_EXPORT DomCode CodeFromMSG(const CHROME_MSG& native_event);

// Returns true if the keyboard event is a character event rather than
// a keystroke event.
EVENTS_EXPORT bool IsCharFromMSG(const CHROME_MSG& native_event);

// Returns the flags of the button that changed during a press/release.
EVENTS_EXPORT int GetChangedMouseButtonFlagsFromMSG(
    const CHROME_MSG& native_event);

// Returns the detailed pointer information for mouse events.
EVENTS_EXPORT PointerDetails
GetMousePointerDetailsFromMSG(const CHROME_MSG& native_event);

// Gets the mouse wheel offsets from a native event.
EVENTS_EXPORT gfx::Vector2d GetMouseWheelOffsetFromMSG(
    const CHROME_MSG& native_event);

// Returns the detailed pointer information for touch events.
EVENTS_EXPORT PointerDetails
GetTouchPointerDetailsFromMSG(const CHROME_MSG& native_event);

// Clear the touch id from bookkeeping if it is a release/cancel event.
EVENTS_EXPORT void ClearTouchIdIfReleased(const CHROME_MSG& native_event);

// Gets the fling velocity from a native event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
EVENTS_EXPORT bool GetFlingData(const CHROME_MSG& native_event,
                                float* vx,
                                float* vy,
                                float* vx_ordinal,
                                float* vy_ordinal,
                                bool* is_cancel);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled.
EVENTS_EXPORT bool GetScrollOffsetsFromMSG(const CHROME_MSG& native_event);

// Makes EventLatencyTimeFromTickClock call the given |clock| to find the
// current time ticks. If |clock| is nullptr, it will call ::GetTickCount(),
// which is the default.
EVENTS_EXPORT void SetEventLatencyTickClockForTesting(
    const base::TickClock* clock);

}  // namespace ui

#endif  // UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_
