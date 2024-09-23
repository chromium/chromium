// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace {

// Set (and always cleared) in EmulateSendEvent() to provide an answer for
// [NSApp currentEvent].
NSEvent* g_current_event = nil;

}  // namespace

@interface NSEventDonor : NSObject
@end

@interface NSApplicationDonor : NSObject
@end

namespace {

// Return the current owner of the EventGeneratorDelegate. May be null.
ui::test::EventGenerator* GetActiveGenerator();

NSPoint ConvertRootPointToTarget(NSWindow* target,
                                 const gfx::Point& point_in_root) {
  DCHECK(GetActiveGenerator());
  gfx::Point point = point_in_root;

  point -= gfx::ScreenRectFromNSRect(target.frame).OffsetFromOrigin();
  return NSMakePoint(point.x(), NSHeight(target.frame) - point.y());
}

// Inverse of ui::EventFlagsFromModifiers().
NSUInteger EventFlagsToModifiers(int flags) {
  NSUInteger modifiers = 0;
  modifiers |= (flags & ui::EF_SHIFT_DOWN) ? NSEventModifierFlagShift : 0;
  modifiers |= (flags & ui::EF_CONTROL_DOWN) ? NSEventModifierFlagControl : 0;
  modifiers |= (flags & ui::EF_ALT_DOWN) ? NSEventModifierFlagOption : 0;
  modifiers |= (flags & ui::EF_COMMAND_DOWN) ? NSEventModifierFlagCommand : 0;
  modifiers |= (flags & ui::EF_CAPS_LOCK_ON) ? NSEventModifierFlagCapsLock : 0;
  // ui::EF_*_MOUSE_BUTTON not handled here.
  // NSEventModifierFlagFunction, NSEventModifierFlagNumericPad and
  // NSHelpKeyMask not mapped.
  return modifiers;
}

// Picks the corresponding mouse event type for the buttons set in |flags|.
NSEventType PickMouseEventType(int flags,
                               NSEventType left,
                               NSEventType right,
                               NSEventType other) {
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    return left;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    return right;
  return other;
}

// Inverse of ui::EventTypeFromNative(). If non-null |modifiers| will be set
// using the inverse of ui::EventFlagsFromNSEventWithModifiers().
NSEventType EventTypeToNative(ui::EventType ui_event_type,
                              int flags,
                              NSUInteger* modifiers) {
  if (modifiers)
    *modifiers = EventFlagsToModifiers(flags);
  switch (ui_event_type) {
    case ui::EventType::kKeyPressed:
      return NSEventTypeKeyDown;
    case ui::EventType::kKeyReleased:
      return NSEventTypeKeyUp;
    case ui::EventType::kMousePressed:
      return PickMouseEventType(flags, NSEventTypeLeftMouseDown,
                                NSEventTypeRightMouseDown,
                                NSEventTypeOtherMouseDown);
    case ui::EventType::kMouseReleased:
      return PickMouseEventType(flags, NSEventTypeLeftMouseUp,
                                NSEventTypeRightMouseUp,
                                NSEventTypeOtherMouseUp);
    case ui::EventType::kMouseDragged:
      return PickMouseEventType(flags, NSEventTypeLeftMouseDragged,
                                NSEventTypeRightMouseDragged,
                                NSEventTypeOtherMouseDragged);
    case ui::EventType::kMouseMoved:
      return NSEventTypeMouseMoved;
    case ui::EventType::kMousewheel:
      return NSEventTypeScrollWheel;
    case ui::EventType::kMouseEntered:
      return NSEventTypeMouseEntered;
    case ui::EventType::kMouseExited:
      return NSEventTypeMouseExited;
    case ui::EventType::kScrollFlingStart:
      return NSEventTypeSwipe;
    default:
      NOTREACHED();
  }
}

// Emulate the dispatching that would be performed by -[NSWindow sendEvent:].
// sendEvent is a black box which (among other things) will try to peek at the
// event queue and can block indefinitely.
void EmulateSendEvent(NSWindow* window, NSEvent* event) {
  base::AutoReset<NSEvent*> reset(&g_current_event, event);
  NSResponder* responder = [window firstResponder];
  switch ([event type]) {
    case NSEventTypeKeyDown:
      [responder keyDown:event];
      return;
    case NSEventTypeKeyUp:
      [responder keyUp:event];
      return;
    default:
      break;
  }

  // For mouse events, NSWindow will use -[NSView hitTest:] for the initial
  // mouseDown, and then keep track of the NSView returned. The toolkit-views
  // RootView does this too. So, for tests, assume tracking will be done there,
  // and the NSWindow's contentView is wrapping a views::internal::RootView.
  responder = window.contentView;
  switch (event.type) {
    case NSEventTypeLeftMouseDown:
      [responder mouseDown:event];
      break;
    case NSEventTypeRightMouseDown:
      [responder rightMouseDown:event];
      break;
    case NSEventTypeOtherMouseDown:
      [responder otherMouseDown:event];
      break;
    case NSEventTypeLeftMouseUp:
      [responder mouseUp:event];
      break;
    case NSEventTypeRightMouseUp:
      [responder rightMouseUp:event];
      break;
    case NSEventTypeOtherMouseUp:
      [responder otherMouseUp:event];
      break;
    case NSEventTypeLeftMouseDragged:
      [responder mouseDragged:event];
      break;
    case NSEventTypeRightMouseDragged:
      [responder rightMouseDragged:event];
      break;
    case NSEventTypeOtherMouseDragged:
      [responder otherMouseDragged:event];
      break;
    case NSEventTypeMouseMoved:
      // Assumes [NSWindow acceptsMouseMovedEvents] would return YES, and that
      // NSTrackingAreas have been appropriately installed on |responder|.
      [responder mouseMoved:event];
      break;
    case NSEventTypeScrollWheel:
      [responder scrollWheel:event];
      break;
    case NSEventTypeMouseEntered:
      [responder mouseEntered:event];
      break;
    case NSEventTypeMouseExited:
      [responder mouseExited:event];
      break;
    case NSEventTypeSwipe:
      // NSEventTypeSwipe events can't be generated using public interfaces on
      // NSEvent, so this will need to be handled at a higher level.
      NOTREACHED();
    default:
      NOTREACHED();
  }
}

NSEvent* CreateMouseEventInWindow(NSWindow* window,
                                  ui::EventType event_type,
                                  const gfx::Point& point_in_root,
                                  const base::TimeTicks time_stamp,
                                  int flags) {
  NSUInteger click_count = 0;
  if (event_type == ui::EventType::kMousePressed ||
      event_type == ui::EventType::kMouseReleased) {
    if (flags & ui::EF_IS_TRIPLE_CLICK)
      click_count = 3;
    else if (flags & ui::EF_IS_DOUBLE_CLICK)
      click_count = 2;
    else
      click_count = 1;
  }
  NSPoint point = ConvertRootPointToTarget(window, point_in_root);
  NSUInteger modifiers = 0;
  NSEventType type = EventTypeToNative(event_type, flags, &modifiers);
  if (event_type == ui::EventType::kMouseEntered ||
      event_type == ui::EventType::kMouseExited) {
    return
        [NSEvent enterExitEventWithType:type
                               location:point
                          modifierFlags:modifiers
                              timestamp:ui::EventTimeStampToSeconds(time_stamp)
                           windowNumber:window.windowNumber
                                context:nil
                            eventNumber:0
                         trackingNumber:0
                               userData:nil];
  }
  return [NSEvent mouseEventWithType:type
                            location:point
                       modifierFlags:modifiers
                           timestamp:ui::EventTimeStampToSeconds(time_stamp)
                        windowNumber:window.windowNumber
                             context:nil
                         eventNumber:0
                          clickCount:click_count
                            pressure:1.0];
}

NSEvent* CreateMouseWheelEventInWindow(NSWindow* window,
                                       const ui::MouseEvent* mouse_event) {
  DCHECK_EQ(mouse_event->type(), ui::EventType::kMousewheel);
  const ui::MouseWheelEvent* mouse_wheel_event =
      mouse_event->AsMouseWheelEvent();
  return cocoa_test_event_utils::TestScrollEvent(
      ConvertRootPointToTarget(window, mouse_wheel_event->location()), window,
      mouse_wheel_event->x_offset(), mouse_wheel_event->y_offset(), false,
      NSEventPhaseNone, NSEventPhaseNone);
}

// Implementation of ui::test::EventGeneratorDelegate for Mac. Everything
// defined inline is just a stub. Interesting overrides are defined below the
// class.
class EventGeneratorDelegateMac : public ui::EventTarget,
                                  public ui::EventSource,
                                  public ui::EventHandler,
                                  public ui::EventProcessor,
                                  public ui::EventTargeter,
                                  public ui::test::EventGeneratorDelegate {
 public:
  EventGeneratorDelegateMac(ui::test::EventGenerator* owner,
                            gfx::NativeWindow root_window,
                            gfx::NativeWindow target_window);
  EventGeneratorDelegateMac(const EventGeneratorDelegateMac&) = delete;
  EventGeneratorDelegateMac& operator=(const EventGeneratorDelegateMac&) =
      delete;
  ~EventGeneratorDelegateMac() override;

  static EventGeneratorDelegateMac* instance() { return instance_; }

  NSEvent* OriginalCurrentEvent(id receiver, SEL selector) {
    return swizzle_current_event_->InvokeOriginal<NSEvent*>(receiver, selector);
  }

  NSWindow* target_window() { return target_window_; }
  ui::test::EventGenerator* owner() { return owner_; }

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override { return true; }
  ui::EventTarget* GetParentTarget() override { return nullptr; }
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override { return this; }

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Overridden from ui::EventSource:
  ui::EventSink* GetEventSink() override { return this; }

  // Overridden from ui::EventProcessor:
  ui::EventTarget* GetRootForEvent(ui::Event* event) override { return this; }
  ui::EventTargeter* GetDefaultEventTargeter() override {
    return this->GetEventTargeter();
  }

  // Overridden from ui::EventDispatcherDelegate (via ui::EventProcessor):
  bool CanDispatchToTarget(EventTarget* target) override { return true; }

  // Overridden from ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    return root;
  }
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override {
    return nullptr;
  }

