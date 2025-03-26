// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/test/ui_controls.h"

#import <Cocoa/Cocoa.h>

#include <vector>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/current_thread.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/geometry/point.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Implementation details: We use [NSApplication sendEvent:] instead
// of [NSApplication postEvent:atStart:] so that the event gets sent
// immediately.  This lets us run the post-event task right
// immediately as well.  Unfortunately I cannot subclass NSEvent (it's
// probably a class cluster) to allow other easy answers.  For
// example, if I could subclass NSEvent, I could run the Task in it's
// dealloc routine (which necessarily happens after the event is
// dispatched).  Unlike Linux, Mac does not have message loop
// observer/notification.  Unlike windows, I cannot post non-events
// into the event queue.  (I can post other kinds of tasks but can't
// guarantee their order with regards to events).

// But [NSApplication sendEvent:] causes a problem when sending mouse click
// events. Because in order to handle mouse drag, when processing a mouse
// click event, the application may want to retrieve the next event
// synchronously by calling NSApplication's nextEventMatchingMask method.
// In this case, [NSApplication sendEvent:] causes deadlock.
// So we need to use [NSApplication postEvent:atStart:] for mouse click
// events. In order to notify the caller correctly after all events has been
// processed, we setup a task to watch for the event queue time to time and
// notify the caller as soon as there is no event in the queue.
//
// TODO(suzhe):
// 1. Investigate why using [NSApplication postEvent:atStart:] for keyboard
//    events causes BrowserKeyEventsTest.CommandKeyEvents to fail.
//    See http://crbug.com/49270
// 2. On OSX 10.6, [NSEvent addLocalMonitorForEventsMatchingMask:handler:] may
//    be used, so that we don't need to poll the event queue time to time.

using cocoa_test_event_utils::SynthesizeKeyEvent;
using cocoa_test_event_utils::TimeIntervalSinceSystemStartup;

namespace {

// Stores the current mouse location on the screen. So that we can use it
// when firing keyboard and mouse click events.
NSPoint g_mouse_location = { 0, 0 };

// Stores the current pressed mouse buttons. Indexed by
// ui_controls::MouseButton.
bool g_mouse_button_down[3] = {false, false, false};

bool g_ui_controls_enabled = false;

// Creates the proper sequence of autoreleased key events for a key down + up.
void SynthesizeKeyEventsSequence(NSWindow* window,
                                 ui::KeyboardCode keycode,
                                 int key_event_types,
                                 int accelerator_state,
                                 std::vector<NSEvent*>* events) {
  NSEvent* event = nil;
  NSUInteger flags = 0;
  if (key_event_types & ui_controls::kKeyPress) {
    if (accelerator_state & ui_controls::kControl) {
      flags |= NSEventModifierFlagControl;
      event = SynthesizeKeyEvent(window, true, ui::VKEY_CONTROL, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kShift) {
      flags |= NSEventModifierFlagShift;
      event = SynthesizeKeyEvent(window, true, ui::VKEY_SHIFT, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kAlt) {
      flags |= NSEventModifierFlagOption;
      event = SynthesizeKeyEvent(window, true, ui::VKEY_MENU, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kCommand) {
      flags |= NSEventModifierFlagCommand;
      event = SynthesizeKeyEvent(window, true, ui::VKEY_COMMAND, flags);
      DCHECK(event);
      events->push_back(event);
    }

    event = SynthesizeKeyEvent(window, true, keycode, flags);
    DCHECK(event);
    events->push_back(event);
  }

  if (key_event_types & ui_controls::kKeyRelease) {
    event = SynthesizeKeyEvent(window, false, keycode, flags);
    DCHECK(event);
    events->push_back(event);

    if (accelerator_state & ui_controls::kCommand) {
      flags &= ~NSEventModifierFlagCommand;
      event = SynthesizeKeyEvent(window, false, ui::VKEY_COMMAND, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kAlt) {
      flags &= ~NSEventModifierFlagOption;
      event = SynthesizeKeyEvent(window, false, ui::VKEY_MENU, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kShift) {
      flags &= ~NSEventModifierFlagShift;
      event = SynthesizeKeyEvent(window, false, ui::VKEY_SHIFT, flags);
      DCHECK(event);
      events->push_back(event);
    }
    if (accelerator_state & ui_controls::kControl) {
      flags &= ~NSEventModifierFlagControl;
      event = SynthesizeKeyEvent(window, false, ui::VKEY_CONTROL, flags);
      DCHECK(event);
      events->push_back(event);
    }
  }
}

// A helper function to watch for the event queue. The specific task will be
// fired when there is no more event in the queue.
void EventQueueWatcher(base::OnceClosure task) {
  NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                      untilDate:nil
                                         inMode:NSDefaultRunLoopMode
                                        dequeue:NO];
  // If there is still event in the queue, then we need to check again.
  if (event) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EventQueueWatcher, std::move(task)));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(task));
  }
}

// Returns the NSWindow located at |g_mouse_location|. NULL if there is no
// window there, or if the window located there is not owned by the application.
// On Mac, unless dragging, mouse events are sent to the window under the
// cursor. Note that the OS will ignore transparent windows and windows that
// explicitly ignore mouse events.
NSWindow* WindowAtCurrentMouseLocation() {
  NSInteger window_number = [NSWindow windowNumberAtPoint:g_mouse_location
                              belowWindowWithWindowNumber:0];
  NSWindow* window =
      [[NSApplication sharedApplication] windowWithWindowNumber:window_number];
  if (window)
    return window;

  // It's possible for a window owned by another application to be at that
  // location. Cocoa won't provide an NSWindow* for those. Tests should not care
  // about other applications, and raising windows in a headless application is
  // flaky due to OS restrictions. For tests, hunt through all of this
  // application's windows, top to bottom, looking for a good candidate.
  NSArray* window_list = [[NSApplication sharedApplication] orderedWindows];
  for (window in window_list) {
    // Note this skips the extra checks (e.g. fully-transparent windows), that
    // +[NSWindow windowNumberAtPoint:] performs. Tests that care about that
    // should check separately (the goal here is to minimize flakiness).
    if (NSPointInRect(g_mouse_location, [window frame]))
      return window;
  }

  // Note that -[NSApplication orderedWindows] won't include NSPanels. If a test
  // uses those, it will need to handle that itself.
  return nil;
}

}  // namespace

