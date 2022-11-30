// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BASE_EVENT_UTILS_H_
#define UI_EVENTS_BASE_EVENT_UTILS_H_

#include <stdint.h>

#include "base/time/tick_clock.h"
#include "ui/events/events_base_export.h"

namespace base {
class TimeTicks;
}

// Common functions to be used for all platforms.
namespace ui {

// Generate an unique identifier for events.
EVENTS_BASE_EXPORT uint32_t GetNextTouchEventId();

// Checks if |flags| contains system key modifiers.
EVENTS_BASE_EXPORT bool IsSystemKeyModifier(int flags);

// Create a timestamp based on the current time.
EVENTS_BASE_EXPORT base::TimeTicks EventTimeForNow();

// Overrides the clock used by EventTimeForNow for testing.
// This doesn't take the ownership of the clock.
EVENTS_BASE_EXPORT void SetEventTickClockForTesting(
    const base::TickClock* tick_clock);

// Converts an event timestamp ticks to seconds (floating point representation).
// WARNING: This should only be used when interfacing with platform code that
// does not use base::Time* types.
EVENTS_BASE_EXPORT double EventTimeStampToSeconds(base::TimeTicks time_stamp);

// Converts an event timestamp in seconds to TimeTicks.
// WARNING: This should only be used when interfacing with platform code that
// does not use base::Time* types.
EVENTS_BASE_EXPORT base::TimeTicks EventTimeStampFromSeconds(
    double time_stamp_seconds);

// Returns false if an event timestamp is clearly bogus given that the event
// was generated a short time before |now|. Some fraction of devices, across
// all platforms provide bogus event timestamps. See
// https://crbug.com/650338#c1.
EVENTS_BASE_EXPORT bool IsValidTimebase(base::TimeTicks now,
                                        base::TimeTicks timestamp);

// Ensures that the event timestamp values are coming from the same underlying
// monotonic clock as base::TimeTicks::Now() and if it is not then falls
// back to using the current ticks for event timestamp.
EVENTS_BASE_EXPORT void ValidateEventTimeClock(base::TimeTicks* timestamp);

}  // namespace ui

#endif  // UI_EVENTS_BASE_EVENT_UTILS_H_
