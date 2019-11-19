// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/events/test/cocoa_test_event_utils.h"

#include <stdint.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/time/time.h"
#include "ui/events/base_event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace cocoa_test_event_utils {

CGPoint ScreenPointFromWindow(NSPoint window_point, NSWindow* window) {
  NSRect window_rect = NSMakeRect(window_point.x, window_point.y, 0, 0);
  NSPoint screen_point = window
                             ? [window convertRectToScreen:window_rect].origin
                             : window_rect.origin;
  CGFloat primary_screen_height =
      NSHeight([[[NSScreen screens] firstObject] frame]);
  screen_point.y = primary_screen_height - screen_point.y;
  return NSPointToCGPoint(screen_point);
}

NSEvent* AttachWindowToCGEvent(CGEventRef event, NSWindow* window) {
  // -[NSEvent locationInWindow] changes from screen coordinates to window
  // coordinates when a window is attached to the mouse event. -[NSEvent
  // eventWithCGEvent:] handles the Quartz -> AppKit coordinate flipping, but
  // not the offset. Unfortunately -eventWithCGEvent: uses the *screen* height
  // to flip, not the window height (it doesn't know about the window yet). So
  // to get the correct -[NSEvent locationInWindow], anticipate the bogus screen
  // flip that eventWithCGEvent: will do. This is yuck, but NSEvent does not
  // provide a way to generate test scrolling events any other way. Fortunately,
  // once you do all the algebra, all we need to do here is offset by the window
  // origin, but in different directions for x/y.
  CGPoint location = CGEventGetLocation(event);
  location.y += NSMinY([window frame]);
  location.x -= NSMinX([window frame]);
  CGEventSetLocation(event, location);

  // These CGEventFields were made public in the 10.7 SDK, but don't help to
  // populate the -[NSEvent window] pointer when creating an event with
  // +[NSEvent eventWithCGEvent:]. Set that separately, using reflection.
  CGEventSetIntegerValueField(event, kCGMouseEventWindowUnderMousePointer,
                              [window windowNumber]);
  CGEventSetIntegerValueField(
      event, kCGMouseEventWindowUnderMousePointerThatCanHandleThisEvent,
      [window windowNumber]);

  // CGEventTimestamp is nanoseconds since system startup as a 64-bit integer.
  // Use EventTimeForNow() so that it can be mocked for tests.
  CGEventTimestamp timestamp =
      (ui::EventTimeForNow() - base::TimeTicks()).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  CGEventSetTimestamp(event, timestamp);

  NSEvent* ns_event = [NSEvent eventWithCGEvent:event];
  DCHECK_EQ(nil, [ns_event window]);  // Verify assumptions.
  [ns_event setValue:window forKey:@"_window"];
  DCHECK_EQ(window, [ns_event window]);

  return ns_event;
}

NSEvent* MouseEventAtPoint(NSPoint point, NSEventType type,
                           NSUInteger modifiers) {
  if (type == NSOtherMouseUp) {
    // To synthesize middle clicks we need to create a CGEvent with the
    // "center" button flags so that our resulting NSEvent will have the
    // appropriate buttonNumber field. NSEvent provides no way to create a
    // mouse event with a buttonNumber directly.
    CGPoint location = { point.x, point.y };
    CGEventRef cg_event = CGEventCreateMouseEvent(NULL, kCGEventOtherMouseUp,
                                                  location,
                                                  kCGMouseButtonCenter);
    // Also specify the modifiers for the middle click case. This makes this
    // test resilient to external modifiers being pressed.
    CGEventSetFlags(cg_event, static_cast<CGEventFlags>(modifiers));
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
    CFRelease(cg_event);
    return event;
  }
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:TimeIntervalSinceSystemStartup()
                        windowNumber:0
                             context:nil
                         eventNumber:0
                          clickCount:1
                            pressure:1.0];
}

NSEvent* MouseEventWithType(NSEventType type, NSUInteger modifiers) {
  return MouseEventAtPoint(NSZeroPoint, type, modifiers);
}

NSEvent* MouseEventAtPointInWindow(NSPoint point,
                                   NSEventType type,
                                   NSWindow* window,
                                   NSUInteger clickCount) {
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:0
                           timestamp:TimeIntervalSinceSystemStartup()
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:clickCount
                            pressure:1.0];
}