// Donates testing implementations of NSEvent methods.
@interface FakeNSEventTestingDonor : NSObject
@end

@implementation FakeNSEventTestingDonor
+ (NSPoint)mouseLocation {
  return g_mouse_location;
}

+ (NSUInteger)pressedMouseButtons {
  NSUInteger result = 0;
  const int buttons[3] = {
      ui_controls::LEFT, ui_controls::RIGHT, ui_controls::MIDDLE};
  for (size_t i = 0; i < std::size(buttons); ++i) {
    if (g_mouse_button_down[buttons[i]])
      result |= (1 << i);
  }
  return result;
}
@end

namespace {

// Swizzles several Cocoa functions that are used to directly get mouse state,
// so that they will return the current simulated mouse position and pressed
// mouse buttons.
class MockNSEventClassMethods {
 public:
  static void Init() {
    static MockNSEventClassMethods* swizzler = nullptr;
    if (!swizzler) {
      swizzler = new MockNSEventClassMethods();
    }
  }

  MockNSEventClassMethods(const MockNSEventClassMethods&) = delete;
  MockNSEventClassMethods& operator=(const MockNSEventClassMethods&) = delete;

 private:
  MockNSEventClassMethods()
      : mouse_location_swizzler_([NSEvent class],
                                 [FakeNSEventTestingDonor class],
                                 @selector(mouseLocation)),
        pressed_mouse_buttons_swizzler_([NSEvent class],
                                        [FakeNSEventTestingDonor class],
                                        @selector(pressedMouseButtons)) {}

  base::apple::ScopedObjCClassSwizzler mouse_location_swizzler_;
  base::apple::ScopedObjCClassSwizzler pressed_mouse_buttons_swizzler_;
};

}  // namespace

namespace ui_controls {

void EnableUIControls() {
  g_ui_controls_enabled = true;
  MockNSEventClassMethods::Init();
}

bool IsUIControlsEnabled() {
  return g_ui_controls_enabled;
}

void ResetUIControlsIfEnabled() {}

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CHECK(g_ui_controls_enabled);
  return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                    base::OnceClosure());
}

// The implementation in ui_controls_aura.cc sends key press *and* release, so
// this implementation does the same.
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                base::OnceClosure task,
                                KeyEventType wait_for) {
  // This doesn't time out if `window` is deleted before the key release events
  // are dispatched, so it's fine to ignore `wait_for` and always wait for key
  // release events.
  CHECK(g_ui_controls_enabled);
  return SendKeyEventsNotifyWhenDone(
      window, key, kKeyPress | kKeyRelease, std::move(task),
      GenerateAcceleratorState(control, shift, alt, command));
}

bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state) {
  CHECK(g_ui_controls_enabled);
  return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                     base::OnceClosure(), accelerator_state);
}

bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure task,
                                 int accelerator_state) {
  CHECK(g_ui_controls_enabled);
  DCHECK(base::CurrentUIThread::IsSet());

  std::vector<NSEvent*> events;
  SynthesizeKeyEventsSequence(window.GetNativeNSWindow(), key, key_event_types,
                              accelerator_state, &events);

  // TODO(suzhe): Using [NSApplication postEvent:atStart:] here causes
  // BrowserKeyEventsTest.CommandKeyEvents to fail. See http://crbug.com/49270
  // But using [NSApplication sendEvent:] should be safe for keyboard events,
  // because until now, no code wants to retrieve the next event when handling
  // a keyboard event.
  for (std::vector<NSEvent*>::iterator iter = events.begin();
       iter != events.end(); ++iter)
    [[NSApplication sharedApplication] sendEvent:*iter];

  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EventQueueWatcher, std::move(task)));
  }

  return true;
}

bool SendMouseMove(int x, int y, gfx::NativeWindow window_hint) {
  CHECK(g_ui_controls_enabled);
  return SendMouseMoveNotifyWhenDone(x, y, base::OnceClosure(), window_hint);
}

