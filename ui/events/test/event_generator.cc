// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/event_generator.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/x/x11.h"
#endif

#if defined(OS_WIN)
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

namespace ui {
namespace test {

// A TickClock that advances time by one millisecond on each call to NowTicks().
class TestTickClock : public base::TickClock {
 public:
  TestTickClock() = default;

  // Unconditionally returns a tick count that is 1ms later than the previous
  // call, starting at 1ms.
  base::TimeTicks NowTicks() const override {
    static constexpr base::TimeDelta kOneMillisecond =
        base::TimeDelta::FromMilliseconds(1);
    return ticks_ += kOneMillisecond;
  }

 private:
  mutable base::TimeTicks ticks_;

  DISALLOW_COPY_AND_ASSIGN(TestTickClock);
};

namespace {

void DummyCallback(EventType, const gfx::Vector2dF&) {}

class TestTouchEvent : public ui::TouchEvent {
 public:
  TestTouchEvent(ui::EventType type,
                 const gfx::Point& root_location,
                 int touch_id,
                 int flags,
                 base::TimeTicks timestamp)
      : TouchEvent(type,
                   root_location,
                   timestamp,
                   ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      /* pointer_id*/ touch_id,
                                      /* radius_x */ 1.0f,
                                      /* radius_y */ 1.0f,
                                      /* force */ 0.0f),
                   flags) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTouchEvent);
};

const int kAllButtonMask = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON;

EventGeneratorDelegate::FactoryFunction g_event_generator_delegate_factory;

}  // namespace

// static
void EventGeneratorDelegate::SetFactoryFunction(FactoryFunction factory) {
  g_event_generator_delegate_factory = std::move(factory);
}

EventGenerator::EventGenerator(std::unique_ptr<EventGeneratorDelegate> delegate)
    : delegate_(std::move(delegate)) {
  Init(nullptr, nullptr);
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window) {
  Init(root_window, nullptr);
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window,
                               const gfx::Point& point)
    : current_screen_location_(point) {
  Init(root_window, nullptr);
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window,
                               gfx::NativeWindow window) {
  Init(root_window, window);
}

EventGenerator::~EventGenerator() {
  ui::SetEventTickClockForTesting(nullptr);
}

void EventGenerator::PressLeftButton() {
  PressButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void EventGenerator::ReleaseLeftButton() {
  ReleaseButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void EventGenerator::ClickLeftButton() {
  PressLeftButton();
  ReleaseLeftButton();
}

void EventGenerator::ClickRightButton() {
  PressRightButton();
  ReleaseRightButton();
}

void EventGenerator::DoubleClickLeftButton() {
  flags_ &= ~ui::EF_IS_DOUBLE_CLICK;
  ClickLeftButton();
  flags_ |= ui::EF_IS_DOUBLE_CLICK;
  ClickLeftButton();
  flags_ &= ~ui::EF_IS_DOUBLE_CLICK;
}

void EventGenerator::PressRightButton() {
  PressButton(ui::EF_RIGHT_MOUSE_BUTTON);
}

void EventGenerator::ReleaseRightButton() {
  ReleaseButton(ui::EF_RIGHT_MOUSE_BUTTON);
}

void EventGenerator::MoveMouseWheel(int delta_x, int delta_y) {
  gfx::Point location = GetLocationInCurrentRoot();
  ui::MouseWheelEvent wheelev(gfx::Vector2d(delta_x, delta_y), location,
                              location, ui::EventTimeForNow(), flags_, 0);
  Dispatch(&wheelev);
}

void EventGenerator::SendMouseEnter() {
  gfx::Point enter_location(current_screen_location_);
  delegate()->ConvertPointToTarget(current_target_, &enter_location);
  ui::MouseEvent mouseev(ui::ET_MOUSE_ENTERED, enter_location, enter_location,
                         ui::EventTimeForNow(), flags_, 0);
  Dispatch(&mouseev);
}

void EventGenerator::SendMouseExit() {
  gfx::Point exit_location(current_screen_location_);
  delegate()->ConvertPointToTarget(current_target_, &exit_location);
  ui::MouseEvent mouseev(ui::ET_MOUSE_EXITED, exit_location, exit_location,
                         ui::EventTimeForNow(), flags_, 0);
  Dispatch(&mouseev);
}

#if defined(OS_CHROMEOS)
void EventGenerator::MoveMouseToWithNative(const gfx::Point& point_in_host,
                                           const gfx::Point& point_for_native) {
  // Ozone uses the location in native event as a system location.
  // Create a fake event with the point in host, which will be passed
  // to the non native event, then update the native event with the native
  // (root) one.
  std::unique_ptr<ui::MouseEvent> native_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, point_in_host, point_in_host,
                         ui::EventTimeForNow(), flags_, 0));
  ui::MouseEvent mouseev(native_event.get());
  native_event->set_location(point_for_native);
  Dispatch(&mouseev);

  current_screen_location_ = point_in_host;
  delegate()->ConvertPointFromHost(current_target_, &current_screen_location_);
}
#endif

