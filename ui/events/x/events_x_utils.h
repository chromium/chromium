// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_EVENTS_X_UTILS_H_
#define UI_EVENTS_X_EVENTS_X_UTILS_H_

#include <stdint.h>

#include <memory>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/pointer_details.h"
#include "ui/events/x/events_x_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

// Gets the EventType from a XEvent.
EVENTS_X_EXPORT EventType EventTypeFromXEvent(const XEvent& xev);

// Gets the EventFlags from a XEvent.
EVENTS_X_EXPORT int EventFlagsFromXEvent(const XEvent& xev);

// Gets the timestamp from a XEvent.
EVENTS_X_EXPORT base::TimeTicks EventTimeFromXEvent(const XEvent& xev);

// Gets the location from a XEvent.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
EVENTS_X_EXPORT gfx::Point EventLocationFromXEvent(const XEvent& xev);

// Gets the location in native system coordinate space.
EVENTS_X_EXPORT gfx::Point EventSystemLocationFromXEvent(const XEvent& xev);

// Returns the 'real' button for an event. The button reported in slave events
// does not take into account any remapping (e.g. using xmodmap), while the
// button reported in master events do. This is a utility function to always
// return the mapped button.
EVENTS_X_EXPORT int EventButtonFromXEvent(const XEvent& xev);

// Returns the flags of the button that changed during a press/release.
EVENTS_X_EXPORT int GetChangedMouseButtonFlagsFromXEvent(const XEvent& xev);

// Gets the mouse wheel offsets from a XEvent.
EVENTS_X_EXPORT gfx::Vector2d GetMouseWheelOffsetFromXEvent(const XEvent& xev);

// Gets the touch id from a XEvent.
EVENTS_X_EXPORT int GetTouchIdFromXEvent(const XEvent& xev);

// Gets the radius along the X/Y axis from a XEvent. Default is 1.0.
EVENTS_X_EXPORT float GetTouchRadiusXFromXEvent(const XEvent& xev);
EVENTS_X_EXPORT float GetTouchRadiusYFromXEvent(const XEvent& xev);

// Gets the angle of the major axis away from the X axis. Default is 0.0.
EVENTS_X_EXPORT float GetTouchAngleFromXEvent(const XEvent& xev);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
EVENTS_X_EXPORT float GetTouchForceFromXEvent(const XEvent& xev);

// Gets the pointer type from a native_event.
EVENTS_X_EXPORT EventPointerType
GetTouchPointerTypeFromXEvent(const XEvent& xev);

// Gets the pointer details from an XEvent.
EVENTS_X_EXPORT PointerDetails
GetTouchPointerDetailsFromXEvent(const XEvent& xev);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset|, |y_offset| and |finger_count| can be NULL.
EVENTS_X_EXPORT bool GetScrollOffsetsFromXEvent(const XEvent& xev,
                                                float* x_offset,
                                                float* y_offset,
                                                float* x_offset_ordinal,
                                                float* y_offset_ordinal,
                                                int* finger_count);

// Gets the fling velocity from a XEvent. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
EVENTS_X_EXPORT bool GetFlingDataFromXEvent(const XEvent& xev,
                                            float* vx,
                                            float* vy,
                                            float* vx_ordinal,
                                            float* vy_ordinal,
                                            bool* is_cancel);

// Uses the XModifierStateWatcher to determine if alt is pressed or not.
EVENTS_X_EXPORT bool IsAltPressed();

EVENTS_X_EXPORT void ResetTimestampRolloverCountersForTesting();

}  // namespace ui

#endif  // UI_EVENTS_X_EVENTS_X_UTILS_H_
