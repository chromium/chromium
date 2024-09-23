// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_EVENT_GENERATOR_H_
#define UI_EVENTS_TEST_EVENT_GENERATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class EventSource;
class EventTarget;

namespace test {

// See EventGenerator::GestureScrollSequenceWithCallback for details.
using ScrollStepCallback =
    base::RepeatingCallback<void(EventType, const gfx::Vector2dF&)>;

class TestTickClock;
class EventGenerator;

// A delegate interface for EventGenerator to abstract platform-specific event
// targeting and coordinate conversion.
class EventGeneratorDelegate {
 public:
  virtual ~EventGeneratorDelegate() {}

  // This factory function is used by EventGenerator to create a delegate if an
  // EventGeneratorDelegate was not supplied to the constructor.
  //
  // Note: Implementations for Windows/Linux, ChromeOS and Mac differ in the way
  // they handle the |root_window| and |target_window|. On Windows/Linux all
  // events are dispatched through the provided |root_window| and the
  // |target_window| is ignored. On ChromeOS both the |root_window| and
  // |target_window| are ignored and all events are dispatched through the root
  // window deduced using the event's screen coordinates. On Mac the concept of
  // a |root_window| doesn't exist and events will only be dispatched to the
  // specified |target_window|.
  using FactoryFunction =
      base::RepeatingCallback<std::unique_ptr<EventGeneratorDelegate>(
          EventGenerator* owner,
          gfx::NativeWindow root_window,
          gfx::NativeWindow target_window)>;
  static void SetFactoryFunction(FactoryFunction factory);

  // Sets the |root_window| on Windows/Linux, ignored on ChromeOS, sets the
  // |target_window| on Mac.
  virtual void SetTargetWindow(gfx::NativeWindow target_window) = 0;

  // The ui::EventTarget at the given |location|.
  virtual EventTarget* GetTargetAt(const gfx::Point& location) = 0;

  // The ui::EventSource for the given |target|.
  virtual EventSource* GetEventSource(EventTarget* target) = 0;

  // Helper functions to determine the center point of |target| or |window|.
  virtual gfx::Point CenterOfTarget(const EventTarget* target) const = 0;
  virtual gfx::Point CenterOfWindow(gfx::NativeWindow window) const = 0;

  // Convert a point between screen coordinates and |target|'s coordinates.
  virtual void ConvertPointFromTarget(const EventTarget* target,
                                      gfx::Point* point) const = 0;
  virtual void ConvertPointToTarget(const EventTarget* target,
                                    gfx::Point* point) const = 0;

  // Converts |point| from |window|'s coordinates to screen coordinates.
  virtual void ConvertPointFromWindow(gfx::NativeWindow window,
                                      gfx::Point* point) const = 0;

  // Convert a point from the coordinate system in the host that contains
  // |hosted_target| into the root window's coordinate system.
  virtual void ConvertPointFromHost(const EventTarget* hosted_target,
                                    gfx::Point* point) const = 0;
};

// ui::test::EventGenerator is a tool that generates and dispatches events.
// Unlike |ui_controls| package in ui/base/test, this does not use platform
// native message loops. Instead, it sends events to the event dispatcher
// synchronously.
//
// This class is not suited for the following cases:
//
// 1) If your test depends on native events (ui::Event::native_event()).
//   This return is empty/NULL event with EventGenerator.
// 2) If your test involves nested run loop, such as
//    menu or drag & drop. Because this class directly
//    post an event to WindowEventDispatcher, this event will not be
//    handled in the nested run loop.
// 3) Similarly, |base::MessagePumpObserver| will not be invoked.
// 4) Any other code that requires native message loops, such as
//    tests for WindowTreeHostWin/WindowTreeHostX11.
//
// If one of these applies to your test, please use |ui_controls|
// package instead.
//
// Note: The coordinates of the points in API is determined by the
// EventGeneratorDelegate.
class EventGenerator {
 public:
  // Some test suites (e.g. chromeos_integration_tests) must use ui_controls.h
  // and cannot use EventGenerator. Calling this function causes
  // EventGenerator::Init() to CHECK-fail with a helpful message.
  static void BanEventGenerator();

  // Create an EventGenerator with EventGeneratorDelegate,
  // which uses the coordinates conversions and targeting provided by
  // |delegate|.
  explicit EventGenerator(std::unique_ptr<EventGeneratorDelegate> delegate);