NSEvent* RightMouseDownAtPointInWindow(NSPoint point, NSWindow* window) {
  return MouseEventAtPointInWindow(point, NSRightMouseDown, window, 1);
}

NSEvent* RightMouseDownAtPoint(NSPoint point) {
  return RightMouseDownAtPointInWindow(point, nil);
}

NSEvent* LeftMouseDownAtPointInWindow(NSPoint point, NSWindow* window) {
  return MouseEventAtPointInWindow(point, NSLeftMouseDown, window, 1);
}

NSEvent* LeftMouseDownAtPoint(NSPoint point) {
  return LeftMouseDownAtPointInWindow(point, nil);
}

std::pair<NSEvent*,NSEvent*> MouseClickInView(NSView* view,
                                              NSUInteger clickCount) {
  const NSRect bounds = [view convertRect:[view bounds] toView:nil];
  const NSPoint mid_point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  NSEvent* down = MouseEventAtPointInWindow(mid_point, NSLeftMouseDown,
                                            [view window], clickCount);
  NSEvent* up = MouseEventAtPointInWindow(mid_point, NSLeftMouseUp,
                                          [view window], clickCount);
  return std::make_pair(down, up);
}

std::pair<NSEvent*, NSEvent*> RightMouseClickInView(NSView* view,
                                                    NSUInteger clickCount) {
  const NSRect bounds = [view convertRect:[view bounds] toView:nil];
  const NSPoint mid_point = NSMakePoint(NSMidX(bounds), NSMidY(bounds));
  NSEvent* down = MouseEventAtPointInWindow(mid_point, NSRightMouseDown,
                                            [view window], clickCount);
  NSEvent* up = MouseEventAtPointInWindow(mid_point, NSRightMouseUp,
                                          [view window], clickCount);
  return std::make_pair(down, up);
}

NSEvent* TestScrollEvent(NSPoint window_point,
                         NSWindow* window,
                         CGFloat delta_x,
                         CGFloat delta_y,
                         bool has_precise_deltas,
                         NSEventPhase event_phase,
                         NSEventPhase momentum_phase) {
  const uint32_t wheel_count = 2;
  int32_t wheel1 = static_cast<int>(delta_y);
  int32_t wheel2 = static_cast<int>(delta_x);
  CGScrollEventUnit units =
      has_precise_deltas ? kCGScrollEventUnitPixel : kCGScrollEventUnitLine;
  base::ScopedCFTypeRef<CGEventRef> scroll(CGEventCreateScrollWheelEvent(
      nullptr, units, wheel_count, wheel1, wheel2));
  CGEventSetLocation(scroll, ScreenPointFromWindow(window_point, window));

  // Always set event flags, otherwise +[NSEvent eventWithCGEvent:] populates
  // flags from current keyboard state which can make tests flaky.
  CGEventSetFlags(scroll, static_cast<CGEventFlags>(0));

  if (has_precise_deltas) {
    // kCGScrollWheelEventIsContinuous is -[NSEvent hasPreciseScrollingDeltas].
    // CGEventTypes.h says it should be non-zero for pixel-based scrolling.
    // Verify that CGEventCreateScrollWheelEvent() set it.
    DCHECK_EQ(1, CGEventGetIntegerValueField(scroll,
                                             kCGScrollWheelEventIsContinuous));
  }

  // Don't set phase information when neither.
  if (event_phase != NSEventPhaseNone || momentum_phase != NSEventPhaseNone) {
    // AppKit conflates CGScrollPhase (bitmask flags) and CGMomentumScrollPhase
    // (an enum) into NSEventPhase, where it is used for both -[NSEvent phase]
    // and -[NSEvent momentumPhase]. Do a reverse mapping here.
    int cg_event_phase = 0;
    if (event_phase & NSEventPhaseBegan)
      cg_event_phase |= kCGScrollPhaseBegan;
    if (event_phase & NSEventPhaseChanged)
      cg_event_phase |= kCGScrollPhaseChanged;
    if (event_phase & NSEventPhaseEnded)
      cg_event_phase |= kCGScrollPhaseEnded;
    if (event_phase & NSEventPhaseCancelled)
      cg_event_phase |= kCGScrollPhaseCancelled;
    if (event_phase & NSEventPhaseMayBegin)
      cg_event_phase |= kCGScrollPhaseMayBegin;

    CGMomentumScrollPhase cg_momentum_phase = kCGMomentumScrollPhaseNone;
    switch (momentum_phase) {
      case NSEventPhaseNone:
        break;
      case NSEventPhaseBegan:
        cg_momentum_phase = kCGMomentumScrollPhaseBegin;
        break;
      case NSEventPhaseChanged:
        cg_momentum_phase = kCGMomentumScrollPhaseContinue;
        break;
      case NSEventPhaseEnded:
        cg_momentum_phase = kCGMomentumScrollPhaseEnd;
        break;
      default:
        // Those are the only 4 options for CGMomentumScrollPhase. If something
        // else was provided it should probably never appear on an NSEvent.
        NOTREACHED();
    }
    CGEventSetIntegerValueField(scroll, kCGScrollWheelEventScrollPhase,
                                cg_event_phase);
    CGEventSetIntegerValueField(scroll, kCGScrollWheelEventMomentumPhase,
                                cg_momentum_phase);
  }
  NSEvent* event = AttachWindowToCGEvent(scroll, window);
  DCHECK_EQ(has_precise_deltas, [event hasPreciseScrollingDeltas]);
  DCHECK_EQ(event_phase, [event phase]);
  DCHECK_EQ(momentum_phase, [event momentumPhase]);
  DCHECK_EQ(window_point.x, [event locationInWindow].x);
  DCHECK_EQ(window_point.y, [event locationInWindow].y);
  return event;
}

