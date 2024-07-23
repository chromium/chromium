// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <stdint.h>

#include "base/check_op.h"
#import "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  NSEventType type = event.type;
  switch (type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
      return IsKeyUpEvent(event) ? EventType::kKeyReleased
                                 : EventType::kKeyPressed;
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
      return EventType::kMousePressed;
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
      return EventType::kMouseReleased;
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
      return EventType::kMouseDragged;
    case NSEventTypeMouseMoved:
      return EventType::kMouseMoved;
    case NSEventTypeScrollWheel:
      return EventType::kScroll;
    case NSEventTypeMouseEntered:
      return EventType::kMouseEntered;
    case NSEventTypeMouseExited:
      return EventType::kMouseExited;
    case NSEventTypeSwipe:
      return EventType::kScrollFlingStart;
    case NSEventTypeAppKitDefined:
    case NSEventTypeSystemDefined:
      return EventType::kUnknown;
    case NSEventTypeApplicationDefined:
    case NSEventTypePeriodic:
    case NSEventTypeCursorUpdate:
    case NSEventTypeTabletPoint:
    case NSEventTypeTabletProximity:
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
  return EventType::kUnknown;
}

int EventFlagsFromNative(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  NSUInteger modifiers = [event modifierFlags];
  return EventFlagsFromNSEventWithModifiers(event, modifiers);
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& platform_event) {
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds([platform_event.Get() timestamp]);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

base::TimeTicks EventLatencyTimeFromNative(const PlatformEvent& platform_event,
                                           base::TimeTicks current_time) {
  return EventTimeFromNative(platform_event);
}

gfx::PointF EventLocationFromNative(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  NSWindow* window = event.window;
  NSPoint location = event.locationInWindow;
  // If there's no window, the event is in screen coordinates.
  NSRect frame = window ? [window contentRectForFrameRect:window.frame]
                        : NSScreen.screens.firstObject.frame;
  // In Cocoa, the y coordinate anchors to the bottom, so we need to flip it.
  return gfx::PointF(location.x, NSHeight(frame) - location.y);
}

gfx::Point EventSystemLocationFromNative(const PlatformEvent& platform_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

int EventButtonFromNative(const PlatformEvent& platform_event) {
  NOTIMPLEMENTED();
  return 0;
}

int GetChangedMouseButtonFlagsFromNative(const PlatformEvent& platform_event) {
  NSEventType type = [platform_event.Get() type];
  switch (type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
      return EF_LEFT_MOUSE_BUTTON;
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeRightMouseDragged:
      return EF_RIGHT_MOUSE_BUTTON;
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
      return EF_MIDDLE_MOUSE_BUTTON;
    default:
      break;
  }
  return 0;
}

PointerDetails GetMousePointerDetailsFromNative(
    const PlatformEvent& platform_event) {
  return PointerDetails(EventPointerType::kMouse);
}

gfx::Vector2d GetMouseWheelOffset(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  if (event.hasPreciseScrollingDeltas) {
    // Handle continuous scrolling devices such as a Magic Mouse or a trackpad.
    // -scrollingDelta{X|Y} have float return types but they return values that
    // are already rounded to integers.
    // The values are the same as the values returned from calling
    // CGEventGetIntegerValueField(kCGScrollWheelEventPointDeltaAxis{1|2}).
    return gfx::Vector2d(event.scrollingDeltaX, event.scrollingDeltaY);
  } else {
    // Empirically, a value of 0.1 is typical for one mousewheel click. Positive
    // values when scrolling up or to the left. Scrolling quickly results in a
    // higher delta per click, up to about 15.0. (Quartz documentation suggests
    // +/-10).
    // Use the same multiplier as content::WebMouseWheelEventBuilder. Note this
    // differs from the value returned by CGEventSourceGetPixelsPerLine(), which
    // is typically 10.
    return gfx::Vector2d(kScrollbarPixelsPerCocoaTick * event.deltaX,
                         kScrollbarPixelsPerCocoaTick * event.deltaY);
  }
}

gfx::Vector2d GetMouseWheelTick120ths(const PlatformEvent& event) {
  CGEventRef cg_event = [event.Get() CGEvent];

  if (!cg_event ||
      CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventIsContinuous)) {
    // Since the device does continuous scrolling, it has no concept of ticks.
    return gfx::Vector2d(0, 0);
  }

  return gfx::Vector2d(
      CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventDeltaAxis2) *
          120,
      CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventDeltaAxis1) *
          120);
}

bool ShouldCopyPlatformEvents() {
  return true;
}

