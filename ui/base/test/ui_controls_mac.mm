// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls.h"

#import <Cocoa/Cocoa.h>

#include <array>
#include <vector>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
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

// Stores the most recently-entered window so that exit and enter events can be
// sent correctly.
__weak NSWindow* g_last_window_weak = nullptr;

// Stores the current pressed mouse buttons. Indexed by
// ui_controls::MouseButton.
std::array<bool, 3> g_mouse_button_down = {false, false, false};

bool g_ui_controls_enabled = false;

void CheckUIControlsEnabled() {
  CHECK(g_ui_controls_enabled)
      << "In order to use ui_controls methods, you must be in a test "
         "executable that enables UI Controls. Currently, this is "
         "interactive_ui_tests and some fuzzing tests.\n"
         "This limitation prevents attempting to send input that might require "
         "the test process to be active and focused in an environment where "
         "the process is not guaranteed to be running exclusively, which can "
         "lead to flaky tests.";
}

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
void PostWhenEventQueueIsEmpty(base::OnceClosure task) {
  NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                      untilDate:nil
                                         inMode:NSDefaultRunLoopMode
                                        dequeue:NO];
  // If there is still event in the queue, then we need to check again.
  if (event) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PostWhenEventQueueIsEmpty, std::move(task)));
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

// Makes a mouse event for `event_type`.
NSEvent* MakeMouseEvent(NSEventType event_type, NSWindow* window) {
  NSTimeInterval timestamp = TimeIntervalSinceSystemStartup();
  NSPoint point_in_window = g_mouse_location;
  if (window) {
    point_in_window = [window convertPointFromScreen:point_in_window];
  }
  const bool is_move = event_type == NSEventTypeMouseMoved;
  const bool is_enter_exit = event_type == NSEventTypeMouseEntered ||
                             event_type == NSEventTypeMouseExited;
  if (is_enter_exit) {
    return [NSEvent enterExitEventWithType:event_type
                                  location:point_in_window
                             modifierFlags:0
                                 timestamp:timestamp
                              windowNumber:[window windowNumber]
                                   context:nil
                               eventNumber:0
                            trackingNumber:0
                                  userData:nullptr];
  }
  return [NSEvent mouseEventWithType:event_type
                            location:point_in_window
                       modifierFlags:0
                           timestamp:timestamp
                        windowNumber:[window windowNumber]
                             context:nil
                         eventNumber:0
                          clickCount:is_move ? 0 : 1
                            pressure:is_move ? 0.0 : 1.0];
}

}  // namespace

// Mock implementation of NSDraggingInfo for use in the fake drag loop.
// Only the four writable properties below are read by Chromium's drag-and-drop
// clients; the remaining members are required by the NSDraggingInfo protocol.
@interface StubDraggingInfo : NSObject <NSDraggingInfo>
@property(assign, nonatomic) NSPoint draggingLocation;
@property(assign, nonatomic) NSDragOperation draggingSourceOperationMask;
@property(weak, nonatomic) NSWindow* draggingDestinationWindow;
@property(strong, nonatomic) NSPasteboard* draggingPasteboard;
@end

@implementation StubDraggingInfo
@synthesize draggingLocation = _draggingLocation;
@synthesize draggingSourceOperationMask = _draggingSourceOperationMask;
@synthesize draggingDestinationWindow = _draggingDestinationWindow;
@synthesize draggingPasteboard = _draggingPasteboard;
@synthesize draggedImageLocation = _draggedImageLocation;
@synthesize draggedImage = _draggedImage;
@synthesize draggingSource = _draggingSource;
@synthesize draggingSequenceNumber = _draggingSequenceNumber;
@synthesize animatesToDestination = _animatesToDestination;
@synthesize numberOfValidItemsForDrop = _numberOfValidItemsForDrop;
@synthesize draggingFormation = _draggingFormation;
@synthesize springLoadingHighlight = _springLoadingHighlight;

- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDestination {
  return nil;
}

- (void)slideDraggedImageTo:(NSPoint)screenPoint {
}

- (void)enumerateDraggingItemsWithOptions:
            (NSDraggingItemEnumerationOptions)enumOpts
                                  forView:(NSView*)view
                                  classes:(NSArray*)classArray
                            searchOptions:(NSDictionary*)searchOptions
                               usingBlock:(void (^)(NSDraggingItem*,
                                                    NSInteger,
                                                    BOOL*))block {
}

- (void)resetSpringLoading {
}

@end

