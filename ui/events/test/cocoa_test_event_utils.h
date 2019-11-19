// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_COCOA_TEST_EVENT_UTILS_H_
#define UI_EVENTS_TEST_COCOA_TEST_EVENT_UTILS_H_

#import <Cocoa/Cocoa.h>
#import <objc/objc-class.h>

#include <utility>

#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace cocoa_test_event_utils {

// Converts |window_point| in |window| coordinates (origin at bottom left of
// window frame) to a point in global display coordinates for a CGEvent (origin
// at top left of primary screen).
CGPoint ScreenPointFromWindow(NSPoint window_point, NSWindow* window);

// Converts |event| to an NSEvent with a timestamp from ui::EventTimeForNow(),
// and attaches |window| to it.
NSEvent* AttachWindowToCGEvent(CGEventRef event, NSWindow* window);

// Create synthetic mouse events for testing. Currently these are very
// basic, flesh out as needed.  Points are all in window coordinates;
// where the window is not specified, coordinate system is undefined
// (but will be repeated when the event is queried).
NSEvent* MouseEventAtPoint(NSPoint point, NSEventType type,
                           NSUInteger modifiers);
NSEvent* MouseEventWithType(NSEventType type, NSUInteger modifiers);
NSEvent* MouseEventAtPointInWindow(NSPoint point,
                                   NSEventType type,
                                   NSWindow* window,
                                   NSUInteger clickCount);
NSEvent* RightMouseDownAtPoint(NSPoint point);
NSEvent* RightMouseDownAtPointInWindow(NSPoint point, NSWindow* window);
NSEvent* LeftMouseDownAtPoint(NSPoint point);
NSEvent* LeftMouseDownAtPointInWindow(NSPoint point, NSWindow* window);

// Return a mouse down and an up event with the given |clickCount| at
// |view|'s midpoint.
std::pair<NSEvent*, NSEvent*> MouseClickInView(NSView* view,
                                               NSUInteger clickCount);

// Return a right mouse down and an up event with the given |clickCount| at
// |view|'s midpoint.
std::pair<NSEvent*, NSEvent*> RightMouseClickInView(NSView* view,
                                                    NSUInteger clickCount);

// Creates a test scroll event. |has_precise_deltas| determines the value of
// -[NSEvent hasPreciseScrollingDeltas] - usually NO for a mouse wheel and YES
// for a trackpad. If |window| is nil, |location| is assumed to be AppKit screen
// coordinates (origin in bottom left of primary screen).
NSEvent* TestScrollEvent(NSPoint location,
                         NSWindow* window,
                         CGFloat delta_x,
                         CGFloat delta_y,
                         bool has_precise_deltas,
                         NSEventPhase event_phase,
                         NSEventPhase momentum_phase);

// Returns a key event with the given character.
NSEvent* KeyEventWithCharacter(unichar c);

// Returns a key event with the given type and modifier flags.
NSEvent* KeyEventWithType(NSEventType event_type, NSUInteger modifiers);

// Returns a key event with the given key code, type, and modifier flags.
NSEvent* KeyEventWithKeyCode(unsigned short key_code,
                             unichar c,
                             NSEventType event_type,
                             NSUInteger modifiers);

// Returns a key event for pressing or releasing a modifier key (aka
// NSFlagsChanged). For example |key_code| == kVK_Shift with (|modifiers| &
// NSShiftKeyMask) != 0 means Shift is pressed and |key_code| == kVK_Shift
// with (|modifiers| & NSShiftKeyMask) == 0 means Shift is released.
NSEvent* KeyEventWithModifierOnly(unsigned short key_code,
                                  NSUInteger modifiers);

// Returns a mouse enter event.
NSEvent* EnterEvent(NSPoint point = NSZeroPoint, NSWindow* window = nil);

// Returns a mouse exit event.
NSEvent* ExitEvent(NSPoint point = NSZeroPoint, NSWindow* window = nil);

// Return an "other" event with the given type.
NSEvent* OtherEventWithType(NSEventType event_type);

// Time interval since system startup. Tests shouldn't rely on this.
NSTimeInterval TimeIntervalSinceSystemStartup();

// Creates a key event in a particular window.
NSEvent* SynthesizeKeyEvent(NSWindow* window,
                            bool keyDown,
                            ui::KeyboardCode keycode,
                            NSUInteger flags,
                            ui::DomKey dom_key = ui::DomKey::NONE);

}  // namespace cocoa_test_event_utils

#endif  // UI_EVENTS_TEST_COCOA_TEST_EVENT_UTILS_H_