void EventGenerator::MoveMouseToInHost(const gfx::Point& point_in_host) {
  const ui::EventType event_type = (flags_ & ui::EF_LEFT_MOUSE_BUTTON) ?
      ui::ET_MOUSE_DRAGGED : ui::ET_MOUSE_MOVED;
  ui::MouseEvent mouseev(event_type, point_in_host, point_in_host,
                         ui::EventTimeForNow(), flags_, 0);
  Dispatch(&mouseev);

  current_screen_location_ = point_in_host;
  delegate()->ConvertPointFromHost(current_target_, &current_screen_location_);
}

void EventGenerator::MoveMouseTo(const gfx::Point& point_in_screen,
                                 int count) {
  DCHECK_GT(count, 0);
  const ui::EventType event_type = (flags_ & ui::EF_LEFT_MOUSE_BUTTON) ?
      ui::ET_MOUSE_DRAGGED : ui::ET_MOUSE_MOVED;

  gfx::Vector2dF diff(point_in_screen - current_screen_location_);
  for (float i = 1; i <= count; i++) {
    gfx::Vector2dF step(diff);
    step.Scale(i / count);
    gfx::Point move_point =
        current_screen_location_ + gfx::ToRoundedVector2d(step);
    if (!grab_)
      UpdateCurrentDispatcher(move_point);
    delegate()->ConvertPointToTarget(current_target_, &move_point);
    ui::MouseEvent mouseev(event_type, move_point, move_point,
                           ui::EventTimeForNow(), flags_, 0);
    Dispatch(&mouseev);
  }
  current_screen_location_ = point_in_screen;
}

void EventGenerator::MoveMouseRelativeTo(const EventTarget* window,
                                         const gfx::Point& point_in_parent) {
  gfx::Point point(point_in_parent);
  delegate()->ConvertPointFromTarget(window, &point);
  MoveMouseTo(point);
}

void EventGenerator::DragMouseTo(const gfx::Point& point) {
  PressLeftButton();
  MoveMouseTo(point);
  ReleaseLeftButton();
}

void EventGenerator::MoveMouseToCenterOf(EventTarget* window) {
  MoveMouseTo(CenterOfWindow(window));
}

void EventGenerator::EnterPenPointerMode() {
  touch_pointer_details_.pointer_type = ui::EventPointerType::POINTER_TYPE_PEN;
}

void EventGenerator::ExitPenPointerMode() {
  touch_pointer_details_.pointer_type =
      ui::EventPointerType::POINTER_TYPE_TOUCH;
}

void EventGenerator::SetTouchRadius(float x, float y) {
  touch_pointer_details_.radius_x = x;
  touch_pointer_details_.radius_y = y;
}

void EventGenerator::SetTouchTilt(float x, float y) {
  touch_pointer_details_.tilt_x = x;
  touch_pointer_details_.tilt_y = y;
}

void EventGenerator::PressTouch() {
  PressTouchId(0);
}

void EventGenerator::PressTouchId(int touch_id) {
  TestTouchEvent touchev(ui::ET_TOUCH_PRESSED, GetLocationInCurrentRoot(),
                         touch_id, flags_, ui::EventTimeForNow());
  Dispatch(&touchev);
}

void EventGenerator::MoveTouch(const gfx::Point& point) {
  MoveTouchId(point, 0);
}

void EventGenerator::MoveTouchId(const gfx::Point& point, int touch_id) {
  current_screen_location_ = point;
  TestTouchEvent touchev(ui::ET_TOUCH_MOVED, GetLocationInCurrentRoot(),
                         touch_id, flags_, ui::EventTimeForNow());
  Dispatch(&touchev);

  if (!grab_)
    UpdateCurrentDispatcher(point);
}

void EventGenerator::ReleaseTouch() {
  ReleaseTouchId(0);
}

void EventGenerator::ReleaseTouchId(int touch_id) {
  TestTouchEvent touchev(ui::ET_TOUCH_RELEASED, GetLocationInCurrentRoot(),
                         touch_id, flags_, ui::EventTimeForNow());
  Dispatch(&touchev);
}