@interface NSView (UIControlsFakeDragDonor)
- (NSDraggingSession*)
    cr_beginDraggingSessionWithItems:(NSArray<NSDraggingItem*>*)items
                               event:(NSEvent*)event
                              source:(id<NSDraggingSource>)source;
@end

namespace {

struct FakeDragSessionState {
  id<NSDraggingSource> source = nil;
  NSDraggingSession* session = nil;
  StubDraggingInfo* dragging_info = nil;
  __weak NSView* last_target_view = nil;
  NSDragOperation last_drag_operation = NSDragOperationNone;
};

FakeDragSessionState& GetFakeDragSessionState() {
  static base::NoDestructor<FakeDragSessionState> state;
  return *state;
}

bool IsDragSessionActive() {
  return GetFakeDragSessionState().session != nil;
}

// Populates a unique pasteboard with the writers from `items`.
NSPasteboard* CreateDragPasteboard(NSArray<NSDraggingItem*>* items) {
  NSPasteboard* pasteboard = [NSPasteboard pasteboardWithUniqueName];
  NSMutableArray<id<NSPasteboardWriting>>* writers = [NSMutableArray array];
  for (NSDraggingItem* item in items) {
    if (item.item) {
      [writers addObject:item.item];
    }
  }
  if (writers.count > 0) {
    [pasteboard writeObjects:writers];
  }
  return pasteboard;
}

// Finds the nearest ancestor view with registered dragged types.
NSView* FindDraggingDestinationView(NSWindow* window, NSPoint point_in_window) {
  if (!window) {
    return nil;
  }
  NSView* view = [window.contentView hitTest:point_in_window];
  // BridgedContentView returns nil from -hitTest: in HTCAPTION
  // (kDraggableBackground) regions so AppKit can initiate a window drag, but
  // those regions can still host drop targets in Views.
  if (!view && window.contentView &&
      NSMouseInRect(point_in_window, window.contentView.frame,
                    window.contentView.isFlipped)) {
    view = window.contentView;
  }
  while (view && view.registeredDraggedTypes.count == 0) {
    view = [view superview];
  }
  return view;
}

// Clears the active drag session and notifies the source that dragging ended.
void EndDragSession(NSDragOperation operation) {
  FakeDragSessionState& state = GetFakeDragSessionState();
  id<NSDraggingSource> source = state.source;
  NSDraggingSession* session = state.session;
  StubDraggingInfo* dragging_info = state.dragging_info;
  NSView* last_target_view = state.last_target_view;
  state = FakeDragSessionState();

  if (operation == NSDragOperationNone &&
      [last_target_view respondsToSelector:@selector(draggingExited:)]) {
    [last_target_view draggingExited:dragging_info];
  }
  if (source && session &&
      [source
          respondsToSelector:@selector(
                                 draggingSession:endedAtPoint:operation:)]) {
    [source draggingSession:session
               endedAtPoint:[NSEvent mouseLocation]
                  operation:operation];
  }
  [dragging_info.draggingPasteboard releaseGlobally];
}

// Performs the drop (or exits if rejected) and ends the drag session.
void CompleteDragDrop(NSView* target_view, StubDraggingInfo* dragging_info) {
  FakeDragSessionState& state = GetFakeDragSessionState();
  NSDragOperation operation = NSDragOperationNone;
  if (target_view && state.last_drag_operation != NSDragOperationNone &&
      [target_view respondsToSelector:@selector(performDragOperation:)] &&
      [target_view performDragOperation:dragging_info]) {
    operation = state.last_drag_operation;
    state.last_target_view = nil;
  }
  EndDragSession(operation);
}

// Sends enter/exit/update notifications to the destination view as the mouse
// moves.
void UpdateDragDestination(NSView* target_view,
                           StubDraggingInfo* dragging_info) {
  FakeDragSessionState& state = GetFakeDragSessionState();
  if (state.last_target_view != target_view) {
    state.last_target_view = target_view;
    state.last_drag_operation =
        [target_view respondsToSelector:@selector(draggingEntered:)]
            ? [target_view draggingEntered:dragging_info]
            : NSDragOperationNone;
  } else if ([target_view respondsToSelector:@selector(draggingUpdated:)]) {
    state.last_drag_operation = [target_view draggingUpdated:dragging_info];
  }
}

// Advances the active fake drag session to the current mouse location, and
// drops if `is_mouse_up`. Called directly by the synthetic event senders:
// AppKit does not deliver mouse events to the application while an
// NSDraggingSession is active, so these events are never posted to the event
// queue (where e.g. an unrelated CocoaMouseCapture monitor could consume them).
void DispatchDragSessionEvent(bool is_mouse_up) {
  FakeDragSessionState& state = GetFakeDragSessionState();
  StubDraggingInfo* dragging_info = state.dragging_info;
  NSWindow* window = WindowAtCurrentMouseLocation();
  NSPoint point_in_window =
      window ? [window convertPointFromScreen:g_mouse_location]
             : g_mouse_location;
  NSView* target_view = FindDraggingDestinationView(window, point_in_window);

  if (state.last_target_view != target_view) {
    if ([state.last_target_view
            respondsToSelector:@selector(draggingExited:)]) {
      [state.last_target_view draggingExited:dragging_info];
    }
    state.last_target_view = nil;
    state.last_drag_operation = NSDragOperationNone;
  }

  dragging_info.draggingLocation = point_in_window;
  dragging_info.draggingDestinationWindow = window;

  UpdateDragDestination(target_view, dragging_info);
  if (is_mouse_up) {
    CompleteDragDrop(target_view, dragging_info);
  }
}

}  // namespace

