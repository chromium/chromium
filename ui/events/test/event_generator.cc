// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/test/event_generator.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif

namespace ui {
namespace test {

// A TickClock that advances time by one millisecond on each call to NowTicks().
class TestTickClock : public base::TickClock {
 public:
  TestTickClock() = default;

  TestTickClock(const TestTickClock&) = delete;
  TestTickClock& operator=(const TestTickClock&) = delete;

  // Returns a tick count that is 1ms later than the previous call to
  // `NowTicks`, plus possible additional time from calls to `Advance`. Starts
  // at 1ms.
  base::TimeTicks NowTicks() const override {
    static constexpr base::TimeDelta kOneMillisecond = base::Milliseconds(1);
    return ticks_ += kOneMillisecond;
  }

  // Advances the clock by `delta`. `delta` should be non-negative.
  void Advance(const base::TimeDelta& delta) {
    DCHECK(delta >= base::TimeDelta());
    ticks_ += delta;
  }

 private:
  mutable base::TimeTicks ticks_;
};

namespace {

void DummyCallback(EventType, const gfx::Vector2dF&) {}

ui::TouchEvent CreateTestTouchEvent(ui::EventType type,
                                    const gfx::Point& root_location,
                                    int touch_id,
                                    int flags,
                                    base::TimeTicks timestamp) {
  return ui::TouchEvent(type, root_location, timestamp,
                        ui::PointerDetails(ui::EventPointerType::kTouch,
                                           /* pointer_id*/ touch_id,
                                           /* radius_x */ 1.0f,
                                           /* radius_y */ 1.0f,
                                           /* force */ 0.0f),
                        flags);
}

const int kAllButtonMask = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON;

EventGeneratorDelegate::FactoryFunction g_event_generator_delegate_factory;

bool g_event_generator_allowed = true;

struct ModifierKey {
  KeyboardCode key;
  EventFlags flag;
};

constexpr ModifierKey kModifierKeys[] = {
    {VKEY_SHIFT, EF_SHIFT_DOWN},
    {VKEY_CONTROL, EF_CONTROL_DOWN},
    {VKEY_MENU, EF_ALT_DOWN},
    {VKEY_LWIN, EF_COMMAND_DOWN},
};

}  // namespace

// static
void EventGeneratorDelegate::SetFactoryFunction(FactoryFunction factory) {
  g_event_generator_delegate_factory = std::move(factory);
}

// static
void EventGenerator::BanEventGenerator() {
  g_event_generator_allowed = false;
}

EventGenerator::EventGenerator(std::unique_ptr<EventGeneratorDelegate> delegate)
    : delegate_(std::move(delegate)) {
  Init(gfx::NativeWindow(), gfx::NativeWindow());
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window) {
  Init(root_window, gfx::NativeWindow());
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window,
                               const gfx::Point& point)
    : current_screen_location_(point) {
  Init(root_window, gfx::NativeWindow());
}

EventGenerator::EventGenerator(gfx::NativeWindow root_window,
                               gfx::NativeWindow target_window) {
  Init(root_window, target_window);
}

EventGenerator::~EventGenerator() {
  ui::SetEventTickClockForTesting(nullptr);
}

void EventGenerator::SetTargetWindow(gfx::NativeWindow target_window) {
  delegate()->SetTargetWindow(target_window);
  SetCurrentScreenLocation(delegate()->CenterOfWindow(target_window));
}

void EventGenerator::PressButton(int flag) {
  if (!(flags_ & flag)) {
    flags_ |= flag;
    grab_ = (flags_ & kAllButtonMask) != 0;
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::EventType::kMousePressed, location, location,
                           ui::EventTimeForNow(), flags_, flag);
    mouseev.set_source_device_id(mouse_source_device_id_);
    Dispatch(&mouseev);
  }
}

void EventGenerator::ReleaseButton(int flag) {
  if (flags_ & flag) {
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::EventType::kMouseReleased, location, location,
                           ui::EventTimeForNow(), flags_, flag);
    mouseev.set_source_device_id(mouse_source_device_id_);
    Dispatch(&mouseev);
    flags_ ^= flag;
  }
  grab_ = (flags_ & kAllButtonMask) != 0;
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
  wheelev.set_source_device_id(mouse_source_device_id_);
  Dispatch(&wheelev);
}