void EventGenerator::PressMoveAndReleaseTouchTo(const gfx::Point& point) {
  PressTouch();
  MoveTouch(point);
  ReleaseTouch();
}

void EventGenerator::PressMoveAndReleaseTouchToCenterOf(EventTarget* window) {
  PressMoveAndReleaseTouchTo(CenterOfWindow(window));
}

void EventGenerator::GestureTapAt(const gfx::Point& location) {
  UpdateCurrentDispatcher(location);
  gfx::Point converted_location = location;
  delegate()->ConvertPointToTarget(current_target_, &converted_location);

  const int kTouchId = 2;
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, converted_location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  Dispatch(&press);

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, converted_location,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  Dispatch(&release);
}

void EventGenerator::GestureTapDownAndUp(const gfx::Point& location) {
  const int kTouchId = 3;
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  Dispatch(&press);

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, location,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(1000),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  Dispatch(&release);
}

base::TimeDelta EventGenerator::CalculateScrollDurationForFlingVelocity(
    const gfx::Point& start,
    const gfx::Point& end,
    float velocity,
    int steps) {
  const float kGestureDistance = (start - end).Length();
  const float kFlingStepDelay = (kGestureDistance / velocity) / steps * 1000000;
  return base::TimeDelta::FromMicroseconds(kFlingStepDelay);
}

void EventGenerator::GestureScrollSequence(const gfx::Point& start,
                                           const gfx::Point& end,
                                           const base::TimeDelta& step_delay,
                                           int steps) {
  GestureScrollSequenceWithCallback(start, end, step_delay, steps,
                                    base::BindRepeating(&DummyCallback));
}

void EventGenerator::GestureScrollSequenceWithCallback(
    const gfx::Point& start,
    const gfx::Point& end,
    const base::TimeDelta& step_delay,
    int steps,
    const ScrollStepCallback& callback) {
  const int kTouchId = 5;
  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, start, timestamp,
                       PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      /* pointer_id*/ kTouchId,
                                      /* radius_x */ 5.0f,
                                      /* radius_y */ 5.0f,
                                      /* force */ 1.0f));
  Dispatch(&press);

  callback.Run(ui::ET_GESTURE_SCROLL_BEGIN, gfx::Vector2dF());

  float dx = static_cast<float>(end.x() - start.x()) / steps;
  float dy = static_cast<float>(end.y() - start.y()) / steps;
  gfx::PointF location(start);
  for (int i = 0; i < steps; ++i) {
    location.Offset(dx, dy);
    timestamp += step_delay;
    ui::TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(), timestamp,
                        PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                       /* pointer_id*/ kTouchId,
                                       /* radius_x */ 5.0f,
                                       /* radius_y */ 5.0f,
                                       /* force */ 1.0f));
    move.set_location_f(location);
    move.set_root_location_f(location);
    Dispatch(&move);
    callback.Run(ui::ET_GESTURE_SCROLL_UPDATE, gfx::Vector2dF(dx, dy));
  }

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, end, timestamp,
      PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                     /* pointer_id*/ kTouchId,
                     /* radius_x */ 5.0f,
                     /* radius_y */ 5.0f,
                     /* force */ 1.0f));
  Dispatch(&release);

  callback.Run(ui::ET_GESTURE_SCROLL_END, gfx::Vector2dF());
}