  // Overridden from ui::test::EventGeneratorDelegate:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return this;
  }
  void SetTargetWindow(gfx::NativeWindow target_window) override {
    target_window_ = target_window.GetNativeNSWindow();
  }
  ui::EventSource* GetEventSource(ui::EventTarget* target) override {
    return this;
  }
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override;
  gfx::Point CenterOfWindow(gfx::NativeWindow window) const override;

  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override {}
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {}
  void ConvertPointFromWindow(gfx::NativeWindow window,
                              gfx::Point* point) const override {}
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override {}

 protected:
  // Overridden from ui::EventDispatcherDelegate (via ui::EventProcessor)
  [[nodiscard]] ui::EventDispatchDetails PreDispatchEvent(
      ui::EventTarget* target,
      ui::Event* event) override;

 private:
  static EventGeneratorDelegateMac* instance_;

  raw_ptr<ui::test::EventGenerator> owner_;
  NSWindow* __strong target_window_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzle_pressed_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzle_location_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzle_current_event_;
  NSMenu* __strong fake_menu_;

  // Mac always sends trackpad scroll events between begin/end phase event
  // markers. If |in_trackpad_scroll| is false, a phase begin event is sent
  // before any trackpad scroll update.
  bool in_trackpad_scroll = false;

  // Timestamp on the last scroll update, used to simulate scroll momentum.
  base::TimeTicks last_scroll_timestamp_;
};