void EventGenerator::SendMouseEnter() {
  gfx::Point enter_location(current_screen_location_);
  delegate()->ConvertPointToTarget(current_target_, &enter_location);
  ui::MouseEvent mouseev(ui::EventType::kMouseEntered, enter_location,
                         enter_location, ui::EventTimeForNow(), flags_, 0);
  mouseev.set_source_device_id(mouse_source_device_id_);
  Dispatch(&mouseev);
}

void EventGenerator::SendMouseExit() {
  gfx::Point exit_location(current_screen_location_);
  delegate()->ConvertPointToTarget(current_target_, &exit_location);
  ui::MouseEvent mouseev(ui::EventType::kMouseExited, exit_location,
                         exit_location, ui::EventTimeForNow(), flags_, 0);
  mouseev.set_source_device_id(mouse_source_device_id_);
  Dispatch(&mouseev);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void EventGenerator::MoveMouseToWithNative(const gfx::Point& point_in_host,
                                           const gfx::Point& point_for_native) {
  // Ozone uses the location in native event as a system location.
  // Create a fake event with the point in host, which will be passed
  // to the non native event, then update the native event with the native
  // (root) one.
  std::unique_ptr<ui::MouseEvent> native_event(
      new ui::MouseEvent(ui::EventType::kMouseMoved, point_in_host,
                         point_in_host, ui::EventTimeForNow(), flags_, 0));
  ui::MouseEvent mouseev(native_event.get());
  mouseev.set_source_device_id(mouse_source_device_id_);
  native_event->set_location(point_for_native);
  Dispatch(&mouseev);

  SetCurrentScreenLocation(point_in_host);
  delegate()->ConvertPointFromHost(current_target_, &current_screen_location_);
}
#endif

void EventGenerator::MoveMouseToInHost(const gfx::Point& point_in_host) {
  const ui::EventType event_type = (flags_ & ui::EF_LEFT_MOUSE_BUTTON)
                                       ? ui::EventType::kMouseDragged
                                       : ui::EventType::kMouseMoved;
  ui::MouseEvent mouseev(event_type, point_in_host, point_in_host,
                         ui::EventTimeForNow(), flags_, 0);
  mouseev.set_source_device_id(mouse_source_device_id_);
  Dispatch(&mouseev);

  SetCurrentScreenLocation(point_in_host);
  delegate()->ConvertPointFromHost(current_target_, &current_screen_location_);
}

void EventGenerator::MoveMouseTo(const gfx::Point& point_in_screen,
                                 int count) {
  DCHECK_GT(count, 0);
  const ui::EventType event_type = (flags_ & ui::EF_LEFT_MOUSE_BUTTON)
                                       ? ui::EventType::kMouseDragged
                                       : ui::EventType::kMouseMoved;

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
    mouseev.set_source_device_id(mouse_source_device_id_);
    Dispatch(&mouseev);
  }
  SetCurrentScreenLocation(point_in_screen);
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
  touch_pointer_details_.pointer_type = ui::EventPointerType::kPen;
}

void EventGenerator::ExitPenPointerMode() {
  touch_pointer_details_.pointer_type = ui::EventPointerType::kTouch;
}

void EventGenerator::SetTouchRadius(float x, float y) {
  touch_pointer_details_.radius_x = x;
  touch_pointer_details_.radius_y = y;
}

void EventGenerator::SetTouchTilt(float x, float y) {
  touch_pointer_details_.tilt_x = x;
  touch_pointer_details_.tilt_y = y;
}

void EventGenerator::PressTouch(
    const std::optional<gfx::Point>& touch_location_in_screen) {
  PressTouchId(0, touch_location_in_screen);
}

