// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <Cocoa/Cocoa.h>
#include <stdint.h>

#include "base/logging.h"
#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  NSEventType type = [native_event type];
  switch (type) {
    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      return IsKeyUpEvent(native_event) ? ET_KEY_RELEASED : ET_KEY_PRESSED;
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      return ET_MOUSE_PRESSED;
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      return ET_MOUSE_RELEASED;
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      return ET_MOUSE_DRAGGED;
    case NSMouseMoved:
      return ET_MOUSE_MOVED;
    case NSScrollWheel:
      return ET_SCROLL;
    case NSMouseEntered:
      return ET_MOUSE_ENTERED;
    case NSMouseExited:
      return ET_MOUSE_EXITED;
    case NSEventTypeSwipe:
      return ET_SCROLL_FLING_START;
    case NSAppKitDefined:
    case NSSystemDefined:
      return ET_UNKNOWN;
    case NSApplicationDefined:
    case NSPeriodic:
    case NSCursorUpdate:
    case NSTabletPoint:
    case NSTabletProximity:
    case NSEventTypeGesture:
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
    case NSEventTypeBeginGesture:
    case NSEventTypeEndGesture:
    case NSEventTypePressure:
      break;
    default:
      NOTIMPLEMENTED() << type;
      break;
  }
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const PlatformEvent& event) {
  NSUInteger modifiers = [event modifierFlags];
  return EventFlagsFromNSEventWithModifiers(event, modifiers);
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds([native_event timestamp]);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

gfx::PointF EventLocationFromNative(const PlatformEvent& native_event) {
  NSWindow* window = [native_event window];
  NSPoint location = [native_event locationInWindow];
  // If there's no window, the event is in screen coordinates.
  NSRect frame = window ? [window contentRectForFrameRect:[window frame]]
                        : [[[NSScreen screens] firstObject] frame];
  // In Cocoa, the y coordinate anchors to the bottom, so we need to flip it.
  return gfx::PointF(location.x, NSHeight(frame) - location.y);
}

gfx::Point EventSystemLocationFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

int EventButtonFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

int GetChangedMouseButtonFlagsFromNative(const PlatformEvent& native_event) {
  NSEventType type = [native_event type];
  switch (type) {
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSLeftMouseDragged:
      return EF_LEFT_MOUSE_BUTTON;
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSRightMouseDragged:
      return EF_RIGHT_MOUSE_BUTTON;
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSOtherMouseDragged:
      return EF_MIDDLE_MOUSE_BUTTON;
    default:
      break;
  }
  return 0;
}

PointerDetails GetMousePointerDetailsFromNative(
    const PlatformEvent& native_event) {
  return PointerDetails(EventPointerType::POINTER_TYPE_MOUSE);
}

gfx::Vector2d GetMouseWheelOffset(const PlatformEvent& event) {
  if ([event hasPreciseScrollingDeltas]) {
    // Handle continuous scrolling devices such as a Magic Mouse or a trackpad.
    // -scrollingDelta{X|Y} have float return types but they return values that
    // are already rounded to integers.
    // The values are the same as the values returned from calling
    // CGEventGetIntegerValueField(kCGScrollWheelEventPointDeltaAxis{1|2}).
    return gfx::Vector2d([event scrollingDeltaX], [event scrollingDeltaY]);
  } else {
    // Empirically, a value of 0.1 is typical for one mousewheel click. Positive
    // values when scrolling up or to the left. Scrolling quickly results in a
    // higher delta per click, up to about 15.0. (Quartz documentation suggests
    // +/-10).
    // Use the same multiplier as content::WebMouseWheelEventBuilder. Note this
    // differs from the value returned by CGEventSourceGetPixelsPerLine(), which
    // is typically 10.
    return gfx::Vector2d(kScrollbarPixelsPerCocoaTick * [event deltaX],
                         kScrollbarPixelsPerCocoaTick * [event deltaY]);
  }
}

PlatformEvent CopyNativeEvent(const PlatformEvent& event) {
  return [event copy];
}

void ReleaseCopiedNativeEvent(const PlatformEvent& event) {
  [event release];
}

void ClearTouchIdIfReleased(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
}

int GetTouchId(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::POINTER_TYPE_UNKNOWN,
                        /* pointer_id*/ 0,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f);
}