void EventGenerator::GestureMultiFingerScrollWithDelays(
    int count,
    const gfx::Point start[],
    const gfx::Vector2d delta[],
    const int delay_adding_finger_ms[],
    const int delay_releasing_finger_ms[],
    int event_separation_time_ms,
    int steps) {
  const int kMaxTouchPoints = 10;
  CHECK_LE(count, kMaxTouchPoints);
  CHECK_GT(steps, 0);

  gfx::Point points[kMaxTouchPoints];
  gfx::Vector2d delta_per_step[kMaxTouchPoints];
  for (int i = 0; i < count; ++i) {
    points[i] = start[i];
    delta_per_step[i].set_x(delta[i].x() / steps);
    delta_per_step[i].set_y(delta[i].y() / steps);
  }

  base::TimeTicks press_time_first = ui::EventTimeForNow();
  base::TimeTicks press_time[kMaxTouchPoints];
  base::TimeTicks release_time[kMaxTouchPoints];
  bool pressed[kMaxTouchPoints];
  for (int i = 0; i < count; ++i) {
    pressed[i] = false;
    press_time[i] = press_time_first +
        base::TimeDelta::FromMilliseconds(delay_adding_finger_ms[i]);
    release_time[i] = press_time_first + base::TimeDelta::FromMilliseconds(
                                             delay_releasing_finger_ms[i]);
    DCHECK_LE(press_time[i], release_time[i]);
  }

  for (int step = 0; step < steps; ++step) {
    base::TimeTicks move_time =
        press_time_first +
        base::TimeDelta::FromMilliseconds(event_separation_time_ms * step);

    for (int i = 0; i < count; ++i) {
      if (!pressed[i] && move_time >= press_time[i]) {
        ui::TouchEvent press(
            ui::ET_TOUCH_PRESSED, points[i], press_time[i],
            ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, i));
        Dispatch(&press);
        pressed[i] = true;
      }
    }

    // All touch release events should occur at the end if
    // |event_separation_time_ms| is 0.
    for (int i = 0; i < count && event_separation_time_ms > 0; ++i) {
      if (pressed[i] && move_time >= release_time[i]) {
        ui::TouchEvent release(
            ui::ET_TOUCH_RELEASED, points[i], release_time[i],
            ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, i));
        Dispatch(&release);
        pressed[i] = false;
      }
    }

    for (int i = 0; i < count; ++i) {
      points[i] += delta_per_step[i];
      if (pressed[i]) {
        ui::TouchEvent move(
            ui::ET_TOUCH_MOVED, points[i], move_time,
            ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, i));
        Dispatch(&move);
      }
    }
  }

  base::TimeTicks default_release_time =
      press_time_first +
      base::TimeDelta::FromMilliseconds(event_separation_time_ms * steps);
  // Ensures that all pressed fingers are released in the end.
  for (int i = 0; i < count; ++i) {
    if (pressed[i]) {
      ui::TouchEvent release(
          ui::ET_TOUCH_RELEASED, points[i], default_release_time,
          ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, i));
      Dispatch(&release);
      pressed[i] = false;
    }
  }
}

void EventGenerator::GestureMultiFingerScrollWithDelays(
    int count,
    const gfx::Point start[],
    const int delay_adding_finger_ms[],
    int event_separation_time_ms,
    int steps,
    int move_x,
    int move_y) {
  const int kMaxTouchPoints = 10;
  int delay_releasing_finger_ms[kMaxTouchPoints];
  gfx::Vector2d delta[kMaxTouchPoints];
  for (int i = 0; i < kMaxTouchPoints; ++i) {
    delay_releasing_finger_ms[i] = event_separation_time_ms * steps;
    delta[i].set_x(move_x);
    delta[i].set_y(move_y);
  }
  GestureMultiFingerScrollWithDelays(
      count, start, delta, delay_adding_finger_ms, delay_releasing_finger_ms,
      event_separation_time_ms, steps);
}

void EventGenerator::GestureMultiFingerScroll(int count,
                                              const gfx::Point start[],
                                              int event_separation_time_ms,
                                              int steps,
                                              int move_x,
                                              int move_y) {
  const int kMaxTouchPoints = 10;
  int delays[kMaxTouchPoints] = {0};
  GestureMultiFingerScrollWithDelays(
      count, start, delays, event_separation_time_ms, steps, move_x, move_y);
}

void EventGenerator::ScrollSequence(const gfx::Point& start,
                                    const base::TimeDelta& step_delay,
                                    float x_offset,
                                    float y_offset,
                                    int steps,
                                    int num_fingers) {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL,
                               start,
                               timestamp,
                               0,
                               0, 0,
                               0, 0,
                               num_fingers);
  Dispatch(&fling_cancel);

  float dx = x_offset / steps;
  float dy = y_offset / steps;
  for (int i = 0; i < steps; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL,
                         start,
                         timestamp,
                         0,
                         dx, dy,
                         dx, dy,
                         num_fingers);
    Dispatch(&move);
  }

  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START,
                              start,
                              timestamp,
                              0,
                              x_offset, y_offset,
                              x_offset, y_offset,
                              num_fingers);
  Dispatch(&fling_start);
}

void EventGenerator::GenerateTrackpadRest() {
  int num_fingers = 2;
  ui::ScrollEvent scroll(ui::ET_SCROLL, current_screen_location_,
                         ui::EventTimeForNow(), 0, 0, 0, 0, 0, num_fingers,
                         EventMomentumPhase::MAY_BEGIN);
  Dispatch(&scroll);
}

void EventGenerator::CancelTrackpadRest() {
  int num_fingers = 2;
  ui::ScrollEvent scroll(ui::ET_SCROLL, current_screen_location_,
                         ui::EventTimeForNow(), 0, 0, 0, 0, 0, num_fingers,
                         EventMomentumPhase::END);
  Dispatch(&scroll);
}