@implementation NSView (UIControlsFakeDragDonor)
- (NSDraggingSession*)
    cr_beginDraggingSessionWithItems:(NSArray<NSDraggingItem*>*)items
                               event:(NSEvent*)event
                              source:(id<NSDraggingSource>)source {
  EndDragSession(NSDragOperationNone);

  // Set up a fake session and synthetic dragging info for this drag.
  FakeDragSessionState& state = GetFakeDragSessionState();
  state.source = source;
  state.session = (NSDraggingSession*)[[NSObject alloc] init];
  StubDraggingInfo* draggingInfo = [[StubDraggingInfo alloc] init];
  draggingInfo.draggingSourceOperationMask =
      [source draggingSession:state.session
          sourceOperationMaskForDraggingContext:
              NSDraggingContextOutsideApplication];
  draggingInfo.draggingPasteboard = CreateDragPasteboard(items);
  state.dragging_info = draggingInfo;

  if ([source
          respondsToSelector:@selector(draggingSession:willBeginAtPoint:)]) {
    [source draggingSession:state.session
           willBeginAtPoint:[NSEvent mouseLocation]];
  }

  // Subsequent synthetic mouse events drive the session via
  // DispatchDragSessionEvent().
  return state.session;
}
@end

// Donates testing implementations of NSEvent methods.
@interface FakeNSEventTestingDonor : NSObject
@end

@implementation FakeNSEventTestingDonor
+ (NSPoint)mouseLocation {
  return g_mouse_location;
}

+ (NSUInteger)pressedMouseButtons {
  NSUInteger result = 0;
  const std::array buttons = {
      ui_controls::LEFT,
      ui_controls::RIGHT,
      ui_controls::MIDDLE,
  };
  int i = 0;
  for (int button : buttons) {
    if (g_mouse_button_down[button]) {
      result |= (1 << i);
    }
    i++;
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
                                        @selector(pressedMouseButtons)),
        drag_swizzler_(
            [NSView class],
            @selector(beginDraggingSessionWithItems:event:source:),
            @selector(cr_beginDraggingSessionWithItems:event:source:)) {}

  base::apple::ScopedObjCClassSwizzler mouse_location_swizzler_;
  base::apple::ScopedObjCClassSwizzler pressed_mouse_buttons_swizzler_;
  base::apple::ScopedObjCClassSwizzler drag_swizzler_;
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

void ResetUIControlsIfEnabled() {
  if (!g_ui_controls_enabled) {
    return;
  }
  EndDragSession(NSDragOperationNone);
  g_mouse_button_down = {false, false, false};
}

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CheckUIControlsEnabled();
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
  CheckUIControlsEnabled();
  return SendKeyEventsNotifyWhenDone(
      window, key, kKeyPress | kKeyRelease, std::move(task),
      GenerateAcceleratorState(control, shift, alt, command));
}

bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state) {
  CheckUIControlsEnabled();
  return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                     base::OnceClosure(), accelerator_state);
}

bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure task,
                                 int accelerator_state) {
  CheckUIControlsEnabled();
  DCHECK(base::CurrentUIThread::IsSet());

  std::vector<NSEvent*> events;
  SynthesizeKeyEventsSequence(window.GetNativeNSWindow(), key, key_event_types,
                              accelerator_state, &events);

  // TODO(suzhe): Using [NSApplication postEvent:atStart:] here causes
  // BrowserKeyEventsTest.CommandKeyEvents to fail. See http://crbug.com/49270
  // But using [NSApplication sendEvent:] should be safe for keyboard events,
  // because until now, no code wants to retrieve the next event when handling
  // a keyboard event.
  for (NSEvent* event : events) {
    [[NSApplication sharedApplication] sendEvent:event];
  }

  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PostWhenEventQueueIsEmpty, std::move(task)));
  }

  return true;
}

bool SendMouseMove(int x, int y, gfx::NativeWindow window_hint) {
  CheckUIControlsEnabled();
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
  CheckUIControlsEnabled();

  g_mouse_location = gfx::ScreenPointToNSPoint(gfx::Point(x, y));  // flip!
  NSWindow* window = window_hint ? window_hint.GetNativeNSWindow()
                                 : WindowAtCurrentMouseLocation();

  // Possibly send exit and enter events.
  if (g_last_window_weak != window) {
    if (g_last_window_weak) {
      NSEvent* event =
          MakeMouseEvent(NSEventTypeMouseExited, g_last_window_weak);
      [g_last_window_weak.contentView mouseExited:event];
    }
    if (window) {
      NSEvent* event = MakeMouseEvent(NSEventTypeMouseEntered, window);
      [g_last_window_weak.contentView mouseEntered:event];
    }
  }
  g_last_window_weak = window;

  NSEventType event_type = NSEventTypeMouseMoved;
  if (g_mouse_button_down[LEFT]) {
    event_type = NSEventTypeLeftMouseDragged;
  } else if (g_mouse_button_down[RIGHT]) {
    event_type = NSEventTypeRightMouseDragged;
  } else if (g_mouse_button_down[MIDDLE]) {
    event_type = NSEventTypeOtherMouseDragged;
  }

  // In production, mouse entered/exited/move events are sent to the owner of
  // the NSTrackingArea that they are in. Unlike other mouse events, they bypass
  // the NSApplication event loop.
  //
  // This test utility simulates that by sending the mouse move
  // to the target NSView directly.
  // TODO(crbug.com/503006742): mouse enter and exit events are not generated
  // for subviews. Fix it.
  if (IsDragSessionActive()) {
    // A move with the left button already released means the drop was missed;
    // complete it now rather than leaving the session dangling.
    DispatchDragSessionEvent(/*is_mouse_up=*/!g_mouse_button_down[LEFT]);
  } else if (window_hint && event_type == NSEventTypeMouseMoved) {
    if (window) {
      NSEvent* event = MakeMouseEvent(event_type, window);
      NSPoint point_in_window = [event locationInWindow];
      // `target_view` might be the contentView or a subview (e.g. a
      // WebView's native view). hitTest: will find that target.
      NSView* target_view = [window.contentView hitTest:point_in_window];
      if (target_view) {
        [target_view mouseMoved:event];
      } else {
        [window.contentView mouseMoved:event];
      }
    }
  } else {
    [[NSApplication sharedApplication]
        postEvent:MakeMouseEvent(event_type, window)
          atStart:NO];
  }

  // Maybe post the follow-up task.
  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PostWhenEventQueueIsEmpty, std::move(task)));
  }

  return true;
}

bool SendMouseEvents(MouseButton type,
                     int button_state,
                     int accelerator_state,
                     gfx::NativeWindow window_hint) {
  CheckUIControlsEnabled();
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state, window_hint);
}

bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int button_state,
                                   base::OnceClosure task,
                                   int accelerator_state,
                                   gfx::NativeWindow window_hint) {
  CheckUIControlsEnabled();
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
  if (IsDragSessionActive() && type == LEFT) {
    // The fake drag session consumes left-button events (see
    // DispatchDragSessionEvent()); a release completes the drop.
    DispatchDragSessionEvent(/*is_mouse_up=*/button_state == UP);
  } else {
    [[NSApplication sharedApplication] postEvent:event atStart:NO];
  }

  if (!task.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PostWhenEventQueueIsEmpty, std::move(task)));
  }

  return true;
}

bool SendMouseClick(MouseButton type, gfx::NativeWindow window_hint) {
  CheckUIControlsEnabled();
  return SendMouseEventsNotifyWhenDone(type, UP | DOWN, base::OnceClosure(),
                                       kNoAccelerator, window_hint);
}

bool IsFullKeyboardAccessEnabled() {
  return [NSApp isFullKeyboardAccessEnabled];
}

}  // namespace ui_controls