void EventGenerator::PressTouchId(
    int touch_id,
    const std::optional<gfx::Point>& touch_location_in_screen) {
  if (touch_location_in_screen.has_value())
    SetCurrentScreenLocation(*touch_location_in_screen);
  ui::TouchEvent touchev = CreateTestTouchEvent(
      ui::EventType::kTouchPressed, GetLocationInCurrentRoot(), touch_id,
      flags_, ui::EventTimeForNow());
  Dispatch(&touchev);
}

void EventGenerator::MoveTouch(const gfx::Point& point) {
  MoveTouchId(point, 0);
}

void EventGenerator::MoveTouchId(const gfx::Point& point, int touch_id) {
  SetCurrentScreenLocation(point);
  ui::TouchEvent touchev = CreateTestTouchEvent(
      ui::EventType::kTouchMoved, GetLocationInCurrentRoot(), touch_id, flags_,
      ui::EventTimeForNow());
  Dispatch(&touchev);

  if (!grab_)
    UpdateCurrentDispatcher(point);
}

void EventGenerator::ReleaseTouch() {
  ReleaseTouchId(0);
}

void EventGenerator::ReleaseTouchId(int touch_id) {
  ui::TouchEvent touchev = CreateTestTouchEvent(
      ui::EventType::kTouchReleased, GetLocationInCurrentRoot(), touch_id,
      flags_, ui::EventTimeForNow());
  Dispatch(&touchev);
}

void EventGenerator::CancelTouch() {
  CancelTouchId(0);
}

