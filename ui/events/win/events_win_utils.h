// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_
#define UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_

#include <stdint.h>
#include <windows.h>

#include <memory>
#include <string>

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
class TimeTicks;
}

// Utility functions for Windows specific events code.
namespace ui {

enum class DomCode;

// Get the EventType from a native event.
EventType EventTypeFromMSG(const MSG& native_event);

// Get the EventFlags from a native event.
int EventFlagsFromMSG(const MSG& native_event);

// Get the timestamp from a native event.
// Note: This is not a pure function meaning that multiple applications on the
// same native event may return different values.
base::TimeTicks EventTimeFromMSG(const MSG& native_event);

// Get the location from a native event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
gfx::Point EventLocationFromMSG(const MSG& native_event);

// Gets the location in native system coordinate space.
gfx::Point EventSystemLocationFromMSG(const MSG& native_event);

// Returns the KeyboardCode from a native event.
KeyboardCode KeyboardCodeFromMSG(const MSG& native_event);

// Returns the DOM KeyboardEvent code (physical location in the
// keyboard) from a native event.
DomCode CodeFromMSG(const MSG& native_event);

// Returns true if the keyboard event is a character event rather than
// a keystroke event.
bool IsCharFromMSG(const MSG& native_event);

// Returns the flags of the button that changed during a press/release.
int GetChangedMouseButtonFlagsFromMSG(const MSG& native_event);

// Returns the detailed pointer information for mouse events.
PointerDetails GetMousePointerDetailsFromMSG(const MSG& native_event);

// Gets the mouse wheel offsets from a native event.
gfx::Vector2d GetMouseWheelOffsetFromMSG(const MSG& native_event);

// Returns a copy of |native_event|. Depending on the platform, this copy may
// need to be deleted with ReleaseCopiedMSGEvent().
MSG CopyMSGEvent(const MSG& native_event);

// Delete a |native_event| previously created by CopyMSGEvent().
void ReleaseCopiedMSGEvent(const MSG& native_event);

// Returns the detailed pointer information for touch events.
PointerDetails GetTouchPointerDetailsFromMSG(const MSG& native_event);

// Clear the touch id from bookkeeping if it is a release/cancel event.
void ClearTouchIdIfReleased(const MSG& native_event);

// Gets the fling velocity from a native event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
bool GetFlingData(const MSG& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled.
bool GetScrollOffsetsFromMSG(const MSG& native_event);

}  // namespace ui

#endif  // UI_EVENTS_WIN_EVENTS_WIN_UTILS_H_