PlatformEvent CreateInvalidPlatformEvent() {
  return PlatformEvent();
}

bool IsPlatformEventValid(const PlatformEvent& event) {
  return !!event;
}

void ClearTouchIdIfReleased(const PlatformEvent& platform_event) {
  NOTIMPLEMENTED();
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& platform_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::kUnknown,
                        /*pointer_id=*/0,
                        /*radius_x=*/1.0,
                        /*radius_y=*/1.0,
                        /*force=*/0.f);
}

bool GetScrollOffsets(const PlatformEvent& platform_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  NSEvent* event = platform_event.Get();
  gfx::Vector2d offset = GetMouseWheelOffset(platform_event);
  *x_offset = *x_offset_ordinal = offset.x();
  *y_offset = *y_offset_ordinal = offset.y();

  // For non-scrolling events, the finger count can be determined with
  // [[platform_event touchesMatchingPhase:NSTouchPhaseTouching inView:nil]
  // count] but it's illegal to ask that of scroll events, so say two fingers.
  *finger_count = 2;

  // If a user just rests two fingers on the touchpad without moving, AppKit
  // uses NSEventPhaseMayBegin. Treat this the same as NSEventPhaseBegan.
  // TODO(bokan): Now that ui::ScrollEvent supports the scroll phase as well as
  // the momentum phase, we should plumb these through individually.
  const NSUInteger kBeginPhaseMask = NSEventPhaseBegan | NSEventPhaseMayBegin;
  const NSUInteger kEndPhaseMask = NSEventPhaseCancelled | NSEventPhaseEnded;

  // Note: although the NSEventPhase constants are bit flags, the logic here
  // assumes AppKit will not combine them, so momentum phase should only be set
  // once. If one of these DCHECKs fails it could mean some new hardware that
  // needs tests in events_mac_unittest.mm.
  DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);

  if (event.phase & kBeginPhaseMask) {
    *momentum_phase = EventMomentumPhase::MAY_BEGIN;
  }

  if ((event.phase | event.momentumPhase) & kEndPhaseMask) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::END;
  } else if (event.momentumPhase != NSEventPhaseNone) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::INERTIAL_UPDATE;
  }

  // If the event completely lacks phase information, there won't be further
  // updates, so they must be treated as an end.
  if ((event.phase | event.momentumPhase) == NSEventPhaseNone) {
    DCHECK_EQ(EventMomentumPhase::NONE, *momentum_phase);
    *momentum_phase = EventMomentumPhase::END;
  }

  return true;
}

bool GetFlingData(const PlatformEvent& platform_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

KeyboardCode KeyboardCodeFromNative(const PlatformEvent& platform_event) {
  return KeyboardCodeFromNSEvent(platform_event.Get());
}

DomCode CodeFromNative(const PlatformEvent& platform_event) {
  return DomCodeFromNSEvent(platform_event.Get());
}

uint32_t WindowsKeycodeFromNative(const PlatformEvent& platform_event) {
  return static_cast<uint32_t>(KeyboardCodeFromNSEvent(platform_event.Get()));
}

uint16_t TextFromNative(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  NSString* text = @"";
  if (event.type != NSEventTypeFlagsChanged) {
    text = event.characters;
  }

  // These exceptions are based on web_input_event_builders_mac.mm:
  uint32_t windows_keycode = WindowsKeycodeFromNative(platform_event);
  if (windows_keycode == '\r')
    text = @"\r";
  if ([text isEqualToString:@"\x7F"])
    text = @"\x8";
  if (windows_keycode == 9)
    text = @"\x9";

  uint16_t return_value;
  [text getCharacters:&return_value range:NSMakeRange(0, 1)];
  return return_value;
}

uint16_t UnmodifiedTextFromNative(const PlatformEvent& platform_event) {
  NSEvent* event = platform_event.Get();
  NSString* text = @"";
  if (event.type != NSEventTypeFlagsChanged) {
    text = event.charactersIgnoringModifiers;
  }

  // These exceptions are based on web_input_event_builders_mac.mm:
  uint32_t windows_keycode = WindowsKeycodeFromNative(platform_event);
  if (windows_keycode == '\r')
    text = @"\r";
  if ([text isEqualToString:@"\x7F"])
    text = @"\x8";
  if (windows_keycode == 9)
    text = @"\x9";

  uint16_t return_value;
  [text getCharacters:&return_value range:NSMakeRange(0, 1)];
  return return_value;
}

bool IsCharFromNative(const PlatformEvent& platform_event) {
  return false;
}

}  // namespace ui