void EventGenerator::CancelTouchId(int touch_id) {
  ui::TouchEvent touchev = CreateTestTouchEvent(
      ui::EventType::kTouchCancelled, GetLocationInCurrentRoot(), touch_id,
      flags_, ui::EventTimeForNow());
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
      ui::EventType::kTouchPressed, converted_location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  Dispatch(&press);

  ui::TouchEvent release(
      ui::EventType::kTouchReleased, converted_location,
      press.time_stamp() + base::Milliseconds(50),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  Dispatch(&release);
}

void EventGenerator::GestureTapDownAndUp(const gfx::Point& location) {
  UpdateCurrentDispatcher(location);
  gfx::Point converted_location = location;
  delegate()->ConvertPointToTarget(current_target_, &converted_location);

  const int kTouchId = 3;
  ui::TouchEvent press(
      ui::EventType::kTouchPressed, converted_location, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  Dispatch(&press);

  ui::TouchEvent release(
      ui::EventType::kTouchReleased, converted_location,
      press.time_stamp() + base::Milliseconds(1000),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  Dispatch(&release);
}

base::TimeDelta EventGenerator::CalculateScrollDurationForFlingVelocity(
    const gfx::Point& start,
    const gfx::Point& end,
    float velocity,
    int steps) {
  const float kGestureDistance = (start - end).Length();
  const float kFlingStepDelay = (kGestureDistance / velocity) / steps * 1000000;
  return base::Microseconds(kFlingStepDelay);
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
  UpdateCurrentDispatcher(start);
  const int kTouchId = 5;
  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::TouchEvent press(ui::EventType::kTouchPressed, start, timestamp,
                       PointerDetails(ui::EventPointerType::kTouch,
                                      /* pointer_id*/ kTouchId,
                                      /* radius_x */ 5.0f,
                                      /* radius_y */ 5.0f,
                                      /* force */ 1.0f));
  Dispatch(&press);

  callback.Run(ui::EventType::kGestureScrollBegin, gfx::Vector2dF());

  float dx = static_cast<float>(end.x() - start.x()) / steps;
  float dy = static_cast<float>(end.y() - start.y()) / steps;
  gfx::PointF location(start);
  for (int i = 0; i < steps; ++i) {
    location.Offset(dx, dy);
    timestamp += step_delay;
    ui::TouchEvent move(ui::EventType::kTouchMoved, gfx::Point(), timestamp,
                        PointerDetails(ui::EventPointerType::kTouch,
                                       /* pointer_id*/ kTouchId,
                                       /* radius_x */ 5.0f,
                                       /* radius_y */ 5.0f,
                                       /* force */ 1.0f));
    move.set_location_f(location);
    move.set_root_location_f(location);
    Dispatch(&move);
    callback.Run(ui::EventType::kGestureScrollUpdate, gfx::Vector2dF(dx, dy));
  }

  ui::TouchEvent release(ui::EventType::kTouchReleased, end, timestamp,
                         PointerDetails(ui::EventPointerType::kTouch,
                                        /* pointer_id*/ kTouchId,
                                        /* radius_x */ 5.0f,
                                        /* radius_y */ 5.0f,
                                        /* force */ 1.0f));
  Dispatch(&release);

  callback.Run(ui::EventType::kGestureScrollEnd, gfx::Vector2dF());
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
    press_time[i] =
        press_time_first + base::Milliseconds(delay_adding_finger_ms[i]);
    release_time[i] =
        press_time_first + base::Milliseconds(delay_releasing_finger_ms[i]);
    DCHECK_LE(press_time[i], release_time[i]);
  }

  for (int step = 0; step < steps; ++step) {
    base::TimeTicks move_time =
        press_time_first + base::Milliseconds(event_separation_time_ms * step);

    for (int i = 0; i < count; ++i) {
      if (!pressed[i] && move_time >= press_time[i]) {
        ui::TouchEvent press(
            ui::EventType::kTouchPressed, points[i], press_time[i],
            ui::PointerDetails(ui::EventPointerType::kTouch, i));
        Dispatch(&press);
        pressed[i] = true;
      }
    }

    // All touch release events should occur at the end if
    // |event_separation_time_ms| is 0.
    for (int i = 0; i < count && event_separation_time_ms > 0; ++i) {
      if (pressed[i] && move_time >= release_time[i]) {
        ui::TouchEvent release(
            ui::EventType::kTouchReleased, points[i], release_time[i],
            ui::PointerDetails(ui::EventPointerType::kTouch, i));
        Dispatch(&release);
        pressed[i] = false;
      }
    }

    for (int i = 0; i < count; ++i) {
      points[i] += delta_per_step[i];
      if (pressed[i]) {
        ui::TouchEvent move(
            ui::EventType::kTouchMoved, points[i], move_time,
            ui::PointerDetails(ui::EventPointerType::kTouch, i));
        Dispatch(&move);
      }
    }
  }

  base::TimeTicks default_release_time =
      press_time_first + base::Milliseconds(event_separation_time_ms * steps);
  // Ensures that all pressed fingers are released in the end.
  for (int i = 0; i < count; ++i) {
    if (pressed[i]) {
      ui::TouchEvent release(
          ui::EventType::kTouchReleased, points[i], default_release_time,
          ui::PointerDetails(ui::EventPointerType::kTouch, i));
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
                                    int num_fingers,
                                    ScrollSequenceType end_state) {
  UpdateCurrentDispatcher(start);

  base::TimeTicks timestamp = ui::EventTimeForNow();
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel, start,
                               timestamp, 0, 0, 0, 0, 0, num_fingers);
  Dispatch(&fling_cancel);

  float dx = x_offset / steps;
  float dy = y_offset / steps;
  for (int i = 0; i < steps; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::EventType::kScroll, start, timestamp, 0, dx, dy,
                         dx, dy, num_fingers);
    Dispatch(&move);
  }

  // End the scroll sequence early if we want to end with the fingers rested on
  // the trackpad.
  if (end_state == ScrollSequenceType::ScrollOnly) {
    return;
  }

  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart, start,
                              timestamp, 0, x_offset, y_offset, x_offset,
                              y_offset, num_fingers);
  Dispatch(&fling_start);
}

void EventGenerator::GenerateTrackpadRest() {
  int num_fingers = 2;
  ui::ScrollEvent scroll(ui::EventType::kScroll, current_screen_location_,
                         ui::EventTimeForNow(), 0, 0, 0, 0, 0, num_fingers,
                         EventMomentumPhase::MAY_BEGIN);
  Dispatch(&scroll);
}

void EventGenerator::CancelTrackpadRest() {
  int num_fingers = 2;
  ui::ScrollEvent scroll(ui::EventType::kScroll, current_screen_location_,
                         ui::EventTimeForNow(), 0, 0, 0, 0, 0, num_fingers,
                         EventMomentumPhase::END);
  Dispatch(&scroll);
}

void EventGenerator::PressKey(ui::KeyboardCode key_code,
                              int flags,
                              int source_device_id) {
  DispatchKeyEvent(true, key_code, flags, source_device_id);
}

void EventGenerator::ReleaseKey(ui::KeyboardCode key_code,
                                int flags,
                                int source_device_id) {
  DispatchKeyEvent(false, key_code, flags, source_device_id);
}

void EventGenerator::PressAndReleaseKey(KeyboardCode key_code,
                                        int flags,
                                        int source_device_id) {
  PressKey(key_code, flags, source_device_id);
  ReleaseKey(key_code, flags, source_device_id);
}

void EventGenerator::PressModifierKeys(int flags, int source_device_id) {
  EventFlags current_flags = 0;
  for (const auto& modifier_key : kModifierKeys) {
    if (flags & modifier_key.flag) {
      current_flags |= modifier_key.flag;
      PressKey(modifier_key.key, current_flags, source_device_id);
    }
  }
}

void EventGenerator::ReleaseModifierKeys(int flags, int source_device_id) {
  EventFlags current_flags = flags;
  for (const auto& modifier_key : base::Reversed(kModifierKeys)) {
    if (flags & modifier_key.flag) {
      current_flags &= ~modifier_key.flag;
      ReleaseKey(modifier_key.key, current_flags, source_device_id);
    }
  }
}

void EventGenerator::PressKeyAndModifierKeys(KeyboardCode key_code,
                                             int flags,
                                             int source_device_id) {
  PressModifierKeys(flags, source_device_id);
  PressKey(key_code, flags, source_device_id);
}

void EventGenerator::ReleaseKeyAndModifierKeys(KeyboardCode key_code,
                                               int flags,
                                               int source_device_id) {
  ReleaseKey(key_code, flags, source_device_id);
  ReleaseModifierKeys(flags, source_device_id);
}

void EventGenerator::PressAndReleaseKeyAndModifierKeys(KeyboardCode key_code,
                                                       int flags,
                                                       int source_device_id) {
  PressKeyAndModifierKeys(key_code, flags, source_device_id);
  ReleaseKeyAndModifierKeys(key_code, flags, source_device_id);
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

void EventGenerator::AdvanceClock(const base::TimeDelta& delta) {
  tick_clock_->Advance(delta);
}

void EventGenerator::Init(gfx::NativeWindow root_window,
                          gfx::NativeWindow target_window) {
  CHECK(g_event_generator_allowed)
      << "EventGenerator is not allowed in this test suite. Please use "
         "functions from ui_controls.h instead.";

  tick_clock_ = std::make_unique<TestTickClock>();
  ui::SetEventTickClockForTesting(tick_clock_.get());
  if (!delegate_) {
    DCHECK(g_event_generator_delegate_factory);
    delegate_ = g_event_generator_delegate_factory.Run(this, root_window,
                                                       target_window);
  }
  if (target_window)
    SetCurrentScreenLocation(delegate()->CenterOfWindow(target_window));
  else if (root_window)
    delegate()->ConvertPointFromWindow(root_window, &current_screen_location_);
  current_target_ = delegate()->GetTargetAt(current_screen_location_);
  touch_pointer_details_ = PointerDetails(ui::EventPointerType::kTouch,
                                          /* pointer_id*/ 0,
                                          /* radius_x */ 1.0f,
                                          /* radius_y */ 1.0f,
                                          /* force */ 0.0f);
}

void EventGenerator::DispatchKeyEvent(bool is_press,
                                      ui::KeyboardCode key_code,
                                      int flags,
                                      int source_device_id) {
#if BUILDFLAG(IS_WIN)
  UINT key_press = WM_KEYDOWN;
  uint16_t character = ui::DomCodeToUsLayoutCharacter(
      ui::UsLayoutKeyboardCodeToDomCode(key_code), flags);
  if (is_press && character) {
    CHROME_MSG native_event = {NULL, WM_KEYDOWN, static_cast<UINT>(key_code),
                               0};
    native_event.time =
        (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() &
        UINT32_MAX;
    ui::KeyEvent keyev(native_event, flags);
    Dispatch(&keyev);
    // On Windows, WM_KEYDOWN event is followed by WM_CHAR with a character
    // if the key event corresponds to a real character.
    key_press = WM_CHAR;
    key_code = static_cast<ui::KeyboardCode>(character);
  }
  CHROME_MSG native_event = {NULL, (is_press ? key_press : WM_KEYUP),
                             static_cast<UINT>(key_code), 0};
  native_event.time =
      (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  ui::KeyEvent keyev(native_event, flags);
#else
  ui::EventType type =
      is_press ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased;
  ui::KeyEvent keyev(type, key_code, flags);
#if BUILDFLAG(IS_OZONE)
  if (is_press) {
    // Set a property as if this is a key event not consumed by IME.
    // Ozone/X11+GTK IME works so already. Ozone/wayland IME relies on this
    // flag to work properly.
    SetKeyboardImeFlags(&keyev, kPropertyKeyboardImeIgnoredFlag);
  }
#endif  // BUILDFLAG(IS_OZONE)
#endif  // BUILDFLAG(IS_WIN)
  keyev.set_source_device_id(source_device_id);
  Dispatch(&keyev);
}

void EventGenerator::SetCurrentScreenLocation(const gfx::Point& point) {
  current_screen_location_ = point;
  UpdateCurrentDispatcher(point);
}

void EventGenerator::UpdateCurrentDispatcher(const gfx::Point& point) {
  current_target_ = delegate()->GetTargetAt(point);
}

gfx::Point EventGenerator::GetLocationInCurrentRoot() const {
  gfx::Point p = current_screen_location_;
  delegate()->ConvertPointToTarget(current_target_, &p);
  return p;
}

gfx::Point EventGenerator::CenterOfWindow(const EventTarget* window) const {
  return delegate()->CenterOfTarget(window);
}

void EmulateFullKeyPressReleaseSequence(test::EventGenerator* generator,
                                        KeyboardCode key,
                                        bool control,
                                        bool shift,
                                        bool alt,
                                        bool command) {
  int flags = ui::EF_FINAL;
  if (control) {
    flags |= ui::EF_CONTROL_DOWN;
    generator->PressKey(ui::VKEY_CONTROL, flags);
  }
  if (shift) {
    flags |= ui::EF_SHIFT_DOWN;
    generator->PressKey(ui::VKEY_SHIFT, flags);
  }
  if (alt) {
    flags |= ui::EF_ALT_DOWN;
    generator->PressKey(ui::VKEY_MENU, flags);
  }
  if (command) {
    flags |= ui::EF_COMMAND_DOWN;
    generator->PressKey(ui::VKEY_COMMAND, flags);
  }

  generator->PressAndReleaseKey(key, flags);

  if (command) {
    flags &= ~ui::EF_COMMAND_DOWN;
    generator->ReleaseKey(ui::VKEY_COMMAND, flags);
  }
  if (alt) {
    flags &= ~ui::EF_ALT_DOWN;
    generator->ReleaseKey(ui::VKEY_MENU, flags);
  }
  if (shift) {
    flags &= ~ui::EF_SHIFT_DOWN;
    generator->ReleaseKey(ui::VKEY_SHIFT, flags);
  }
  if (control) {
    flags &= ~ui::EF_CONTROL_DOWN;
    generator->ReleaseKey(ui::VKEY_CONTROL, flags);
  }
}

}  // namespace test
}  // namespace ui