// static
EventGeneratorDelegateMac* EventGeneratorDelegateMac::instance_ = nullptr;

EventGeneratorDelegateMac::EventGeneratorDelegateMac(
    ui::test::EventGenerator* owner,
    gfx::NativeWindow root_window,
    gfx::NativeWindow target_window)
    : owner_(owner) {
  DCHECK(!instance_);
  instance_ = this;
  SetTargetHandler(this);
  // Install a fake "edit" menu.
  fake_menu_ = [[NSMenu alloc] initWithTitle:@"Edit"];
  struct FakeMenuItem {
    NSString* title;
    SEL action;
    NSString* key_equivalent;
  };
  const auto kFakeMenuItems = std::to_array<FakeMenuItem>({
      {@"Undo", @selector(undo:), @"z"},
      {@"Redo", @selector(redo:), @"Z"},
      {@"Copy", @selector(copy:), @"c"},
      {@"Cut", @selector(cut:), @"x"},
      {@"Paste", @selector(paste:), @"v"},
      {@"Select All", @selector(selectAll:), @"a"},
  });
  for (size_t i = 0; i < kFakeMenuItems.size(); ++i) {
    [fake_menu_ insertItemWithTitle:kFakeMenuItems[i].title
                             action:kFakeMenuItems[i].action
                      keyEquivalent:kFakeMenuItems[i].key_equivalent
                            atIndex:i];
  }

  // Mac doesn't use a |root_window|. Assume that if a single-argument
  // constructor was used, it should be the actual |target_window|.
  // TODO(tluk) fix use of the API so this doesn't have to be assumed.
  // (crbug.com/1071628)
  if (!target_window)
    target_window = root_window;

  swizzle_pressed_.reset();
  swizzle_location_.reset();
  swizzle_current_event_.reset();

  SetTargetWindow(target_window);

  // Normally, edit menu items have a `nil` target. This results in -[NSMenu
  // performKeyEquivalent:] relying on -[NSApplication targetForAction:to:from:]
  // to find a target starting at the first responder of the key window. Since
  // non-interactive tests have no key window, that won't work. So set (or
  // clear) the target explicitly on all menu items.
  [fake_menu_.itemArray
      makeObjectsPerformSelector:@selector(setTarget:)
                      withObject:[target_window.GetNativeNSWindow()
                                     firstResponder]];

  if (owner_) {
    swizzle_pressed_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        [NSEvent class], [NSEventDonor class], @selector(pressedMouseButtons));
    swizzle_location_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        [NSEvent class], [NSEventDonor class], @selector(mouseLocation));
    swizzle_current_event_ =
        std::make_unique<base::apple::ScopedObjCClassSwizzler>(
            [NSApplication class], [NSApplicationDonor class],
            @selector(currentEvent));
  }
}