NSEvent* KeyEventWithCharacter(unichar c) {
  return KeyEventWithKeyCode(0, c, NSKeyDown, 0);
}

NSEvent* KeyEventWithType(NSEventType event_type, NSUInteger modifiers) {
  return KeyEventWithKeyCode(0x78, 'x', event_type, modifiers);
}

NSEvent* KeyEventWithKeyCode(unsigned short key_code,
                             unichar c,
                             NSEventType event_type,
                             NSUInteger modifiers) {
  NSString* chars = [NSString stringWithCharacters:&c length:1];
  return [NSEvent keyEventWithType:event_type
                          location:NSZeroPoint
                     modifierFlags:modifiers
                         timestamp:TimeIntervalSinceSystemStartup()
                      windowNumber:0
                           context:nil
                        characters:chars
       charactersIgnoringModifiers:chars
                         isARepeat:NO
                           keyCode:key_code];
}

NSEvent* KeyEventWithModifierOnly(unsigned short key_code,
                                  NSUInteger modifiers) {
  return [NSEvent keyEventWithType:NSFlagsChanged
                          location:NSZeroPoint
                     modifierFlags:modifiers
                         timestamp:TimeIntervalSinceSystemStartup()
                      windowNumber:0
                           context:nil
                        characters:@""
       charactersIgnoringModifiers:@""
                         isARepeat:NO
                           keyCode:key_code];
}

static NSEvent* EnterExitEventWithType(NSPoint point,
                                       NSEventType event_type,
                                       NSWindow* window) {
  return [NSEvent enterExitEventWithType:event_type
                                location:point
                           modifierFlags:0
                               timestamp:TimeIntervalSinceSystemStartup()
                            windowNumber:[window windowNumber]
                                 context:nil
                             eventNumber:0
                          trackingNumber:0
                                userData:NULL];
}

NSEvent* EnterEvent(NSPoint point, NSWindow* window) {
  return EnterExitEventWithType(point, NSMouseEntered, window);
}

NSEvent* ExitEvent(NSPoint point, NSWindow* window) {
  return EnterExitEventWithType(point, NSMouseExited, window);
}

NSEvent* OtherEventWithType(NSEventType event_type) {
  return [NSEvent otherEventWithType:event_type
                            location:NSZeroPoint
                       modifierFlags:0
                           timestamp:TimeIntervalSinceSystemStartup()
                        windowNumber:0
                             context:nil
                             subtype:0
                               data1:0
                               data2:0];
}

NSTimeInterval TimeIntervalSinceSystemStartup() {
  base::TimeDelta time_elapsed = ui::EventTimeForNow() - base::TimeTicks();
  return time_elapsed.InSecondsF();
}