bool GetScrollOffsets(const PlatformEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  gfx::Vector2d offset = GetMouseWheelOffset(native_event);
  *x_offset = *x_offset_ordinal = offset.x();
  *y_offset = *y_offset_ordinal = offset.y();

  // For non-scrolling events, the finger count can be determined with
  // [[native_event touchesMatchingPhase:NSTouchPhaseTouching inView:nil] count]
  // but it's illegal to ask that of scroll events, so say two fingers.
  *finger_count = 2;

  // If a user just rests two fingers on the touchpad without moving, AppKit
  // uses NSEventPhaseMayBegin. Treat this the same as NSEventPhaseBegan.
  const NSUInteger kBeginPhaseMask = NSEventPhaseBegan | NSEventPhaseMayBegin;
  const NSUInteger kEndPhaseMask = NSEventPhaseCancelled | NSEventPhaseEnded;

  // Note: although the NSEventPhase constants are bit flags, the logic here
  // assumes AppKit will not combine them, so momentum phase should only be set
  // once. If one of these DCHECKs fails it could mean some new hardware that
  // needs tests in events_mac_unittest.mm.
  DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);

  if ([native_event phase] & kBeginPhaseMask)
    *momentum_phase = EventMomentumPhase::MAY_BEGIN;

  if (([native_event phase] | [native_event momentumPhase]) & kEndPhaseMask) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::END;
  } else if ([native_event momentumPhase] != NSEventPhaseNone) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::INERTIAL_UPDATE;
  }

  // If the event completely lacks phase information, there won't be further
  // updates, so they must be treated as an end.
  if (([native_event phase] | [native_event momentumPhase]) ==
      NSEventPhaseNone) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::END;
  }

  return true;
}

bool GetFlingData(const PlatformEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

KeyboardCode KeyboardCodeFromNative(const PlatformEvent& native_event) {
  return KeyboardCodeFromNSEvent(native_event);
}

DomCode CodeFromNative(const PlatformEvent& native_event) {
  return DomCodeFromNSEvent(native_event);
}

uint32_t WindowsKeycodeFromNative(const PlatformEvent& native_event) {
  return static_cast<uint32_t>(KeyboardCodeFromNSEvent(native_event));
}

uint16_t TextFromNative(const PlatformEvent& native_event) {
  NSString* text = @"";
  if ([native_event type] != NSFlagsChanged)
    text = [native_event characters];

  // These exceptions are based on web_input_event_builders_mac.mm:
  uint32_t windows_keycode = WindowsKeycodeFromNative(native_event);
  if (windows_keycode == '\r')
    text = @"\r";
  if ([text isEqualToString:@"\x7F"])
    text = @"\x8";
  if (windows_keycode == 9)
    text = @"\x9";

  uint16_t return_value;
  [text getCharacters:&return_value];
  return return_value;
}

uint16_t UnmodifiedTextFromNative(const PlatformEvent& native_event) {
  NSString* text = @"";
  if ([native_event type] != NSFlagsChanged)
    text = [native_event charactersIgnoringModifiers];

  // These exceptions are based on web_input_event_builders_mac.mm:
  uint32_t windows_keycode = WindowsKeycodeFromNative(native_event);
  if (windows_keycode == '\r')
    text = @"\r";
  if ([text isEqualToString:@"\x7F"])
    text = @"\x8";
  if (windows_keycode == 9)
    text = @"\x9";

  uint16_t return_value;
  [text getCharacters:&return_value];
  return return_value;
}

bool IsCharFromNative(const PlatformEvent& native_event) {
  return false;
}

}  // namespace ui