EventGeneratorDelegateMac::~EventGeneratorDelegateMac() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

std::unique_ptr<ui::EventTargetIterator>
EventGeneratorDelegateMac::GetChildIterator() const {
  // Return nullptr to dispatch all events to the result of GetRootTarget().
  return nullptr;
}

void EventGeneratorDelegateMac::OnMouseEvent(ui::MouseEvent* event) {
  NSEvent* ns_event =
      event->type() == ui::EventType::kMousewheel
          ? CreateMouseWheelEventInWindow(target_window_, event)
          : CreateMouseEventInWindow(target_window_, event->type(),
                                     event->location(), event->time_stamp(),
                                     event->flags());

  using Target = ui::test::EventGenerator::Target;
  switch (owner_->target()) {
    case Target::APPLICATION:
      [NSApp sendEvent:ns_event];
      break;
    case Target::WINDOW:
      [target_window_ sendEvent:ns_event];
      break;
    case Target::WIDGET:
      EmulateSendEvent(target_window_, ns_event);
      break;
  }
}

void EventGeneratorDelegateMac::OnKeyEvent(ui::KeyEvent* event) {
  NSUInteger modifiers = EventFlagsToModifiers(event->flags());
  NSEvent* ns_event = cocoa_test_event_utils::SynthesizeKeyEvent(
      target_window_, event->type() == ui::EventType::kKeyPressed,
      event->key_code(), modifiers,
      event->is_char() ? event->GetDomKey() : ui::DomKey::NONE);

  using Target = ui::test::EventGenerator::Target;
  switch (owner_->target()) {
    case Target::APPLICATION:
      [NSApp sendEvent:ns_event];
      break;
    case Target::WINDOW:
      // -[NSApp sendEvent:] sends -performKeyEquivalent: if Command or Control
      // modifiers are pressed. Emulate that behavior.
      if (ns_event.type == NSEventTypeKeyDown &&
          (ns_event.modifierFlags &
           (NSEventModifierFlagControl | NSEventModifierFlagCommand)) &&
          [target_window_ performKeyEquivalent:ns_event]) {
        break;  // Handled by performKeyEquivalent:.
      }

      [target_window_ sendEvent:ns_event];
      break;
    case Target::WIDGET:
      if ([fake_menu_ performKeyEquivalent:ns_event])
        return;

      EmulateSendEvent(target_window_, ns_event);
      break;
  }
}

