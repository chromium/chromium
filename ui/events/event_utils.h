// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_UTILS_H_
#define UI_EVENTS_EVENT_UTILS_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace gfx {
class Point;
class Vector2d;
}  // namespace gfx

namespace base {
class TimeTicks;
}

// Common functions to be used for all platforms except Android.
namespace ui {

class Event;
class MouseEvent;
enum class DomCode : uint32_t;

// Key used to store keyboard 'state' values in Event::Properties.
constexpr char kPropertyKeyboardState[] = "_keyevent_kbd_state_";

// Key used to store keyboard 'group' values in Event::Properties.
constexpr char kPropertyKeyboardGroup[] = "_keyevent_kbd_group_";

// Key used to store 'hardware key code' values in Event::Properties.
constexpr char kPropertyKeyboardHwKeyCode[] = "_keyevent_kbd_hw_keycode_";

// Key used to store mouse event flag telling EventType::kMouseExited must
// actually be interpreted as "crossing intermediate window" in blink context.
constexpr char kPropertyMouseCrossedIntermediateWindow[] =
    "_mouseevent_cros_window_";

// Returns a ui::Event wrapping a native event. Ownership of the returned value
// is transferred to the caller.
EVENTS_EXPORT std::unique_ptr<Event> EventFromNative(
    const PlatformEvent& native_event);

// Get the EventType from a native event.
EVENTS_EXPORT EventType EventTypeFromNative(const PlatformEvent& native_event);

// Get the EventFlags from a native event.
EVENTS_EXPORT int EventFlagsFromNative(const PlatformEvent& native_event);

// Get the timestamp from a native event.
// Note: This is not a pure function meaning that multiple applications on the
// same native event may return different values.
EVENTS_EXPORT base::TimeTicks EventTimeFromNative(
    const PlatformEvent& native_event);

// Get the timestamp to use for latency metrics from |native_event|.
// |current_time| is a timestamp returned by EventTimeForNow which will be
// compared to the |native_event| timestamp to calculate latency. This is
// different from EventTimeFromNative because on some platforms (eg. Windows)
// EventTimeFromNative returns a synthesized timestamp.
EVENTS_EXPORT base::TimeTicks EventLatencyTimeFromNative(
    const PlatformEvent& native_event,
    base::TimeTicks current_time);

// Get the location from a native event.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
EVENTS_EXPORT gfx::PointF EventLocationFromNative(
    const PlatformEvent& native_event);

// Gets the location in native system coordinate space.
EVENTS_EXPORT gfx::Point EventSystemLocationFromNative(
    const PlatformEvent& native_event);

// Returns the KeyboardCode from a native event.
EVENTS_EXPORT KeyboardCode
KeyboardCodeFromNative(const PlatformEvent& native_event);

// Returns the DOM KeyboardEvent code (physical location in the
// keyboard) from a native event.
EVENTS_EXPORT DomCode CodeFromNative(const PlatformEvent& native_event);

// Returns true if the keyboard event is a character event rather than
// a keystroke event.
EVENTS_EXPORT bool IsCharFromNative(const PlatformEvent& native_event);

// Returns the flags of the button that changed during a press/release.
EVENTS_EXPORT int GetChangedMouseButtonFlagsFromNative(
    const PlatformEvent& native_event);

// Returns the detailed pointer information for mouse events.
EVENTS_EXPORT PointerDetails
GetMousePointerDetailsFromNative(const PlatformEvent& native_event);

// Returns the movement vector associated with this mouse movement event.
EVENTS_EXPORT const gfx::Vector2dF& GetMouseMovementFromNative(
    const PlatformEvent& native_event);

// Gets the mouse wheel offsets from a native event.
EVENTS_EXPORT gfx::Vector2d GetMouseWheelOffset(
    const PlatformEvent& native_event);

// Gets the mouse wheel tick counts from a native event, with a value of 120
// representing a whole tick.
EVENTS_EXPORT gfx::Vector2d GetMouseWheelTick120ths(
    const PlatformEvent& native_event);

// Returns whether platform events should be copied when ui::Events are copied.
EVENTS_EXPORT bool ShouldCopyPlatformEvents();

// Creates a new, invalid event.
EVENTS_EXPORT PlatformEvent CreateInvalidPlatformEvent();

// Returns if the platform event is valid.
EVENTS_EXPORT bool IsPlatformEventValid(const PlatformEvent& platform_event);

// Returns the detailed pointer information for touch events.
EVENTS_EXPORT PointerDetails
GetTouchPointerDetailsFromNative(const PlatformEvent& native_event);

// Gets the fling velocity from a native event. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
EVENTS_EXPORT bool GetFlingData(const PlatformEvent& native_event,
                                float* vx,
                                float* vy,
                                float* vx_ordinal,
                                float* vy_ordinal,
                                bool* is_cancel);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset|, |y_offset| and |finger_count| can be NULL.
EVENTS_EXPORT bool GetScrollOffsets(const PlatformEvent& native_event,
                                    float* x_offset,
                                    float* y_offset,
                                    float* x_offset_ordinal,
                                    float* y_offset_ordinal,
                                    int* finger_count,
                                    EventMomentumPhase* momentum_phase);

// Returns whether natural scrolling should be used for touchpad.
EVENTS_EXPORT bool ShouldDefaultToNaturalScroll();

// Returns whether or not the internal display produces touch events.
EVENTS_EXPORT display::Display::TouchSupport GetInternalDisplayTouchSupport();

EVENTS_EXPORT void ComputeEventLatencyOS(const PlatformEvent& native_event);

EVENTS_EXPORT void ComputeEventLatencyOS(ui::EventType type,
                                         base::TimeTicks time_stamp,
                                         base::TimeTicks current_time);

#if BUILDFLAG(IS_WIN)
// Like ComputeEventLatencyOS, but for events whose timestamp comes from a
// TOUCHINPUT structure instead of PlatformEvent.
EVENTS_EXPORT void ComputeEventLatencyOSFromTOUCHINPUT(
    ui::EventType event_type,
    TOUCHINPUT touch_input,
    base::TimeTicks current_time);

// Like ComputeEventLatencyOS, but for events whose timestamp comes from a
// POINTER_INFO structure instead of PlatformEvent.
EVENTS_EXPORT void ComputeEventLatencyOSFromPOINTER_INFO(
    ui::EventType event_type,
    POINTER_INFO pointer_info,
    base::TimeTicks current_time);

EVENTS_EXPORT int GetModifiersFromKeyState();

// Returns true if |message| identifies a mouse event that was generated as the
// result of a touch event.
EVENTS_EXPORT bool IsMouseEventFromTouch(UINT message);

// Converts scan code and lParam each other.  The scan code
// representing an extended key contains 0xE000 bits.
EVENTS_EXPORT uint16_t GetScanCodeFromLParam(LPARAM lParam);
EVENTS_EXPORT LPARAM GetLParamFromScanCode(uint16_t scan_code);

// Creates a CHROME_MSG from the given KeyEvent if there is no native_event.
EVENTS_EXPORT CHROME_MSG MSGFromKeyEvent(KeyEvent* key_event,
                                         HWND hwnd = nullptr);
EVENTS_EXPORT KeyEvent KeyEventFromMSG(const CHROME_MSG& msg);
EVENTS_EXPORT MouseEvent MouseEventFromMSG(const CHROME_MSG& msg);
EVENTS_EXPORT MouseWheelEvent MouseWheelEventFromMSG(const CHROME_MSG& msg);

#endif  // BUILDFLAG(IS_WIN)

// Registers a custom event type.
EVENTS_EXPORT int RegisterCustomEventType();

// Updates the location of |located_event| from |current_window_origin| to be in
// |target_window_origin|'s coordinate system so that it can be dispatched to a
// window based on |target_window_origin|.
EVENTS_EXPORT void ConvertEventLocationToTargetWindowLocation(
    const gfx::Point& target_window_origin,
    const gfx::Point& current_window_origin,
    ui::LocatedEvent* located_event);

// The following utilities are useful for debugging and tracing.

// Returns a string description of an event type.
EVENTS_EXPORT std::string_view EventTypeName(EventType type);

// Returns a vector of string representations of EventFlags.
EVENTS_EXPORT std::vector<std::string_view> EventFlagsNames(int event_flags);

// Returns a a vector of string representations of KeyEventFlags.
EVENTS_EXPORT std::vector<std::string_view> KeyEventFlagsNames(int event_flags);

// Returns a a vector of string representations of MouseEventFlags.
EVENTS_EXPORT std::vector<std::string_view> MouseEventFlagsNames(
    int event_flags);
}  // namespace ui

#endif  // UI_EVENTS_EVENT_UTILS_H_