void EventGenerator::PressKey(ui::KeyboardCode key_code, int flags) {
  DispatchKeyEvent(true, key_code, flags);
}

void EventGenerator::ReleaseKey(ui::KeyboardCode key_code, int flags) {
  DispatchKeyEvent(false, key_code, flags);
}

void EventGenerator::Dispatch(ui::Event* event) {
  if (event->IsTouchEvent()) {
    ui::TouchEvent* touch_event = static_cast<ui::TouchEvent*>(event);
    touch_pointer_details_.id = touch_event->pointer_details().id;
    touch_event->SetPointerDetailsForTest(touch_pointer_details_);
  }

  if (!event->handled()) {
    ui::EventSource* event_source = delegate()->GetEventSource(current_target_);
    ui::EventSourceTestApi event_source_test(event_source);
    ui::EventDispatchDetails details = event_source_test.SendEventToSink(event);
    if (details.dispatcher_destroyed)
      current_target_ = nullptr;
  }
}

void EventGenerator::Init(gfx::NativeWindow root_window,
                          gfx::NativeWindow window_context) {
  tick_clock_ = std::make_unique<TestTickClock>();
  ui::SetEventTickClockForTesting(tick_clock_.get());
  if (!delegate_) {
    DCHECK(g_event_generator_delegate_factory);
    delegate_ = g_event_generator_delegate_factory.Run(this, root_window,
                                                       window_context);
  }
  if (window_context)
    current_screen_location_ = delegate()->CenterOfWindow(window_context);
  else if (root_window)
    delegate()->ConvertPointFromWindow(root_window, &current_screen_location_);
  current_target_ = delegate()->GetTargetAt(current_screen_location_);
  touch_pointer_details_ =
      PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                     /* pointer_id*/ 0,
                     /* radius_x */ 1.0f,
                     /* radius_y */ 1.0f,
                     /* force */ 0.0f);
}

void EventGenerator::DispatchKeyEvent(bool is_press,
                                      ui::KeyboardCode key_code,
                                      int flags) {
#if defined(OS_WIN)
  UINT key_press = WM_KEYDOWN;
  uint16_t character = ui::DomCodeToUsLayoutCharacter(
      ui::UsLayoutKeyboardCodeToDomCode(key_code), flags);
  if (is_press && character) {
    MSG native_event = { NULL, WM_KEYDOWN, key_code, 0 };
    ui::KeyEvent keyev(native_event, flags);
    Dispatch(&keyev);
    // On Windows, WM_KEYDOWN event is followed by WM_CHAR with a character
    // if the key event cooresponds to a real character.
    key_press = WM_CHAR;
    key_code = static_cast<ui::KeyboardCode>(character);
  }
  MSG native_event =
      { NULL, (is_press ? key_press : WM_KEYUP), key_code, 0 };
  native_event.time =
      (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  ui::KeyEvent keyev(native_event, flags);
#elif defined(USE_X11)
  ui::ScopedXI2Event xevent;
  xevent.InitKeyEvent(is_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED,
                      key_code,
                      flags);
  static_cast<XEvent*>(xevent)->xkey.time =
      (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  ui::KeyEvent keyev(xevent);
#else
  ui::EventType type = is_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
  ui::KeyEvent keyev(type, key_code, flags);
#endif  // OS_WIN
  Dispatch(&keyev);
}

void EventGenerator::UpdateCurrentDispatcher(const gfx::Point& point) {
  current_target_ = delegate()->GetTargetAt(point);
}

void EventGenerator::PressButton(int flag) {
  if (!(flags_ & flag)) {
    flags_ |= flag;
    grab_ = (flags_ & kAllButtonMask) != 0;
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::ET_MOUSE_PRESSED, location, location,
                           ui::EventTimeForNow(), flags_, flag);
    Dispatch(&mouseev);
  }
}

void EventGenerator::ReleaseButton(int flag) {
  if (flags_ & flag) {
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::ET_MOUSE_RELEASED, location, location,
                           ui::EventTimeForNow(), flags_, flag);
    Dispatch(&mouseev);
    flags_ ^= flag;
  }
  grab_ = (flags_ & kAllButtonMask) != 0;
}

gfx::Point EventGenerator::GetLocationInCurrentRoot() const {
  gfx::Point p = current_screen_location_;
  delegate()->ConvertPointToTarget(current_target_, &p);
  return p;
}

gfx::Point EventGenerator::CenterOfWindow(const EventTarget* window) const {
  return delegate()->CenterOfTarget(window);
}

}  // namespace test
}  // namespace ui