void EventGeneratorDelegateMac::OnTouchEvent(ui::TouchEvent* event) {
  NOTREACHED() << "Touchscreen events not supported on Chrome Mac.";
}

void EventGeneratorDelegateMac::OnScrollEvent(ui::ScrollEvent* event) {
  // Ignore FLING_CANCEL. Cocoa provides a continuous stream of events during a
  // fling. For now, this method simulates a momentum stream using a single
  // update with a momentum phase (plus begin/end phase events), triggered when
  // the EventGenerator requests a FLING_START.
  if (event->type() == ui::EventType::kScrollFlingCancel) {
    return;
  }

  NSPoint location =
      ConvertRootPointToTarget(target_window_, event->location());

  // MAY_BEGIN/END comes from the EventGenerator for trackpad rests.
  if (event->momentum_phase() == ui::EventMomentumPhase::MAY_BEGIN ||
      event->momentum_phase() == ui::EventMomentumPhase::END) {
    DCHECK_EQ(0, event->x_offset());
    DCHECK_EQ(0, event->y_offset());
    NSEventPhase phase =
        event->momentum_phase() == ui::EventMomentumPhase::MAY_BEGIN
            ? NSEventPhaseMayBegin
            : NSEventPhaseCancelled;

    NSEvent* rest = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, 0, 0, true, phase, NSEventPhaseNone);
    EmulateSendEvent(target_window_, rest);

    // Allow the next ScrollSequence to skip the "begin".
    in_trackpad_scroll = phase == NSEventPhaseMayBegin;
    return;
  }

  NSEventPhase event_phase = NSEventPhaseBegan;
  NSEventPhase momentum_phase = NSEventPhaseNone;

  // Treat FLING_START as the beginning of a momentum phase.
  if (event->type() == ui::EventType::kScrollFlingStart) {
    DCHECK(in_trackpad_scroll);
    // First end the non-momentum phase.
    NSEvent* end = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, 0, 0, true, NSEventPhaseEnded,
        NSEventPhaseNone);
    EmulateSendEvent(target_window_, end);
    in_trackpad_scroll = false;

    // Assume a zero time delta means no fling. Just end the event phase.
    if (event->time_stamp() == last_scroll_timestamp_)
      return;

    // Otherwise, switch phases for the "fling".
    std::swap(event_phase, momentum_phase);
  }

  // Send a begin for the current event phase, unless it's already in progress.
  if (!in_trackpad_scroll) {
    NSEvent* begin = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, 0, 0, true, event_phase, momentum_phase);
    EmulateSendEvent(target_window_, begin);
    in_trackpad_scroll = true;
  }

  if (event->type() == ui::EventType::kScroll) {
    NSEvent* update = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, -event->x_offset(), -event->y_offset(), true,
        NSEventPhaseChanged, NSEventPhaseNone);
    EmulateSendEvent(target_window_, update);
  } else {
    DCHECK_EQ(event->type(), ui::EventType::kScrollFlingStart);
    // Mac generates a stream of events. For the purposes of testing, just
    // generate one.
    NSEvent* update = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, -event->x_offset(), -event->y_offset(), true,
        NSEventPhaseNone, NSEventPhaseChanged);
    EmulateSendEvent(target_window_, update);

    // Never leave the momentum part hanging.
    NSEvent* end = cocoa_test_event_utils::TestScrollEvent(
        location, target_window_, 0, 0, true, NSEventPhaseNone,
        NSEventPhaseEnded);
    EmulateSendEvent(target_window_, end);
    in_trackpad_scroll = false;
  }

  last_scroll_timestamp_ = event->time_stamp();
}

