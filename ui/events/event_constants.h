// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_CONSTANTS_H_
#define UI_EVENTS_EVENT_CONSTANTS_H_

#include "build/build_config.h"

namespace ui {

// Event types. (prefixed because of a conflict with windows headers)
enum EventType {
  ET_UNKNOWN = 0,
  ET_MOUSE_PRESSED,
  ET_MOUSE_DRAGGED,
  ET_MOUSE_RELEASED,
  ET_MOUSE_MOVED,
  ET_MOUSE_ENTERED,
  ET_MOUSE_EXITED,
  ET_KEY_PRESSED,
  ET_KEY_RELEASED,
  ET_MOUSEWHEEL,
  ET_MOUSE_CAPTURE_CHANGED,  // Event has no location.
  ET_TOUCH_RELEASED,
  ET_TOUCH_PRESSED,
  // NOTE: This corresponds to a drag and is always preceeded by an
  // ET_TOUCH_PRESSED. GestureRecognizers generally ignore ET_TOUCH_MOVED events
  // without a corresponding ET_TOUCH_PRESSED.
  ET_TOUCH_MOVED,
  ET_TOUCH_CANCELLED,
  ET_DROP_TARGET_EVENT,

  // GestureEvent types
  ET_GESTURE_SCROLL_BEGIN,
  ET_GESTURE_TYPE_START = ET_GESTURE_SCROLL_BEGIN,
  ET_GESTURE_SCROLL_END,
  ET_GESTURE_SCROLL_UPDATE,
  ET_GESTURE_TAP,
  ET_GESTURE_TAP_DOWN,
  ET_GESTURE_TAP_CANCEL,
  ET_GESTURE_TAP_UNCONFIRMED,  // User tapped, but the tap delay hasn't expired.
  ET_GESTURE_DOUBLE_TAP,
  ET_GESTURE_BEGIN,  // The first event sent when each finger is pressed.
  ET_GESTURE_END,    // Sent for each released finger.
  ET_GESTURE_TWO_FINGER_TAP,
  ET_GESTURE_PINCH_BEGIN,
  ET_GESTURE_PINCH_END,
  ET_GESTURE_PINCH_UPDATE,
  ET_GESTURE_LONG_PRESS,
  ET_GESTURE_LONG_TAP,
  // A SWIPE gesture can happen at the end of a touch sequence involving one or
  // more fingers if the finger velocity was high enough when the first finger
  // was released.
  ET_GESTURE_SWIPE,
  ET_GESTURE_SHOW_PRESS,

  // Scroll support.
  // TODO[davemoore] we need to unify these events w/ touch and gestures.
  ET_SCROLL,
  ET_SCROLL_FLING_START,
  ET_SCROLL_FLING_CANCEL,
  ET_GESTURE_TYPE_END = ET_SCROLL_FLING_CANCEL,

  // Sent by the system to indicate any modal type operations, such as drag and
  // drop or menus, should stop.
  ET_CANCEL_MODE,

  // Sent by the CrOS gesture library for interesting patterns that we want
  // to track with the UMA system.
  ET_UMA_DATA,

  // Must always be last. User namespace starts above this value.
  // See ui::RegisterCustomEventType().
  ET_LAST
};

// Event flags currently supported.  It is OK to add values to the middle of
// this list and/or reorder it, but make sure you also touch the various other
// enums/constants that want to stay in sync with this.
enum EventFlags {
  EF_NONE = 0,  // Used to denote no flags explicitly

  // Universally applicable status bits.
  EF_IS_SYNTHESIZED = 1 << 0,

  // Modifier key state.
  EF_SHIFT_DOWN = 1 << 1,
  EF_CONTROL_DOWN = 1 << 2,
  EF_ALT_DOWN = 1 << 3,
  EF_COMMAND_DOWN = 1 << 4,  // GUI Key (e.g. Command on OS X
                             // keyboards, Search on Chromebook
                             // keyboards, Windows on MS-oriented
                             // keyboards)
  EF_ALTGR_DOWN = 1 << 5,
  EF_MOD3_DOWN = 1 << 6,

  // Other keyboard states.
  EF_NUM_LOCK_ON = 1 << 7,
  EF_CAPS_LOCK_ON = 1 << 8,
  EF_SCROLL_LOCK_ON = 1 << 9,