NSEvent* SynthesizeKeyEvent(NSWindow* window,
                            bool keyDown,
                            ui::KeyboardCode keycode,
                            NSUInteger flags,
                            ui::DomKey dom_key) {
  // If caps lock is set for an alpha keycode, treat it as if shift was pressed.
  // Note on Mac (unlike other platforms) shift while caps is down does not go
  // back to lowercase.
  if (keycode >= ui::VKEY_A && keycode <= ui::VKEY_Z &&
      (flags & NSAlphaShiftKeyMask))
    flags |= NSShiftKeyMask;

  // Clear caps regardless -- MacKeyCodeForWindowsKeyCode doesn't implement
  // logic to support it.
  flags &= ~NSAlphaShiftKeyMask;

  // Call sites may generate unicode character events with an undefined
  // keycode. Since it's not feasible to determine the correct keycode for
  // each unicode character, we use a dummy keycode corresponding to key 'A'.
  if (dom_key.IsCharacter() && keycode == ui::VKEY_UNKNOWN)
    keycode = ui::VKEY_A;

  unichar character;
  unichar shifted_character;
  int macKeycode = ui::MacKeyCodeForWindowsKeyCode(
      keycode, flags, &shifted_character, &character);

  if (macKeycode < 0)
    return nil;

  // If an explicit unicode character is provided, use that instead of the one
  // derived from the keycode.
  if (dom_key.IsCharacter())
    shifted_character = dom_key.ToCharacter();

  // Note that, in line with AppKit's documentation (and tracing "real" events),
  // -[NSEvent charactersIngoringModifiers]" are "the characters generated by
  // the receiving key event as if no modifier key (except for Shift)".
  // So |charactersIgnoringModifiers| uses |shifted_character|.
  NSString* charactersIgnoringModifiers =
      [[[NSString alloc] initWithCharacters:&shifted_character
                                     length:1] autorelease];

  // Control + [Shift] Tab is special.
  if (keycode == ui::VKEY_TAB && (flags & NSControlKeyMask)) {
    if (flags & NSShiftKeyMask) {
      charactersIgnoringModifiers = @"\x19";
    } else {
      charactersIgnoringModifiers = @"\x9";
    }
  }

  NSString* characters;
  // The following were determined empirically on OSX 10.9.
  if (flags & NSControlKeyMask) {
    // If Ctrl is pressed, Cocoa always puts an empty string into |characters|.
    characters = [NSString string];
  } else if (flags & NSCommandKeyMask) {
    // If Cmd is pressed, Cocoa puts a lowercase character into |characters|,
    // regardless of Shift. If, however, Alt is also pressed then shift *is*
    // preserved, but re-mappings for Alt are not implemented. Although we still
    // need to support Alt for things like Alt+Left/Right which don't care.
    characters =
        [[[NSString alloc] initWithCharacters:&character length:1] autorelease];
  } else {
    // If just Shift or nothing is pressed, |characters| will match
    // |charactersIgnoringModifiers|. Alt puts a special character into
    // |characters| (not |charactersIgnoringModifiers|), but they're not mapped
    // here.
    characters = charactersIgnoringModifiers;
  }

  NSEventType type = (keyDown ? NSKeyDown : NSKeyUp);

  // Modifier keys generate NSFlagsChanged event rather than
  // NSKeyDown/NSKeyUp events.
  if (keycode == ui::VKEY_CONTROL || keycode == ui::VKEY_SHIFT ||
      keycode == ui::VKEY_MENU || keycode == ui::VKEY_COMMAND)
    type = NSFlagsChanged;

  // For events other than mouse moved, [event locationInWindow] is
  // UNDEFINED if the event is not NSMouseMoved.  Thus, the (0,0)
  // location should be fine.
  NSEvent* event = [NSEvent keyEventWithType:type
                                    location:NSZeroPoint
                               modifierFlags:flags
                                   timestamp:TimeIntervalSinceSystemStartup()
                                windowNumber:[window windowNumber]
                                     context:nil
                                  characters:characters
                 charactersIgnoringModifiers:charactersIgnoringModifiers
                                   isARepeat:NO
                                     keyCode:(unsigned short)macKeycode];

  return event;
}

}  // namespace cocoa_test_event_utils