gfx::Point EventGeneratorDelegateMac::CenterOfTarget(
    const ui::EventTarget* target) const {
  DCHECK_EQ(target, this);
  return CenterOfWindow(gfx::NativeWindow(target_window_));
}

gfx::Point EventGeneratorDelegateMac::CenterOfWindow(
    gfx::NativeWindow native_window) const {
  NSWindow* window = native_window.GetNativeNSWindow();
  DCHECK_EQ(window, target_window_);
  // Assume the window is at the top-left of the coordinate system (even if
  // AppKit has moved it into the work area) see ConvertRootPointToTarget().
  return gfx::Point(NSWidth([window frame]) / 2, NSHeight([window frame]) / 2);
}

ui::EventDispatchDetails EventGeneratorDelegateMac::PreDispatchEvent(
    ui::EventTarget* target,
    ui::Event* event) {
  // Set the TestScreen's cursor point before mouse event dispatch. The
  // Screen's value is checked by views controls and other UI components; this
  // pattern matches aura::WindowEventDispatcher::PreDispatchMouseEvent().
  if (event->IsMouseEvent()) {
    ui::MouseEvent* mouse_event = event->AsMouseEvent();
    // Similar to the logic in Aura's
    // EnvInputStateController::UpdateStateForMouseEvent(), capture change and
    // synthesized events don't need to update the cursor location.
    if (mouse_event->type() != ui::EventType::kMouseCaptureChanged &&
        !(mouse_event->flags() & ui::EF_IS_SYNTHESIZED)) {
      // Update the cursor location on screen.
      owner_->set_current_screen_location(mouse_event->root_location());
      display::Screen::GetScreen()->SetCursorScreenPointForTesting(
          mouse_event->root_location());
    }
  }
  return ui::EventDispatchDetails();
}

ui::test::EventGenerator* GetActiveGenerator() {
  return EventGeneratorDelegateMac::instance()
             ? EventGeneratorDelegateMac::instance()->owner()
             : nullptr;
}

}  // namespace

namespace views::test {

std::unique_ptr<ui::test::EventGeneratorDelegate>
CreateEventGeneratorDelegateMac(ui::test::EventGenerator* owner,
                                gfx::NativeWindow root_window,
                                gfx::NativeWindow target_window) {
  return std::make_unique<EventGeneratorDelegateMac>(owner, root_window,
                                                     target_window);
}
}  // namespace views::test

@implementation NSEventDonor

// Donate +[NSEvent pressedMouseButtons] by retrieving the flags from the
// active generator.
+ (NSUInteger)pressedMouseButtons {
  ui::test::EventGenerator* generator = GetActiveGenerator();
  if (!generator)
    return [NSEventDonor pressedMouseButtons];  // Call original implementation.

  int flags = generator->flags();
  NSUInteger bitmask = 0;
  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    bitmask |= 1;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    bitmask |= 1 << 1;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    bitmask |= 1 << 2;
  return bitmask;
}

// Donate +[NSEvent mouseLocation] by retrieving the current position on screen.
+ (NSPoint)mouseLocation {
  ui::test::EventGenerator* generator = GetActiveGenerator();
  if (!generator)
    return [NSEventDonor mouseLocation];  // Call original implementation.

  // The location is the point in the root window which, for desktop widgets, is
  // the widget itself.
  gfx::Point point_in_root = generator->current_screen_location();
  NSWindow* window = EventGeneratorDelegateMac::instance()->target_window();
  NSPoint point_in_window = ConvertRootPointToTarget(window, point_in_root);
  return [window convertPointToScreen:point_in_window];
}

@end

@implementation NSApplicationDonor

- (NSEvent*)currentEvent {
  if (g_current_event)
    return g_current_event;

  // Find the original implementation and invoke it.
  return EventGeneratorDelegateMac::instance()->OriginalCurrentEvent(self,
                                                                     _cmd);
}

@end