  // Mouse buttons.
  EF_LEFT_MOUSE_BUTTON = 1 << 10,
  EF_MIDDLE_MOUSE_BUTTON = 1 << 11,
  EF_RIGHT_MOUSE_BUTTON = 1 << 12,
  EF_BACK_MOUSE_BUTTON = 1 << 13,
  EF_FORWARD_MOUSE_BUTTON = 1 << 14,

// An artificial value used to bridge platform differences.
// Many commands on Mac as Cmd+Key are the counterparts of
// Ctrl+Key on other platforms.
#if defined(OS_MACOSX)
  EF_PLATFORM_ACCELERATOR = EF_COMMAND_DOWN,
#else
  EF_PLATFORM_ACCELERATOR = EF_CONTROL_DOWN,
#endif
};

// Flags specific to key events.
// WARNING: If you add or remove values make sure traits for serializing these
// values are updated.
enum KeyEventFlags {
  EF_IME_FABRICATED_KEY = 1 << 15,  // Key event fabricated by the underlying
                                    // IME without a user action.
                                    // (Linux X11 only)
  EF_IS_REPEAT = 1 << 16,
  EF_FINAL = 1 << 17,            // Do not remap; the event was created with
                                 // the desired final values.
  EF_IS_EXTENDED_KEY = 1 << 18,  // Windows extended key (see WM_KEYDOWN doc)
  EF_MAX_KEY_EVENT_FLAGS_VALUE = (1 << 19) - 1,
};

// Flags specific to mouse events.
enum MouseEventFlags {
  EF_IS_DOUBLE_CLICK = 1 << 15,
  EF_IS_TRIPLE_CLICK = 1 << 16,
  EF_IS_NON_CLIENT = 1 << 17,
  EF_FROM_TOUCH = 1 << 18,           // Indicates this mouse event is generated
                                     // from an unconsumed touch/gesture event.
  EF_TOUCH_ACCESSIBILITY = 1 << 19,  // Indicates this event was generated from
                                     // touch accessibility mode.
  EF_CURSOR_HIDE = 1 << 20,          // Indicates this mouse event is generated
                                     // because the cursor was just hidden. This
                                     // can be used to update hover state.
  EF_PRECISION_SCROLLING_DELTA =     // Indicates this mouse event is from high
  1 << 21,                           // precision touchpad and will come with a
                                     // high precision delta.
  EF_SCROLL_BY_PAGE = 1 << 22,       // Indicates this mouse event is generated
                                     // when users is requesting to scroll by
                                     // pages.
  EF_UNADJUSTED_MOUSE = 1 << 23,     // Indicates this mouse event is unadjusted
                                  // mouse events that has unadjusted movement
                                  // delta, i.e. is from WM_INPUT on Windows.
};

// Result of dispatching an event.
enum EventResult {
  ER_UNHANDLED = 0,      // The event hasn't been handled. The event can be
                         // propagated to other handlers.
  ER_HANDLED = 1 << 0,   // The event has already been handled, but it can
                         // still be propagated to other handlers.
  ER_CONSUMED = 1 << 1,  // The event has been handled, and it should not be
                         // propagated to other handlers.
  ER_DISABLE_SYNC_HANDLING =
      1 << 2,  // The event shouldn't be handled synchronously. This
               // happens if the event is being handled
               // asynchronously, or if the event is invalid and
               // shouldn't be handled at all.
};

// Phase of the event dispatch.
enum EventPhase {
  EP_PREDISPATCH,
  EP_PRETARGET,
  EP_TARGET,
  EP_POSTTARGET,
  EP_POSTDISPATCH
};

// Phase information used for a ScrollEvent. ScrollEventPhase is for scroll
// stream from user gesture, EventMomentumPhase is for inertia scroll stream
// after user gesture.
enum class ScrollEventPhase {
  // Event has no phase information. eg. the Event is not in a scroll stream.
  kNone,

  // Event is the beginning of a scroll event stream.
  kBegan,

  // Event is a scroll event with phase information.
  kUpdate,

  // Event is the end of the current scroll event stream.
  kEnd,
};

// Momentum phase information used for a ScrollEvent.
enum class EventMomentumPhase {
  // Event is a non-momentum update to an event stream already begun.
  NONE,

  // Event is the beginning of an event stream that may result in momentum.
  // BEGAN vs MAY_BEGIN:
  // - BEGAN means we already know the inertia scroll stream must happen after
  //   BEGAN event. On Windows touchpad, we sent this when receive the first
  //   inertia scroll event or Direct Manipulation state change to INERTIA.
  // - MAY_BEGIN means the inertia scroll stream may happen after MAY_BEGIN
  //   event. On Mac, we send this when receive releaseTouches, but we do not
  //   know the inertia scroll stream will happen or not at that time.
  BEGAN,

  // Event maybe the beginning of an event stream that may result in momentum.
  // This state used on Mac.
  MAY_BEGIN,

  // Event is an update while in a momentum phase. A "begin" event for the
  // momentum phase portion of an event stream uses this also, but the scroll
  // offsets will be zero.
  INERTIAL_UPDATE,

  // Event marks the end of the current event stream. Note that this is also set
  // for events that are not a "stream", but indicate both the start and end of
  // the event (e.g. a mouse wheel tick).
  END,

  // EventMomentumPhase can only be BLOCKED when ScrollEventPhase is kEnd. Event
  // marks the end of the current event stream, when there will be no inertia
  // scrolling after the user gesture. ScrollEventPhase must simultaneously be
  // kEnd because that is when it is determined if an event stream that results
  // in momentum will begin or not. This phase is only used on Windows.
  BLOCKED,
};

// Device ID for Touch and Key Events.
enum EventDeviceId {
  ED_UNKNOWN_DEVICE = -1
};

// Pointing device type.
enum class EventPointerType : int {
  POINTER_TYPE_UNKNOWN = 0,
  POINTER_TYPE_MOUSE,
  POINTER_TYPE_PEN,
  POINTER_TYPE_TOUCH,
  POINTER_TYPE_ERASER,
};

// Device type for gesture events.
enum class GestureDeviceType : int {
  DEVICE_UNKNOWN = 0,
  DEVICE_TOUCHPAD,
  DEVICE_TOUCHSCREEN,
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_CONSTANTS_H_