// Input position is in screen coordinates.  However, NSEventTypeMouseMoved
// events require them window-relative, so we adjust.  We *DO* flip
// the coordinate space, so input events can be the same for all
// platforms.  E.g. (0,0) is upper-left.
bool SendMouseMoveNotifyWhenDone(int x,
                                 int y,
                                 base::OnceClosure task,
                                 gfx::NativeWindow window_hint) {
  CHECK(g_ui_controls_enabled);
  g_mouse_location = gfx::ScreenPointToNSPoint(gfx::Point(x, y));  // flip!

  NSWindow* window = window_hint ? window_hint.GetNativeNSWindow()
                                 : WindowAtCurrentMouseLocation();

  NSPoint pointInWindow = g_mouse_location;
  if (window) {
    pointInWindow = [window convertPointFromScreen:pointInWindow];
  }
  NSTimeInterval timestamp = TimeIntervalSinceSystemStartup();

  NSEventType event_type = NSEventTypeMouseMoved;
  if (g_mouse_button_down[LEFT]) {
    event_type = NSEventTypeLeftMouseDragged;
  } else if (g_mouse_button_down[RIGHT]) {
    event_type = NSEventTypeRightMouseDragged;
  } else if (g_mouse_button_down[MIDDLE]) {
    event_type = NSEventTypeOtherMouseDragged;
  }

  NSEvent* event = [NSEvent
      mouseEventWithType:event_type
                location:pointInWindow
           modifierFlags:0
               timestamp:timestamp
            windowNumber:[window windowNumber]
                 context:nil
             eventNumber:0
              clickCount:event_type == NSEventTypeMouseMoved ? 0 : 1
                pressure:event_type == NSEventTypeMouseMoved ? 0.0 : 1.0];
  [[NSApplication sharedApplication] postEvent:event atStart:NO];

  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EventQueueWatcher, std::move(task)));
  }

  return true;
}

bool SendMouseEvents(MouseButton type,
                     int button_state,
                     int accelerator_state,
                     gfx::NativeWindow window_hint) {
  CHECK(g_ui_controls_enabled);
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state, window_hint);
}

bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int button_state,
                                   base::OnceClosure task,
                                   int accelerator_state,
                                   gfx::NativeWindow window_hint) {
  CHECK(g_ui_controls_enabled);
  // Handle the special case of mouse clicking (UP | DOWN) case.
  if (button_state == (UP | DOWN)) {
    return (SendMouseEventsNotifyWhenDone(type, DOWN, base::OnceClosure(),
                                          accelerator_state, window_hint) &&
            SendMouseEventsNotifyWhenDone(type, UP, std::move(task),
                                          accelerator_state, window_hint));
  }
  NSEventType event_type = NSEventTypeLeftMouseDown;
  if (type == LEFT) {
    if (button_state == UP) {
      event_type = NSEventTypeLeftMouseUp;
    } else {
      event_type = NSEventTypeLeftMouseDown;
    }
  } else if (type == MIDDLE) {
    if (button_state == UP) {
      event_type = NSEventTypeOtherMouseUp;
    } else {
      event_type = NSEventTypeOtherMouseDown;
    }
  } else {
    CHECK_EQ(type, RIGHT);
    if (button_state == UP) {
      event_type = NSEventTypeRightMouseUp;
    } else {
      event_type = NSEventTypeRightMouseDown;
    }
  }
  g_mouse_button_down[type] = button_state == DOWN;

  NSWindow* window = window_hint ? window_hint.GetNativeNSWindow()
                                 : WindowAtCurrentMouseLocation();

  NSPoint pointInWindow = g_mouse_location;
  if (window) {
    pointInWindow = [window convertPointFromScreen:pointInWindow];
  }

  // Process the accelerator key state.
  NSEventModifierFlags modifier = 0;
  if (accelerator_state & kShift)
    modifier |= NSEventModifierFlagShift;
  if (accelerator_state & kControl)
    modifier |= NSEventModifierFlagControl;
  if (accelerator_state & kAlt)
    modifier |= NSEventModifierFlagOption;
  if (accelerator_state & kCommand)
    modifier |= NSEventModifierFlagCommand;

  NSEvent* event =
      [NSEvent mouseEventWithType:event_type
                         location:pointInWindow
                    modifierFlags:modifier
                        timestamp:TimeIntervalSinceSystemStartup()
                     windowNumber:[window windowNumber]
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:button_state == DOWN ? 1.0 : 0.0];
  [[NSApplication sharedApplication] postEvent:event atStart:NO];

  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EventQueueWatcher, std::move(task)));
  }

  return true;
}

bool SendMouseClick(MouseButton type, gfx::NativeWindow window_hint) {
  CHECK(g_ui_controls_enabled);
  return SendMouseEventsNotifyWhenDone(type, UP | DOWN, base::OnceClosure(),
                                       kNoAccelerator, window_hint);
}

bool IsFullKeyboardAccessEnabled() {
  return [NSApp isFullKeyboardAccessEnabled];
}

}  // namespace ui_controls