  // Creates an EventGenerator with the mouse/touch location (0,0),
  // which uses the |root_window|'s coordinates and the default delegate for
  // this platform.
  explicit EventGenerator(gfx::NativeWindow root_window);

  // Creates an EventGenerator with the mouse/touch location
  // at |initial_location|, which uses the |root_window|'s coordinates.
  EventGenerator(gfx::NativeWindow root_window,
                 const gfx::Point& initial_location);

  // Creates an EventGenerator with the mouse/touch location centered over
  // |target_window|.
  EventGenerator(gfx::NativeWindow root_window,
                 gfx::NativeWindow target_window);

  EventGenerator(const EventGenerator&) = delete;
  EventGenerator& operator=(const EventGenerator&) = delete;

  virtual ~EventGenerator();

  // Explicitly sets the location used by mouse/touch events, in screen
  // coordinates. This is set by the various methods that take a location but
  // can be manipulated directly, typically for touch.
  void set_current_screen_location(const gfx::Point& location) {
    current_screen_location_ = location;
  }
  const gfx::Point& current_screen_location() const {
    return current_screen_location_;
  }

  // Events could be dispatched using different methods. The choice is a
  // tradeoff between test robustness and coverage of OS internals that affect
  // event dispatch.
  // Currently only supported on Mac.
  enum class Target {
    // Dispatch through the application. Least robust.
    APPLICATION,
    // Dispatch directly to target NSWindow via -sendEvent:.
    WINDOW,
    // Default. Emulates default NSWindow dispatch: calls specific event handler
    // based on event type. Most robust.
    WIDGET,
  };

  // Determines the end state of a scroll sequence.
  enum class ScrollSequenceType {
    // Create an "incomplete" scroll sequence, meaning that it ends with the
    // fingers resting on the trackpad.
    ScrollOnly,
    // Create a "complete" scroll sequence, meaning that it ends with the
    // fingers being lifted off of the trackpad.
    UpToFling,
  };

  // Updates the |current_screen_location_| to point to the middle of the target
  // window and sets the appropriate dispatcher target.
  void SetTargetWindow(gfx::NativeWindow target_window);

  // Selects dispatch method. Currently only supported on Mac.
  void set_target(Target target) { target_ = target; }
  Target target() const { return target_; }

  // Resets the event flags bitmask.
  void set_flags(int flags) { flags_ = flags; }
  int flags() const { return flags_; }

  // Resets the Mouse Event device ID.
  void set_mouse_source_device_id(
      int source_device_id = ui::ED_UNKNOWN_DEVICE) {
    mouse_source_device_id_ = source_device_id;
  }

  // Generates a button press / release event.
  void PressButton(int flag);
  void ReleaseButton(int flag);

  // Generates a left button press event.
  void PressLeftButton();

  // Generates a left button release event.
  void ReleaseLeftButton();

  // Generates events to click (press, release) left button.
  void ClickLeftButton();

  // Generates events to click (press, release) right button.
  void ClickRightButton();

  // Generates a double click event using the left button.
  void DoubleClickLeftButton();

  // Generates a right button press event.
  void PressRightButton();

  // Generates a right button release event.
  void ReleaseRightButton();

  // Moves the mouse wheel by |delta_x|, |delta_y|.
  void MoveMouseWheel(int delta_x, int delta_y);

  // Generates a mouse enter event.
  void SendMouseEnter();

  // Generates a mouse exit.
  void SendMouseExit();

