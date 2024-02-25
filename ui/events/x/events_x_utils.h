// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_EVENTS_X_UTILS_H_
#define UI_EVENTS_X_EVENTS_X_UTILS_H_

#include <stdint.h>


#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/events/x/events_x_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xinput.h"

namespace ui {

// Gets the EventType from a x11::Event.
EVENTS_X_EXPORT EventType EventTypeFromXEvent(const x11::Event& xev);

// Gets the EventFlags from a x11::KeyEvent.  `send_event` indicates if the
// event was sent by an X11 client instead of the server.
EVENTS_X_EXPORT int GetEventFlagsFromXKeyEvent(const x11::KeyEvent& key,
                                               bool send_event);

// Gets the EventFlags from a x11::Event.
EVENTS_X_EXPORT int EventFlagsFromXEvent(const x11::Event& xev);

// Gets the timestamp from a x11::Event.
EVENTS_X_EXPORT base::TimeTicks EventTimeFromXEvent(const x11::Event& xev);

// Gets the location from a x11::Event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
EVENTS_X_EXPORT gfx::Point EventLocationFromXEvent(const x11::Event& xev);

// Gets the location in native system coordinate space.
EVENTS_X_EXPORT gfx::Point EventSystemLocationFromXEvent(const x11::Event& xev);

// Returns the 'real' button for an event. The button reported in slave events
// does not take into account any remapping (e.g. using xmodmap), while the
// button reported in master events do. This is a utility function to always
// return the mapped button.
EVENTS_X_EXPORT int EventButtonFromXEvent(const x11::Event& xev);

// Returns the flags of the button that changed during a press/release.
EVENTS_X_EXPORT int GetChangedMouseButtonFlagsFromXEvent(const x11::Event& xev);

// Gets the mouse wheel offsets from a x11::Event.
EVENTS_X_EXPORT gfx::Vector2d GetMouseWheelOffsetFromXEvent(
    const x11::Event& xev);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
EVENTS_X_EXPORT float GetStylusForceFromXEvent(const x11::Event& xev);

// Gets the tilt x from a native_event. Value in degree. Default is 0.0.
EVENTS_X_EXPORT float GetStylusTiltXFromXEvent(const x11::Event& xev);

// Gets the tilt y from a native_event. Value in degree. Default is 0.0.
EVENTS_X_EXPORT float GetStylusTiltYFromXEvent(const x11::Event& xev);

// Gets the pointer details from an x11::Event.
EVENTS_X_EXPORT PointerDetails
GetStylusPointerDetailsFromXEvent(const x11::Event& xev);

// Gets the touch id from a x11::Event.
EVENTS_X_EXPORT int GetTouchIdFromXEvent(const x11::Event& xev);

// Gets the radius along the X/Y axis from a x11::Event. Default is 1.0.
EVENTS_X_EXPORT float GetTouchRadiusXFromXEvent(const x11::Event& xev);
EVENTS_X_EXPORT float GetTouchRadiusYFromXEvent(const x11::Event& xev);

// Gets the angle of the major axis away from the X axis. Default is 0.0.
EVENTS_X_EXPORT float GetTouchAngleFromXEvent(const x11::Event& xev);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
EVENTS_X_EXPORT float GetTouchForceFromXEvent(const x11::Event& xev);

// Gets the pointer details from an x11::Event.
EVENTS_X_EXPORT PointerDetails
GetTouchPointerDetailsFromXEvent(const x11::Event& xev);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset|, |y_offset| and |finger_count| can be NULL.
EVENTS_X_EXPORT bool GetScrollOffsetsFromXEvent(const x11::Event& xev,
                                                float* x_offset,
                                                float* y_offset,
                                                float* x_offset_ordinal,
                                                float* y_offset_ordinal,
                                                int* finger_count);

// Gets the fling velocity from a x11::Event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
EVENTS_X_EXPORT bool GetFlingDataFromXEvent(const x11::Event& xev,
                                            float* vx,
                                            float* vy,
                                            float* vx_ordinal,
                                            float* vy_ordinal,
                                            bool* is_cancel);

// Uses the XModifierStateWatcher to determine if alt is pressed or not.
EVENTS_X_EXPORT bool IsAltPressed();

// Proxies the XModifierStateWatcher::state() to return the current state of
// modifier keys.
EVENTS_X_EXPORT int GetModifierKeyState();

EVENTS_X_EXPORT void ResetTimestampRolloverCountersForTesting();

}  // namespace ui

#endif  // UI_EVENTS_X_EVENTS_X_UTILS_H_