  // Generates events to move mouse to be the given |point| in the
  // |current_root_window_|'s host window coordinates.
  void MoveMouseToInHost(const gfx::Point& point_in_host);
  void MoveMouseToInHost(int x, int y) {
    MoveMouseToInHost(gfx::Point(x, y));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Generates a mouse move event at the point given in the host
  // coordinates, with a native event with |point_for_natve|.
  void MoveMouseToWithNative(const gfx::Point& point_in_host,
                             const gfx::Point& point_for_native);
#endif

  // Generates events to move mouse to be the given |point| in screen
  // coordinates.
  void MoveMouseTo(const gfx::Point& point_in_screen, int count);
  void MoveMouseTo(const gfx::Point& point_in_screen) {
    MoveMouseTo(point_in_screen, 1);
  }
  void MoveMouseTo(int x, int y) {
    MoveMouseTo(gfx::Point(x, y));
  }

  // Generates events to move mouse to be the given |point| in |window|'s
  // coordinates.
  void MoveMouseRelativeTo(const EventTarget* window, const gfx::Point& point);
  void MoveMouseRelativeTo(const EventTarget* window, int x, int y) {
    MoveMouseRelativeTo(window, gfx::Point(x, y));
  }

  void MoveMouseBy(int x, int y) {
    MoveMouseTo(current_screen_location_ + gfx::Vector2d(x, y));
  }

  // Generates events to drag mouse to given |point|.
  void DragMouseTo(const gfx::Point& point);

  void DragMouseTo(int x, int y) {
    DragMouseTo(gfx::Point(x, y));
  }

  void DragMouseBy(int dx, int dy) {
    DragMouseTo(current_screen_location_ + gfx::Vector2d(dx, dy));
  }

  // Generates events to move the mouse to the center of the window.
  void MoveMouseToCenterOf(EventTarget* window);

  // Enter pen-pointer mode, which will cause any generated mouse events to have
  // a pointer type ui::EventPointerType::kPen.
  void EnterPenPointerMode();

  // Exit pen-pointer mode. Generated mouse events will use the default pointer
  // type event.
  void ExitPenPointerMode();

  // Set radius of touch PointerDetails.
  void SetTouchRadius(float x, float y);

  // Set tilt of touch PointerDetails.
  void SetTouchTilt(float x, float y);

  // Set pointer type of touch PointerDetails.
  void SetTouchPointerType(ui::EventPointerType type) {
    touch_pointer_details_.pointer_type = type;
  }

  // Set force of touch PointerDetails.
  void SetTouchForce(float force) { touch_pointer_details_.force = force; }

  // Generates a touch press event. If |touch_location_in_screen| is not null,
  // the touch press event will happen at |touch_location_in_screen|. Otherwise,
  // it will happen at the current event location |current_screen_location_|.
  void PressTouch(
      const std::optional<gfx::Point>& touch_location_in_screen = std::nullopt);

  // Generates a touch press event with |touch_id|. See PressTouch() event for
  // the description of |touch_location_in_screen| parameter.
  void PressTouchId(
      int touch_id,
      const std::optional<gfx::Point>& touch_location_in_screen = std::nullopt);

  // Generates a EventType::kTouchMoved event to |point|.
  void MoveTouch(const gfx::Point& point);

  // Generates a EventType::kTouchMoved event moving by (x, y) from current
  // location.
  void MoveTouchBy(int x, int y) {
    MoveTouch(current_screen_location_ + gfx::Vector2d(x, y));
  }

  // Generates a EventType::kTouchMoved event to |point| with |touch_id|.
  void MoveTouchId(const gfx::Point& point, int touch_id);

  // Generates a EventType::kTouchMoved event moving (x, y) from current
  // location with |touch_id|.
  void MoveTouchIdBy(int touch_id, int x, int y) {
    MoveTouchId(current_screen_location_ + gfx::Vector2d(x, y), touch_id);
  }

  // Generates a touch release event.
  void ReleaseTouch();

  // Generates a touch release event with |touch_id|.
  void ReleaseTouchId(int touch_id);

  // Generates a touch cancel event.
  void CancelTouch();

  // Generates a touch cancel event with |touch_id|.
  void CancelTouchId(int touch_id);

  // Generates press, move and release event to move touch
  // to be the given |point|.
  void PressMoveAndReleaseTouchTo(const gfx::Point& point);

  void PressMoveAndReleaseTouchTo(int x, int y) {
    PressMoveAndReleaseTouchTo(gfx::Point(x, y));
  }

  void PressMoveAndReleaseTouchBy(int x, int y) {
    PressMoveAndReleaseTouchTo(current_screen_location_ + gfx::Vector2d(x, y));
  }

  // Generates press, move and release events to move touch
  // to the center of the window.
  void PressMoveAndReleaseTouchToCenterOf(EventTarget* window);

  // Generates and dispatches touch-events required to generate a TAP gesture.
  // Note that this can generate a number of other gesture events at the same
  // time (e.g. GESTURE_BEGIN, TAP_DOWN, END).
  void GestureTapAt(const gfx::Point& point);

  // Generates press and release touch-events to generate a TAP_DOWN event, but
  // without generating any scroll or tap events. This can also generate a few
  // other gesture events (e.g. GESTURE_BEGIN, END).
  void GestureTapDownAndUp(const gfx::Point& point);

  // Calculates a time duration that can be used with the given |start|, |end|,
  // and |steps| values when calling GestureScrollSequence (or
  // GestureScrollSequenceWithCallback) to achieve the given |velocity|.
  base::TimeDelta CalculateScrollDurationForFlingVelocity(
      const gfx::Point& start,
      const gfx::Point& end,
      float velocity,
      int steps);

  // Generates press, move, release touch-events to generate a sequence of
  // scroll events. |duration| and |steps| affect the velocity of the scroll,
  // and depending on these values, this may also generate FLING scroll
  // gestures. If velocity/fling is irrelevant for the test, then any non-zero
  // values for these should be sufficient.
  void GestureScrollSequence(const gfx::Point& start,
                             const gfx::Point& end,
                             const base::TimeDelta& duration,
                             int steps);

  // The same as GestureScrollSequence(), with the exception that |callback| is
  // called at each step of the scroll sequence. |callback| is called at the
  // start of the sequence with EventType::kGestureScrollBegin, followed by one
  // or more EventType::kGestureScrollUpdate and ends with an
  // EventType::kGestureScrollEnd.
  void GestureScrollSequenceWithCallback(const gfx::Point& start,
                                         const gfx::Point& end,
                                         const base::TimeDelta& duration,
                                         int steps,
                                         const ScrollStepCallback& callback);

  // Generates press, move, release touch-events to generate a sequence of
  // multi-finger scroll events. |count| specifies the number of touch-points
  // that should generate the scroll events. |start| are the starting positions
  // of all the touch points. |delta| specifies the moving vectors for all
  // fingers. |delay_adding_finger_ms| are delays in ms from the starting time
  // till touching down of each finger. |delay_releasing_finger_ms| are delays
  // in ms from starting time till touching release of each finger. These two
  // parameters are useful when testing complex gestures that start with 1 or 2
  // fingers and add fingers with a delay. |steps| and
  // |event_separation_time_ms| are relevant when testing velocity/fling/swipe,
  // otherwise these can be any non-zero value.
  void GestureMultiFingerScrollWithDelays(int count,
                                          const gfx::Point start[],
                                          const gfx::Vector2d delta[],
                                          const int delay_adding_finger_ms[],
                                          const int delay_releasing_finger_ms[],
                                          int event_separation_time_ms,
                                          int steps);

  // Similar to GestureMultiFingerScrollWithDelays() above. Generates press,
  // move, release touch-events to generate a sequence of multi-finger scroll
  // events. All fingers are released at the end of scrolling together. All
  // fingers move the same amount specified by |move_x| and |move_y|.
  void GestureMultiFingerScrollWithDelays(int count,
                                          const gfx::Point start[],
                                          const int delay_adding_finger_ms[],
                                          int event_separation_time_ms,
                                          int steps,
                                          int move_x,
                                          int move_y);

  // Similar to GestureMultiFingerScrollWithDelays(). Generates press, move,
  // release touch-events to generate a sequence of multi-finger scroll events.
  // All fingers are pressed at the beginning together and are released at the
  // end of scrolling together. All fingers move move the same amount specified
  // by |move_x| and |move_y|.
  void GestureMultiFingerScroll(int count,
                                const gfx::Point start[],
                                int event_separation_time_ms,
                                int steps,
                                int move_x,
                                int move_y);

  // Generates scroll sequences starting with a FlingCancel, followed by Scrolls
  // with constant deltas to `x_offset` and `y_offset` in `steps`. If
  // `end_state` == `UpToFling`, the scroll sequences end with a FlingStart.
  // Otherwise, return early to simulate the fingers still resting on the
  // trackpad.
  void ScrollSequence(
      const gfx::Point& start,
      const base::TimeDelta& step_delay,
      float x_offset,
      float y_offset,
      int steps,
      int num_fingers,
      ScrollSequenceType end_state = ScrollSequenceType::UpToFling);

  // Generate a TrackPad "rest" event. That is, a user resting fingers on the
  // trackpad without moving. This may then be followed by a ScrollSequence(),
  // or a CancelTrackpadRest().
  void GenerateTrackpadRest();

  // Cancels a previous GenerateTrackpadRest(). That is, a user lifting fingers
  // from the trackpad without having moved them in any direction.
  void CancelTrackpadRest();

  // Generates a key press event. On platforms except Windows and X11, a key
  // event without native_event() is generated. Note that ui::EF_ flags should
  // be passed as |flags|, not the native ones like 'ShiftMask' in <X11/X.h>.
  // TODO(yusukes): Support native_event() on all platforms.
  void PressKey(KeyboardCode key_code,
                int flags,
                int source_device_id = ED_UNKNOWN_DEVICE);

  // Generates a key release event. On platforms except Windows and X11, a key
  // event without native_event() is generated. Note that ui::EF_ flags should
  // be passed as |flags|, not the native ones like 'ShiftMask' in <X11/X.h>.
  // TODO(yusukes): Support native_event() on all platforms.
  void ReleaseKey(KeyboardCode key_code,
                  int flags,
                  int source_device_id = ED_UNKNOWN_DEVICE);

  // Calls PressKey() then ReleaseKey() to simulate typing one character.
  void PressAndReleaseKey(KeyboardCode key_code,
                          int flags = EF_NONE,
                          int source_device_id = ED_UNKNOWN_DEVICE);

  // Emulates pressing modidifer keys from given flags.
  void PressModifierKeys(int flags, int source_device_id = ED_UNKNOWN_DEVICE);

  // Emulates releasing modifier keys from given flags.
  void ReleaseModifierKeys(int flags, int source_device_id = ED_UNKNOWN_DEVICE);

  // Emulates press key with modifiers. If modifier key `flags` are specified,
  // corresponding key press events are first sent, then key press event for
  // the given `key` will be sent.
  void PressKeyAndModifierKeys(KeyboardCode key,
                               int flags,
                               int source_device_id = ED_UNKNOWN_DEVICE);
  // Emulates release key with modifiers. If modifier key `flags` are specified,
  // corresponding key release events are sent after the key release event for
  // the given `key`.
  void ReleaseKeyAndModifierKeys(KeyboardCode key,
                                 int flags,
                                 int source_device_id = ED_UNKNOWN_DEVICE);

  // Convenient alias to call PressKeyAndModifierKeys() followed by
  // ReleaseKeyAndModifierKeys() to emulate user key event sequence.
  void PressAndReleaseKeyAndModifierKeys(
      KeyboardCode key,
      int flags,
      int source_device_id = ED_UNKNOWN_DEVICE);

  // Dispatch the event to the WindowEventDispatcher.
  void Dispatch(Event* event);

  // Advances the event generator's clock by `delta`.
  void AdvanceClock(const base::TimeDelta& delta);

  void set_current_target(EventTarget* target) {
    current_target_ = target;
  }

  const EventGeneratorDelegate* delegate() const { return delegate_.get(); }
  EventGeneratorDelegate* delegate() { return delegate_.get(); }

 private:
  // Set up the test context using the delegate.
  void Init(gfx::NativeWindow root_window, gfx::NativeWindow target_window);

  // Dispatch a key event to the WindowEventDispatcher.
  void DispatchKeyEvent(bool is_press,
                        KeyboardCode key_code,
                        int flags,
                        int source_device_id);

  void SetCurrentScreenLocation(const gfx::Point& point);
  void UpdateCurrentDispatcher(const gfx::Point& point);

  gfx::Point GetLocationInCurrentRoot() const;
  gfx::Point CenterOfWindow(const EventTarget* window) const;

  std::unique_ptr<EventGeneratorDelegate> delegate_;
  gfx::Point current_screen_location_;
  raw_ptr<EventTarget, AcrossTasksDanglingUntriaged> current_target_ = nullptr;
  int flags_ = 0;
  int mouse_source_device_id_ = ui::ED_UNKNOWN_DEVICE;
  bool grab_ = false;

  ui::PointerDetails touch_pointer_details_;

  Target target_ = Target::WIDGET;

  std::unique_ptr<TestTickClock> tick_clock_;
};

// This generates key events for moidfiers as well as the key with
// modifiers.
// TODO(crbug.com/40256427): Remove this once the EventGenerator is
// modified to generate the same sequence.
void EmulateFullKeyPressReleaseSequence(test::EventGenerator* generator,
                                        KeyboardCode key,
                                        bool control,
                                        bool shift,
                                        bool alt,
                                        bool command);

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_EVENT_GENERATOR_H_
