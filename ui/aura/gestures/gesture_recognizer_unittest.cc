// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_provider.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
namespace test {

namespace {

std::string WindowIDAsString(ui::GestureConsumer* consumer) {
  return consumer ? base::NumberToString(static_cast<Window*>(consumer)->id())
                  : "?";
}

#define EXPECT_0_EVENTS(events) \
    EXPECT_EQ(0u, events.size())

#define EXPECT_1_EVENT(events, e0) \
    EXPECT_EQ(1u, events.size()); \
    EXPECT_EQ(e0, events[0])

#define EXPECT_2_EVENTS(events, e0, e1) \
    EXPECT_EQ(2u, events.size()); \
    EXPECT_EQ(e0, events[0]); \
    EXPECT_EQ(e1, events[1])

#define EXPECT_3_EVENTS(events, e0, e1, e2) \
    EXPECT_EQ(3u, events.size()); \
    EXPECT_EQ(e0, events[0]); \
    EXPECT_EQ(e1, events[1]); \
    EXPECT_EQ(e2, events[2])

#define EXPECT_4_EVENTS(events, e0, e1, e2, e3) \
    EXPECT_EQ(4u, events.size()); \
    EXPECT_EQ(e0, events[0]); \
    EXPECT_EQ(e1, events[1]); \
    EXPECT_EQ(e2, events[2]); \
    EXPECT_EQ(e3, events[3])

// A delegate that keeps track of gesture events.
class GestureEventConsumeDelegate : public TestWindowDelegate {
 public:
  GestureEventConsumeDelegate()
      : tap_(false),
        tap_down_(false),
        tap_cancel_(false),
        begin_(false),
        end_(false),
        scroll_begin_(false),
        scroll_update_(false),
        scroll_end_(false),
        pinch_begin_(false),
        pinch_update_(false),
        pinch_end_(false),
        long_press_(false),
        fling_(false),
        two_finger_tap_(false),
        show_press_(false),
        swipe_left_(false),
        swipe_right_(false),
        swipe_up_(false),
        swipe_down_(false),
        scroll_x_(0),
        scroll_y_(0),
        scroll_velocity_x_(0),
        scroll_velocity_y_(0),
        velocity_x_(0),
        velocity_y_(0),
        scroll_x_hint_(0),
        scroll_y_hint_(0),
        tap_count_(0),
        flags_(0),
        wait_until_event_(ui::ET_UNKNOWN) {}

  ~GestureEventConsumeDelegate() override {}

  void Reset() {
    events_.clear();
    tap_ = false;
    tap_down_ = false;
    tap_cancel_ = false;
    begin_ = false;
    end_ = false;
    scroll_begin_ = false;
    scroll_update_ = false;
    scroll_end_ = false;
    pinch_begin_ = false;
    pinch_update_ = false;
    pinch_end_ = false;
    long_press_ = false;
    fling_ = false;
    two_finger_tap_ = false;
    show_press_ = false;
    swipe_left_ = false;
    swipe_right_ = false;
    swipe_up_ = false;
    swipe_down_ = false;

    scroll_begin_position_.SetPoint(0, 0);
    tap_location_.SetPoint(0, 0);
    gesture_end_location_.SetPoint(0, 0);

    scroll_x_ = 0;
    scroll_y_ = 0;
    scroll_velocity_x_ = 0;
    scroll_velocity_y_ = 0;
    velocity_x_ = 0;
    velocity_y_ = 0;
    scroll_x_hint_ = 0;
    scroll_y_hint_ = 0;
    tap_count_ = 0;
    scale_ = 0;
    flags_ = 0;
  }

  const std::vector<ui::EventType>& events() const { return events_; }

  bool tap() const { return tap_; }
  bool tap_down() const { return tap_down_; }
  bool tap_cancel() const { return tap_cancel_; }
  bool begin() const { return begin_; }
  bool end() const { return end_; }
  bool scroll_begin() const { return scroll_begin_; }
  bool scroll_update() const { return scroll_update_; }
  bool scroll_end() const { return scroll_end_; }
  bool pinch_begin() const { return pinch_begin_; }
  bool pinch_update() const { return pinch_update_; }
  bool pinch_end() const { return pinch_end_; }
  bool long_press() const { return long_press_; }
  bool long_tap() const { return long_tap_; }
  bool fling() const { return fling_; }
  bool two_finger_tap() const { return two_finger_tap_; }
  bool show_press() const { return show_press_; }
  bool swipe_left() const { return swipe_left_; }
  bool swipe_right() const { return swipe_right_; }
  bool swipe_up() const { return swipe_up_; }
  bool swipe_down() const { return swipe_down_; }

  const gfx::Point& scroll_begin_position() const {
    return scroll_begin_position_;
  }

  const gfx::Point& tap_location() const {
    return tap_location_;
  }

  const gfx::Point& gesture_end_location() const {
    return gesture_end_location_;
  }

  float scroll_x() const { return scroll_x_; }
  float scroll_y() const { return scroll_y_; }
  float scroll_velocity_x() const { return scroll_velocity_x_; }
  float scroll_velocity_y() const { return scroll_velocity_y_; }
  float velocity_x() const { return velocity_x_; }
  float velocity_y() const { return velocity_y_; }
  float scroll_x_hint() const { return scroll_x_hint_; }
  float scroll_y_hint() const { return scroll_y_hint_; }
  float scale() const { return scale_; }
  const gfx::Rect& bounding_box() const { return bounding_box_; }
  int tap_count() const { return tap_count_; }
  int flags() const { return flags_; }

  void WaitUntilReceivedGesture(ui::EventType type) {
    wait_until_event_ = type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void OnGestureEvent(ui::GestureEvent* gesture) override {
    events_.push_back(gesture->type());
    bounding_box_ = gesture->details().bounding_box();
    flags_ = gesture->flags();
    switch (gesture->type()) {
      case ui::ET_GESTURE_TAP:
        tap_location_ = gesture->location();
        tap_count_ = gesture->details().tap_count();
        tap_ = true;
        break;
      case ui::ET_GESTURE_TAP_DOWN:
        tap_down_ = true;
        break;
      case ui::ET_GESTURE_TAP_CANCEL:
        tap_cancel_ = true;
        break;
      case ui::ET_GESTURE_BEGIN:
        begin_ = true;
        break;
      case ui::ET_GESTURE_END:
        end_ = true;
        gesture_end_location_ = gesture->location();
        break;
      case ui::ET_GESTURE_SCROLL_BEGIN:
        scroll_begin_ = true;
        scroll_begin_position_ = gesture->location();
        scroll_x_hint_ = gesture->details().scroll_x_hint();
        scroll_y_hint_ = gesture->details().scroll_y_hint();
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        scroll_update_ = true;
        scroll_x_ += gesture->details().scroll_x();
        scroll_y_ += gesture->details().scroll_y();
        break;
      case ui::ET_GESTURE_SCROLL_END:
        EXPECT_TRUE(velocity_x_ == 0 && velocity_y_ == 0);
        scroll_end_ = true;
        break;
      case ui::ET_GESTURE_PINCH_BEGIN:
        pinch_begin_ = true;
        break;
      case ui::ET_GESTURE_PINCH_UPDATE:
        pinch_update_ = true;
        scale_ = gesture->details().scale();
        break;
      case ui::ET_GESTURE_PINCH_END:
        pinch_end_ = true;
        break;
      case ui::ET_GESTURE_LONG_PRESS:
        long_press_ = true;
        break;
      case ui::ET_GESTURE_LONG_TAP:
        long_tap_ = true;
        break;
      case ui::ET_SCROLL_FLING_START:
        EXPECT_TRUE(gesture->details().velocity_x() != 0 ||
                    gesture->details().velocity_y() != 0);
        EXPECT_FALSE(scroll_end_);
        fling_ = true;
        velocity_x_ = gesture->details().velocity_x();
        velocity_y_ = gesture->details().velocity_y();
        break;
      case ui::ET_GESTURE_TWO_FINGER_TAP:
        two_finger_tap_ = true;
        break;
      case ui::ET_GESTURE_SHOW_PRESS:
        show_press_ = true;
        break;
      case ui::ET_GESTURE_SWIPE:
        swipe_left_ = gesture->details().swipe_left();
        swipe_right_ = gesture->details().swipe_right();
        swipe_up_ = gesture->details().swipe_up();
        swipe_down_ = gesture->details().swipe_down();
        break;
      case ui::ET_SCROLL_FLING_CANCEL:
        // Only used in unified gesture detection.
        break;
      default:
        NOTREACHED();
    }
    if (wait_until_event_ == gesture->type() && run_loop_) {
      run_loop_->Quit();
      wait_until_event_ = ui::ET_UNKNOWN;
    }
    gesture->StopPropagation();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<ui::EventType> events_;

  bool tap_;
  bool tap_down_;
  bool tap_cancel_;
  bool begin_;
  bool end_;
  bool scroll_begin_;
  bool scroll_update_;
  bool scroll_end_;
  bool pinch_begin_;
  bool pinch_update_;
  bool pinch_end_;
  bool long_press_;
  bool long_tap_;
  bool fling_;
  bool two_finger_tap_;
  bool show_press_;
  bool swipe_left_;
  bool swipe_right_;
  bool swipe_up_;
  bool swipe_down_;

  gfx::Point scroll_begin_position_;
  gfx::Point tap_location_;
  gfx::Point gesture_end_location_;

  float scroll_x_;
  float scroll_y_;
  float scroll_velocity_x_;
  float scroll_velocity_y_;
  float velocity_x_;
  float velocity_y_;
  float scroll_x_hint_;
  float scroll_y_hint_;
  float scale_;
  gfx::Rect bounding_box_;
  int tap_count_;
  int flags_;

  ui::EventType wait_until_event_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventConsumeDelegate);
};

class QueueTouchEventDelegate : public GestureEventConsumeDelegate {
 public:
  explicit QueueTouchEventDelegate(WindowEventDispatcher* dispatcher)
      : window_(NULL),
        dispatcher_(dispatcher),
        synchronous_ack_for_next_event_(AckState::PENDING) {}

  ~QueueTouchEventDelegate() override {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    event->DisableSynchronousHandling();
    if (synchronous_ack_for_next_event_ != AckState::PENDING) {
      aura::Env::GetInstance()->gesture_recognizer()->AckTouchEvent(
          event->unique_event_id(),
          synchronous_ack_for_next_event_ == AckState::CONSUMED
              ? ui::ER_CONSUMED
              : ui::ER_UNHANDLED,
          false /* is_source_touch_event_set_non_blocking */, window_);
      synchronous_ack_for_next_event_ = AckState::PENDING;
    } else {
      sent_events_ids_.push_back(event->unique_event_id());
    }
  }

  void ReceivedAck() {
    ReceivedAckImpl(false);
  }

  void ReceivedAckPreventDefaulted() {
    ReceivedAckImpl(true);
  }

  void set_window(Window* w) { window_ = w; }
  void set_synchronous_ack_for_next_event(bool consumed) {
    DCHECK(synchronous_ack_for_next_event_ == AckState::PENDING);
    synchronous_ack_for_next_event_ =
        consumed ? AckState::CONSUMED : AckState::UNCONSUMED;
  }

 private:
  enum class AckState {
    PENDING,
    CONSUMED,
    UNCONSUMED,
  };

  void ReceivedAckImpl(bool prevent_defaulted) {
    DCHECK(!sent_events_ids_.empty());
    if (sent_events_ids_.empty())
      return;
    uint32_t sent_event_id = sent_events_ids_.front();
    sent_events_ids_.pop_front();
    dispatcher_->ProcessedTouchEvent(
        sent_event_id, window_,
        prevent_defaulted ? ui::ER_HANDLED : ui::ER_UNHANDLED,
        false /* is_source_touch_event_set_non_blocking */);
  }

  Window* window_;
  WindowEventDispatcher* dispatcher_;
  AckState synchronous_ack_for_next_event_;
  std::list<uint32_t> sent_events_ids_;

  DISALLOW_COPY_AND_ASSIGN(QueueTouchEventDelegate);
};

// A delegate that ignores gesture events but keeps track of [synthetic] mouse
// events.
class GestureEventSynthDelegate : public TestWindowDelegate {
 public:
  GestureEventSynthDelegate()
      : mouse_enter_(false),
        mouse_exit_(false),
        mouse_press_(false),
        mouse_release_(false),
        mouse_move_(false),
        double_click_(false) {
  }

  void Reset() {
    mouse_enter_ = false;
    mouse_exit_ = false;
    mouse_press_ = false;
    mouse_release_ = false;
    mouse_move_ = false;
    double_click_ = false;
  }

  bool mouse_enter() const { return mouse_enter_; }
  bool mouse_exit() const { return mouse_exit_; }
  bool mouse_press() const { return mouse_press_; }
  bool mouse_move() const { return mouse_move_; }
  bool mouse_release() const { return mouse_release_; }
  bool double_click() const { return double_click_; }

  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
        double_click_ = event->flags() & ui::EF_IS_DOUBLE_CLICK;
        mouse_press_ = true;
        break;
      case ui::ET_MOUSE_RELEASED:
        mouse_release_ = true;
        break;
      case ui::ET_MOUSE_MOVED:
        mouse_move_ = true;
        break;
      case ui::ET_MOUSE_ENTERED:
        mouse_enter_ = true;
        break;
      case ui::ET_MOUSE_EXITED:
        mouse_exit_ = true;
        break;
      default:
        NOTREACHED();
    }
    event->SetHandled();
  }

 private:
  bool mouse_enter_;
  bool mouse_exit_;
  bool mouse_press_;
  bool mouse_release_;
  bool mouse_move_;
  bool double_click_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventSynthDelegate);
};

class TimedEvents {
 private:
  base::SimpleTestTickClock tick_clock_;

 public:
  // Use a non-zero start time to pass DCHECKs which ensure events have had a
  // time assigned.
  TimedEvents() {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
  }

  base::TimeTicks Now() {
    base::TimeTicks t = tick_clock_.NowTicks();
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1));
    return t;
  }

  base::TimeTicks LeapForward(int time_in_millis) {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(time_in_millis));
    return tick_clock_.NowTicks();
  }

  base::TimeTicks InFuture(int time_in_millis) {
    return tick_clock_.NowTicks() +
        base::TimeDelta::FromMilliseconds(time_in_millis);
  }

  void SendScrollEvents(ui::EventSink* sink,
                        int x_start,
                        int y_start,
                        int dx,
                        int dy,
                        int touch_id,
                        int time_step_ms,
                        int num_steps,
                        GestureEventConsumeDelegate* delegate) {
    float x = x_start;
    float y = y_start;

    for (int i = 0; i < num_steps; i++) {
      x += dx;
      y += dy;
      ui::TouchEvent move(
          ui::ET_TOUCH_MOVED, gfx::Point(x, y), tick_clock_.NowTicks(),
          ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                             touch_id));
      ui::EventDispatchDetails details = sink->OnEventFromSource(&move);
      ASSERT_FALSE(details.dispatcher_destroyed);
      tick_clock_.Advance(base::TimeDelta::FromMilliseconds(time_step_ms));
    }
  }

  void SendScrollEvent(ui::EventSink* sink,
                       float x,
                       float y,
                       int touch_id,
                       GestureEventConsumeDelegate* delegate) {
    delegate->Reset();
    ui::TouchEvent move(
        ui::ET_TOUCH_MOVED, gfx::Point(), Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, touch_id));
    move.set_location_f(gfx::PointF(x, y));
    move.set_root_location_f(gfx::PointF(x, y));
    ui::EventDispatchDetails details = sink->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
  }
};

// An event handler to keep track of events.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler()
      : touch_released_count_(0),
        touch_pressed_count_(0),
        touch_moved_count_(0) {}

  ~TestEventHandler() override {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    switch (event->type()) {
      case ui::ET_TOUCH_RELEASED:
        touch_released_count_++;
        break;
      case ui::ET_TOUCH_PRESSED:
        touch_pressed_count_++;
        break;
      case ui::ET_TOUCH_MOVED:
        touch_moved_count_++;
        break;
      case ui::ET_TOUCH_CANCELLED:
        cancelled_touch_points_.push_back(event->location_f());
        break;
      default:
        break;
    }
  }

  void Reset() {
    touch_released_count_ = 0;
    touch_pressed_count_ = 0;
    touch_moved_count_ = 0;
    cancelled_touch_points_.clear();
  }

  int touch_released_count() const { return touch_released_count_; }
  int touch_pressed_count() const { return touch_pressed_count_; }
  int touch_moved_count() const { return touch_moved_count_; }
  int touch_cancelled_count() const {
    return static_cast<int>(cancelled_touch_points_.size());
  }
  const std::vector<gfx::PointF>& cancelled_touch_points() const {
    return cancelled_touch_points_;
  }

 private:
  int touch_released_count_;
  int touch_pressed_count_;
  int touch_moved_count_;
  std::vector<gfx::PointF> cancelled_touch_points_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

// Removes the target window from its parent when it receives a touch-cancel
// event.
class RemoveOnTouchCancelHandler : public TestEventHandler {
 public:
  RemoveOnTouchCancelHandler() {}
  ~RemoveOnTouchCancelHandler() override {}

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    TestEventHandler::OnTouchEvent(event);
    if (event->type() == ui::ET_TOUCH_CANCELLED) {
      Window* target = static_cast<Window*>(event->target());
      target->parent()->RemoveChild(target);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(RemoveOnTouchCancelHandler);
};

void DelayByLongPressTimeout() {
  ui::GestureProvider::Config config;
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      config.gesture_detector_config.longpress_timeout * 2);
  run_loop.Run();
}

void DelayByShowPressTimeout() {
  ui::GestureProvider::Config config;
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      config.gesture_detector_config.showpress_timeout * 2);
  run_loop.Run();
}

void SetTouchRadius(ui::TouchEvent* event, float radius_x, float radius_y) {
  // Using ctor (over direct struct access) due to it's special behavior with
  // radii.
  ui::PointerDetails details(
      ui::EventPointerType::POINTER_TYPE_TOUCH, event->pointer_details().id,
      radius_x, radius_y, event->pointer_details().force,
      event->pointer_details().twist, event->pointer_details().tilt_x,
      event->pointer_details().tilt_y,
      event->pointer_details().tangential_pressure);

  event->SetPointerDetailsForTest(details);
}

}  // namespace

class GestureRecognizerTest : public AuraTestBase {
 public:
  GestureRecognizerTest() {}

  void SetUp() override {
    AuraTestBase::SetUp();
    ui::GestureConfiguration::GetInstance()->set_show_press_delay_in_ms(2);
    ui::GestureConfiguration::GetInstance()->set_long_press_time_in_ms(3);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerTest);
};

class GestureRecognizerWithSwitchTest : public GestureRecognizerTest {
 public:
  GestureRecognizerWithSwitchTest() {}

  void SetUp() override {
    GestureRecognizerTest::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCompensateForUnstablePinchZoom);
    ui::GestureConfiguration::GetInstance()->set_min_pinch_update_span_delta(5);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerWithSwitchTest);
};

// Verify that we do not crash when removing a window during a cancel touch
// event originating from CancelActiveTouchesExcept. This monitors for
// regressions on crbug.com/651258.
TEST_F(GestureRecognizerTest, TouchCancelCanDestroyWindow) {
  auto delegate = std::make_unique<GestureEventConsumeDelegate>();
  TimedEvents tes;
  const int kTouchId = 1;

  // Create a window that will remove itself from its parent on touch cancelled
  // events.
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, gfx::Rect(0, 0, 200, 200), root_window()));
  auto handler = std::make_unique<RemoveOnTouchCancelHandler>();
  window->AddPreTargetHandler(handler.get());

  // Dispatch an event to |host_window| that will be cancelled.
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 101), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Cancel event, verify there is no crash.
  aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
      nullptr);

  EXPECT_EQ(1, handler->touch_cancelled_count());
  EXPECT_EQ(nullptr, window->parent());
  window->RemovePreTargetHandler(handler.get());
}

// Check that appropriate touch events generate tap gesture events.
TEST_F(GestureRecognizerTest, GestureEventTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->show_press());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->long_press());

  delegate->Reset();
  delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_down());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));

  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_TRUE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  EXPECT_EQ(1, delegate->tap_count());
}

// Check that appropriate touch events generate tap gesture events
// when information about the touch radii are provided.
TEST_F(GestureRecognizerTest, GestureEventTapRegion) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 800;
  const int kWindowHeight = 600;
  const int kTouchId = 2;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  // Test with no ET_TOUCH_MOVED events.
  {
     delegate->Reset();
     ui::TouchEvent press(
         ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&press, 5, 12);
     DispatchEventUsingWindowDispatcher(&press);
     EXPECT_FALSE(delegate->tap());
     EXPECT_TRUE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_TRUE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     // Make sure there is enough delay before the touch is released so that it
     // is recognized as a tap.
     delegate->Reset();
     ui::TouchEvent release(
         ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&release, 5, 12);

     DispatchEventUsingWindowDispatcher(&release);
     EXPECT_TRUE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_TRUE(delegate->end());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());

     EXPECT_EQ(1, delegate->tap_count());
     gfx::Point actual_point(delegate->tap_location());
     EXPECT_EQ(24, delegate->bounding_box().width());
     EXPECT_EQ(24, delegate->bounding_box().height());
     EXPECT_EQ(101, actual_point.x());
     EXPECT_EQ(201, actual_point.y());
  }

  // Test with no ET_TOUCH_MOVED events but different touch points and radii.
  {
     delegate->Reset();
     ui::TouchEvent press(
         ui::ET_TOUCH_PRESSED, gfx::Point(365, 290), tes.Now(),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&press, 8, 14);
     DispatchEventUsingWindowDispatcher(&press);
     EXPECT_FALSE(delegate->tap());
     EXPECT_TRUE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_TRUE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent release(
         ui::ET_TOUCH_RELEASED, gfx::Point(367, 291), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&release, 20, 13);

     DispatchEventUsingWindowDispatcher(&release);
     EXPECT_TRUE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_TRUE(delegate->end());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());

     EXPECT_EQ(1, delegate->tap_count());
     gfx::Point actual_point(delegate->tap_location());
     EXPECT_EQ(40, delegate->bounding_box().width());
     EXPECT_EQ(40, delegate->bounding_box().height());
     EXPECT_EQ(367, actual_point.x());
     EXPECT_EQ(291, actual_point.y());
  }

  // Test with a single ET_TOUCH_MOVED event.
  {
     delegate->Reset();
     ui::TouchEvent press(
         ui::ET_TOUCH_PRESSED, gfx::Point(46, 205), tes.Now(),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&press, 6, 10);
     DispatchEventUsingWindowDispatcher(&press);
     EXPECT_FALSE(delegate->tap());
     EXPECT_TRUE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_TRUE(delegate->begin());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent move(
         ui::ET_TOUCH_MOVED, gfx::Point(49, 204), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&move, 8, 12);
     DispatchEventUsingWindowDispatcher(&move);
     EXPECT_FALSE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent release(
         ui::ET_TOUCH_RELEASED, gfx::Point(49, 204), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&release, 4, 8);

     DispatchEventUsingWindowDispatcher(&release);
     EXPECT_TRUE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_TRUE(delegate->end());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());

     EXPECT_EQ(1, delegate->tap_count());
     gfx::Point actual_point(delegate->tap_location());
     EXPECT_EQ(16, delegate->bounding_box().width());
     EXPECT_EQ(16, delegate->bounding_box().height());
     EXPECT_EQ(49, actual_point.x());
     EXPECT_EQ(204, actual_point.y());
  }

  // Test with a few ET_TOUCH_MOVED events.
  {
     delegate->Reset();
     ui::TouchEvent press(
         ui::ET_TOUCH_PRESSED, gfx::Point(400, 150), tes.Now(),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&press, 7, 10);
     DispatchEventUsingWindowDispatcher(&press);
     EXPECT_FALSE(delegate->tap());
     EXPECT_TRUE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_TRUE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent move(
         ui::ET_TOUCH_MOVED, gfx::Point(397, 151), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&move, 13, 12);
     DispatchEventUsingWindowDispatcher(&move);
     EXPECT_FALSE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent move1(
         ui::ET_TOUCH_MOVED, gfx::Point(397, 149), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&move1, 16, 16);
     DispatchEventUsingWindowDispatcher(&move1);
     EXPECT_FALSE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent move2(
         ui::ET_TOUCH_MOVED, gfx::Point(400, 150), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&move2, 14, 10);
     DispatchEventUsingWindowDispatcher(&move2);
     EXPECT_FALSE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());
     EXPECT_FALSE(delegate->long_press());

     delegate->Reset();
     ui::TouchEvent release(
         ui::ET_TOUCH_RELEASED, gfx::Point(401, 149), tes.LeapForward(50),
         ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                            kTouchId));
     SetTouchRadius(&release, 8, 9);

     DispatchEventUsingWindowDispatcher(&release);
     EXPECT_TRUE(delegate->tap());
     EXPECT_FALSE(delegate->tap_down());
     EXPECT_FALSE(delegate->tap_cancel());
     EXPECT_FALSE(delegate->begin());
     EXPECT_TRUE(delegate->end());
     EXPECT_FALSE(delegate->scroll_begin());
     EXPECT_FALSE(delegate->scroll_update());
     EXPECT_FALSE(delegate->scroll_end());

     EXPECT_EQ(1, delegate->tap_count());
     gfx::Point actual_point(delegate->tap_location());
     EXPECT_EQ(18, delegate->bounding_box().width());
     EXPECT_EQ(18, delegate->bounding_box().height());
     EXPECT_EQ(401, actual_point.x());
     EXPECT_EQ(149, actual_point.y());
  }
}

// Check that appropriate touch events generate scroll gesture events.
TEST_F(GestureRecognizerTest, GestureEventScroll) {
  // We'll start by moving the touch point by (10.5, 10.5). We want 5 dips of
  // that distance to be consumed by the slop, so we set the slop radius to
  // sqrt(5 * 5 + 5 * 5).
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(sqrt(5.f * 5 + 5 * 5));
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 5;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_BEGIN,
                  ui::ET_GESTURE_TAP_DOWN);

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  // The first movement is diagonal, to ensure that we have a free scroll,
  // and not a rail scroll.
  tes.SendScrollEvent(event_sink(), 111.5, 211.5, kTouchId, delegate.get());
  EXPECT_3_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE);
  // The slop consumed 5 dips
  EXPECT_FLOAT_EQ(5.5, delegate->scroll_x());
  EXPECT_FLOAT_EQ(5.5, delegate->scroll_y());
  EXPECT_EQ(gfx::Point(1, 1).ToString(),
            delegate->scroll_begin_position().ToString());

  // When scrolling with a single finger, the bounding box of the gesture should
  // be empty, since it's a single point and the radius for testing is zero.
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());

  // Move some more to generate a few more scroll updates. Make sure that we get
  // out of the snap channel for the unified GR.
  tes.SendScrollEvent(event_sink(), 20, 120, kTouchId, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_FLOAT_EQ(-91.5, delegate->scroll_x());
  EXPECT_FLOAT_EQ(-91.5, delegate->scroll_y());
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());

  tes.SendScrollEvent(event_sink(), 50, 124, kTouchId, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_EQ(30, delegate->scroll_x());
  EXPECT_EQ(4, delegate->scroll_y());
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_SCROLL_FLING_START,
                  ui::ET_GESTURE_END);
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());
}

// Check that predicted scroll update positions are correct.
TEST_F(GestureRecognizerTest, GestureEventScrollPrediction) {
  // We'll start by moving the touch point by (5, 5). We want all of that
  // distance to be consumed by the slop, so we set the slop radius to
  // sqrt(5 * 5 + 5 * 5).
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(sqrt(5.f * 5 + 5 * 5));

  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 5;
  gfx::Rect bounds(95, 195, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  // Tracks the total scroll since we want to verify that the correct position
  // will be scrolled to throughout the prediction.
  gfx::Vector2dF total_scroll;
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(96, 196), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_BEGIN,
                  ui::ET_GESTURE_TAP_DOWN);
  delegate->Reset();

  // Get rid of touch slop.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(111, 211), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_3_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE);
  total_scroll.set_x(total_scroll.x() + delegate->scroll_x());
  total_scroll.set_y(total_scroll.y() + delegate->scroll_y());

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  // The first movement is diagonal, to ensure that we have a free scroll,
  // and not a rail scroll.
  tes.LeapForward(30);
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());
  EXPECT_1_EVENT(delegate->events(),
                 ui::ET_GESTURE_SCROLL_UPDATE);
  total_scroll.set_x(total_scroll.x() + delegate->scroll_x());
  total_scroll.set_y(total_scroll.y() + delegate->scroll_y());

  // Move some more to generate a few more scroll updates.
  tes.LeapForward(30);
  tes.SendScrollEvent(event_sink(), 110, 211, kTouchId, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  total_scroll.set_x(total_scroll.x() + delegate->scroll_x());
  total_scroll.set_y(total_scroll.y() + delegate->scroll_y());

  tes.LeapForward(30);
  tes.SendScrollEvent(event_sink(), 140, 215, kTouchId, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  total_scroll.set_x(total_scroll.x() + delegate->scroll_x());
  total_scroll.set_y(total_scroll.y() + delegate->scroll_y());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
}

// Check that the bounding box during a scroll event is correct.
TEST_F(GestureRecognizerTest, GestureEventScrollBoundingBox) {
  TimedEvents tes;
  for (int radius = 1; radius <= 10; ++radius) {
    ui::GestureConfiguration::GetInstance()->set_default_radius(radius);
    std::unique_ptr<GestureEventConsumeDelegate> delegate(
        new GestureEventConsumeDelegate());
    const int kWindowWidth = 123;
    const int kWindowHeight = 45;
    const int kTouchId = 5;
    gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
    std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
        delegate.get(), -1234, bounds, root_window()));

    const int kPositionX = 101;
    const int kPositionY = 201;
    delegate->Reset();
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, gfx::Point(kPositionX, kPositionY), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&press);
    EXPECT_EQ(gfx::Rect(kPositionX - radius, kPositionY - radius, radius * 2,
                        radius * 2),
              delegate->bounding_box());

    const int kScrollAmount = 50;
    tes.SendScrollEvents(event_sink(), kPositionX, kPositionY, 1, 1, kTouchId,
                         1, kScrollAmount, delegate.get());
    EXPECT_EQ(gfx::Point(1, 1).ToString(),
              delegate->scroll_begin_position().ToString());
    EXPECT_EQ(
        gfx::Rect(kPositionX + kScrollAmount - radius,
                  kPositionY + kScrollAmount - radius, radius * 2, radius * 2),
        delegate->bounding_box());

    // Release the touch. This should end the scroll.
    delegate->Reset();
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED,
        gfx::Point(kPositionX + kScrollAmount, kPositionY + kScrollAmount),
        press.time_stamp() + base::TimeDelta::FromMilliseconds(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&release);
    EXPECT_EQ(
        gfx::Rect(kPositionX + kScrollAmount - radius,
                  kPositionY + kScrollAmount - radius, radius * 2, radius * 2),
        delegate->bounding_box());
  }
  ui::GestureConfiguration::GetInstance()->set_default_radius(0);
}

// Check Scroll End Events report correct velocities
// if the user was on a horizontal rail
TEST_F(GestureRecognizerTest, GestureEventHorizontalRailFling) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Get rid of touch slop.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  delegate->Reset();


  // Move the touch-point horizontally enough that it is considered a
  // horizontal scroll.
  tes.SendScrollEvent(event_sink(), 30, 1, kTouchId, delegate.get());
  EXPECT_FLOAT_EQ(0, delegate->scroll_y());
  EXPECT_FLOAT_EQ(20, delegate->scroll_x());

  // Get a high x velocity, while still staying on the rail
  const int kScrollAmount = 8;
  tes.SendScrollEvents(event_sink(), 1, 1, 100, 10, kTouchId, 1, kScrollAmount,
                       delegate.get());

  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  EXPECT_TRUE(delegate->fling());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_GT(delegate->velocity_x(), 0);
  EXPECT_EQ(0, delegate->velocity_y());
}

// Check Scroll End Events report correct velocities
// if the user was on a vertical rail
TEST_F(GestureRecognizerTest, GestureEventVerticalRailFling) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Get rid of touch slop.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(0, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  delegate->Reset();

  // Move the touch-point vertically enough that it is considered a
  // vertical scroll.
  tes.SendScrollEvent(event_sink(), 1, 30, kTouchId, delegate.get());
  EXPECT_EQ(20, delegate->scroll_y());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_velocity_x());

  // Get a high y velocity, while still staying on the rail
  const int kScrollAmount = 8;
  tes.SendScrollEvents(event_sink(), 1, 6, 10, 100, kTouchId, 1, kScrollAmount,
                       delegate.get());
  EXPECT_EQ(0, delegate->scroll_velocity_x());

  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 206), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  EXPECT_TRUE(delegate->fling());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(0, delegate->velocity_x());
  EXPECT_GT(delegate->velocity_y(), 0);
}

// Check Scroll End Events report non-zero velocities if the user is not on a
// rail
TEST_F(GestureRecognizerTest, GestureEventNonRailFling) {
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(0);
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Move the touch-point such that a non-rail scroll begins, and we're outside
  // the snap channel for the unified GR.
  tes.SendScrollEvent(event_sink(), 50, 50, kTouchId, delegate.get());
  EXPECT_EQ(50, delegate->scroll_y());
  EXPECT_EQ(50, delegate->scroll_x());

  const int kScrollAmount = 8;
  tes.SendScrollEvents(event_sink(), 1, 1, 10, 100, kTouchId, 1, kScrollAmount,
                       delegate.get());

  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  EXPECT_TRUE(delegate->fling());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_GT(delegate->velocity_x(), 0);
  EXPECT_GT(delegate->velocity_y(), 0);
}

// Check that appropriate touch events generate long press events
TEST_F(GestureRecognizerTest, GestureEventLongPress) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->tap_cancel());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Wait until the timer runs out
  delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(delegate->long_press());
  EXPECT_FALSE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->long_press());

  // Note the tap cancel isn't dispatched until the release
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->tap());
}

// Check that scrolling prevents a long press.
TEST_F(GestureRecognizerTest, GestureEventLongPressCancelledByScroll) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 6;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());
  EXPECT_FALSE(delegate->tap_cancel());

  // Scroll around, to cancel the long press
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());

  // Wait until a long press event would have fired, if it hadn't been
  // cancelled.
  DelayByLongPressTimeout();

  EXPECT_FALSE(delegate->long_press());
  EXPECT_TRUE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(10),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->long_press());
  EXPECT_FALSE(delegate->tap_cancel());
}

// Check that appropriate touch events generate long tap events
TEST_F(GestureRecognizerTest, GestureEventLongTap) {
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_down_duration_for_click_in_ms(3);
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->tap_cancel());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Wait until the timer runs out
  delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(delegate->long_press());
  EXPECT_FALSE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->long_press());
  EXPECT_TRUE(delegate->long_tap());

  // Note the tap cancel isn't dispatched until the release
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->tap());
}

// Check that second tap cancels a long press
TEST_F(GestureRecognizerTest, GestureEventLongPressCancelledBySecondTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 8;
  const int kTouchId2 = 2;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_TRUE(delegate->begin());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Second tap, to cancel the long press
  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(delegate->tap_down());  // no touch down for second tap.
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());

  // Wait until the timer runs out
  DelayByLongPressTimeout();

  // No long press occurred
  EXPECT_FALSE(delegate->long_press());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->long_press());
  EXPECT_TRUE(delegate->two_finger_tap());
  EXPECT_FALSE(delegate->tap_cancel());
}

// Check that horizontal scroll gestures cause scrolls on horizontal rails.
// Also tests that horizontal rails can be broken.
TEST_F(GestureRecognizerTest, GestureEventHorizontalRailScroll) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Get rid of touch slop.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(5, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));

  DispatchEventUsingWindowDispatcher(&move);
  delegate->Reset();

  // Move the touch-point horizontally enough that it is considered a
  // horizontal scroll.
  tes.SendScrollEvent(event_sink(), 25, 0, kTouchId, delegate.get());
  EXPECT_EQ(0, delegate->scroll_y());
  EXPECT_EQ(20, delegate->scroll_x());

  tes.SendScrollEvent(event_sink(), 30, 6, kTouchId, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  // y shouldn't change, as we're on a horizontal rail.
  EXPECT_EQ(0, delegate->scroll_y());

  // Send enough information that a velocity can be calculated for the gesture,
  // and we can break the rail
  const int kScrollAmount = 8;
  tes.SendScrollEvents(event_sink(), 1, 1, 6, 100, kTouchId, 1, kScrollAmount,
                       delegate.get());

  tes.SendScrollEvent(event_sink(), 5, 0, kTouchId, delegate.get());
  tes.SendScrollEvent(event_sink(), 10, 5, kTouchId, delegate.get());

  // The rail should be broken
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  EXPECT_EQ(5, delegate->scroll_y());
}

// Check that vertical scroll gestures cause scrolls on vertical rails.
// Also tests that vertical rails can be broken.
TEST_F(GestureRecognizerTest, GestureEventVerticalRailScroll) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Get rid of touch slop.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(0, 5), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  delegate->Reset();

  // Move the touch-point vertically enough that it is considered a
  // vertical scroll.
  tes.SendScrollEvent(event_sink(), 0, 25, kTouchId, delegate.get());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(20, delegate->scroll_y());

  tes.SendScrollEvent(event_sink(), 6, 30, kTouchId, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_y());
  // x shouldn't change, as we're on a vertical rail.
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_velocity_x());

  // Send enough information that a velocity can be calculated for the gesture,
  // and we can break the rail
  const int kScrollAmount = 8;
  tes.SendScrollEvents(event_sink(), 1, 6, 100, 1, kTouchId, 1, kScrollAmount,
                       delegate.get());

  tes.SendScrollEvent(event_sink(), 0, 5, kTouchId, delegate.get());
  tes.SendScrollEvent(event_sink(), 5, 10, kTouchId, delegate.get());

  // The rail should be broken
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  EXPECT_EQ(5, delegate->scroll_y());
}

TEST_F(GestureRecognizerTest, GestureTapFollowedByScroll) {
  // We'll start by moving the touch point by (5, 5). We want all of that
  // distance to be consumed by the slop, so we set the slop radius to
  // sqrt(5 * 5 + 5 * 5).
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(sqrt(5.f * 5 + 5 * 5));

  // First, tap. Then, do a scroll using the same touch-id.
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 3;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_TRUE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Now, do a scroll gesture. Delay it sufficiently so that it doesn't trigger
  // a double-tap.
  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.LeapForward(1000),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Get rid of touch slop.
  ui::TouchEvent move_remove_slop(
      ui::ET_TOUCH_MOVED, gfx::Point(116, 216), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move_remove_slop);
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(10, delegate->scroll_x_hint());
  EXPECT_EQ(10, delegate->scroll_y_hint());

  delegate->Reset();

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  // The first movement is diagonal, to ensure that we have a free scroll,
  // and not a rail scroll.
  delegate->Reset();
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(135, 235), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(19, delegate->scroll_x());
  EXPECT_EQ(19, delegate->scroll_y());

  // Move some more to generate a few more scroll updates.
  delegate->Reset();
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(115, 216), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(-20, delegate->scroll_x());
  EXPECT_EQ(-19, delegate->scroll_y());
  EXPECT_EQ(0, delegate->scroll_x_hint());
  EXPECT_EQ(0, delegate->scroll_y_hint());

  delegate->Reset();
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(145, 220), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(30, delegate->scroll_x());
  EXPECT_EQ(4, delegate->scroll_y());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_TRUE(delegate->fling());
}

TEST_F(GestureRecognizerTest, AsynchronousGestureRecognition) {
  std::unique_ptr<QueueTouchEventDelegate> queued_delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 6;
  const int kTouchId2 = 4;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> queue(CreateTestWindowWithDelegate(
      queued_delegate.get(), -1234, bounds, root_window()));

  queued_delegate->set_window(queue.get());

  // Touch down on the window. This should not generate any gesture event.
  queued_delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Introduce some delay before the touch is released so that it is recognized
  // as a tap. However, this still should not create any gesture events.
  queued_delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
      press.time_stamp() + base::TimeDelta::FromMilliseconds(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Create another window, and place a touch-down on it. This should create a
  // tap-down gesture.
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -2345, gfx::Rect(0, 0, 50, 50), root_window()));
  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&release2);

  // Process the first queued event.
  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_TRUE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Now, process the second queued event.
  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_TRUE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Start all over. Press on the first window, then press again on the second
  // window. The second press should still go to the first window.
  queued_delegate->Reset();
  ui::TouchEvent press3(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press3);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  delegate->Reset();
  ui::TouchEvent press4(
      ui::ET_TOUCH_PRESSED, gfx::Point(103, 203), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press4);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Move the second touch-point enough so that it is considered a pinch. This
  // should generate SCROLL_BEGIN, PINCH_BEGIN, and PINCH_UPDATE gestures.
  queued_delegate->Reset();
  delegate->Reset();
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED,
      gfx::Point(203 + ui::GestureConfiguration::GetInstance()
                           ->max_touch_move_in_pixels_for_click(),
                 303),
      tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_TRUE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());  // no touch down for second tap.
  EXPECT_TRUE(queued_delegate->tap_cancel());
  EXPECT_TRUE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());
  EXPECT_FALSE(queued_delegate->pinch_begin());
  EXPECT_FALSE(queued_delegate->pinch_update());
  EXPECT_FALSE(queued_delegate->pinch_end());

  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->end());
  EXPECT_TRUE(queued_delegate->scroll_begin());

  EXPECT_TRUE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());
  EXPECT_TRUE(queued_delegate->pinch_begin());
  EXPECT_TRUE(queued_delegate->pinch_update());
  EXPECT_FALSE(queued_delegate->pinch_end());
}

// Check that appropriate touch events generate pinch gesture events.
TEST_F(GestureRecognizerTest, GestureEventPinchFromScroll) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 5;
  const int kTouchId2 = 3;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_BEGIN,
                  ui::ET_GESTURE_TAP_DOWN);

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  delegate->Reset();
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(130, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_3_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE);

  // Press the second finger. It should cause pinch-begin. Note that we will not
  // transition to two finger tap here because the touch points are far enough.
  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_BEGIN);
  EXPECT_EQ(gfx::Rect(10, 10, 120, 291).ToString(),
            delegate->bounding_box().ToString());

  // Move the first finger.
  delegate->Reset();
  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(95, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move3);
  EXPECT_3_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_PINCH_BEGIN, ui::ET_GESTURE_PINCH_UPDATE);
  EXPECT_EQ(gfx::Rect(10, 10, 85, 191).ToString(),
            delegate->bounding_box().ToString());

  // Now move the second finger.
  delegate->Reset();
  ui::TouchEvent move4(
      ui::ET_TOUCH_MOVED, gfx::Point(55, 15), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move4);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_PINCH_UPDATE);
  EXPECT_EQ(gfx::Rect(55, 15, 40, 186).ToString(),
            delegate->bounding_box().ToString());

  // Release the first finger. This should end pinch.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_2_EVENTS(delegate->events(),
                 ui::ET_GESTURE_PINCH_END,
                 ui::ET_GESTURE_END);
  EXPECT_EQ(gfx::Rect(55, 15, 46, 186).ToString(),
            delegate->bounding_box().ToString());

  // Move the second finger. This should still generate a scroll.
  delegate->Reset();
  ui::TouchEvent move5(
      ui::ET_TOUCH_MOVED, gfx::Point(25, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move5);
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());
}

TEST_F(GestureRecognizerTest, GestureEventPinchFromScrollFromPinch) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 5;
  const int kTouchId2 = 3;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(delegate->pinch_begin());

  // Touch move triggers pinch begin and update.
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId1, delegate.get());
  EXPECT_TRUE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Touch move triggers pinch update.
  tes.SendScrollEvent(event_sink(), 160, 200, kTouchId1, delegate.get());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Pinch has started, now release the second finger
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_TRUE(delegate->pinch_end());

  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId2, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());

  // Pinch again
  delegate->Reset();
  ui::TouchEvent press3(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press3);
  // Now the touch points are close. So we will go into two finger tap.
  // Move the touch-point enough to break two-finger-tap and enter pinch.
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(101, 50), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_TRUE(delegate->pinch_begin());

  tes.SendScrollEvent(event_sink(), 350, 350, kTouchId1, delegate.get());
  EXPECT_TRUE(delegate->pinch_update());
}

TEST_F(GestureRecognizerTest, GestureEventPinchFromTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  const int kTouchId2 = 5;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_BEGIN,
                  ui::ET_GESTURE_TAP_DOWN);
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());

  // Press the second finger far enough to break two finger tap.
  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_BEGIN);
  EXPECT_EQ(gfx::Rect(10, 10, 91, 291).ToString(),
            delegate->bounding_box().ToString());

  // Move the first finger.
  delegate->Reset();
  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move3);
  EXPECT_4_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE, ui::ET_GESTURE_PINCH_BEGIN,
                  ui::ET_GESTURE_PINCH_UPDATE);
  EXPECT_EQ(gfx::Rect(10, 10, 55, 191).ToString(),
            delegate->bounding_box().ToString());

  // Now move the second finger.
  delegate->Reset();
  ui::TouchEvent move4(
      ui::ET_TOUCH_MOVED, gfx::Point(55, 15), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move4);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_PINCH_UPDATE);
  EXPECT_EQ(gfx::Rect(55, 15, 10, 186).ToString(),
            delegate->bounding_box().ToString());

  // Release the first finger. This should end pinch.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(10),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_PINCH_END,
                  ui::ET_GESTURE_END);
  EXPECT_EQ(gfx::Rect(55, 15, 46, 186).ToString(),
            delegate->bounding_box().ToString());

  // Move the second finger. This should still generate a scroll.
  delegate->Reset();
  ui::TouchEvent move5(
      ui::ET_TOUCH_MOVED, gfx::Point(25, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move5);
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());
}

TEST_F(GestureRecognizerTest, GestureEventIgnoresDisconnectedEvents) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;

  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 6));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
}

// Check that a touch is locked to the window of the closest current touch
// within max_separation_for_gesture_touches_in_pixels
TEST_F(GestureRecognizerTest, GestureEventTouchLockSelectsCorrectWindow) {
  ui::GestureRecognizer* gesture_recognizer =
      aura::Env::GetInstance()->gesture_recognizer();
  TimedEvents tes;

  ui::GestureConsumer* target;
  const int kNumWindows = 4;

  std::unique_ptr<GestureEventConsumeDelegate* []> delegates(
      new GestureEventConsumeDelegate*[kNumWindows]);

  ui::GestureConfiguration::GetInstance()
      ->set_max_separation_for_gesture_touches_in_pixels(499);

  std::unique_ptr<gfx::Rect[]> window_bounds(new gfx::Rect[kNumWindows]);
  window_bounds[0] = gfx::Rect(0, 0, 1, 1);
  window_bounds[1] = gfx::Rect(500, 0, 1, 1);
  window_bounds[2] = gfx::Rect(0, 500, 1, 1);
  window_bounds[3] = gfx::Rect(500, 500, 1, 1);

  std::unique_ptr<aura::Window* []> windows(new aura::Window*[kNumWindows]);

  // Instantiate windows with |window_bounds| and touch each window at
  // its origin.
  for (int i = 0; i < kNumWindows; ++i) {
    delegates[i] = new GestureEventConsumeDelegate();
    windows[i] = CreateTestWindowWithDelegate(
        delegates[i], i, window_bounds[i], root_window());
    windows[i]->set_id(i);
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, window_bounds[i].origin(), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, i));
    DispatchEventUsingWindowDispatcher(&press);
  }

  // Touches should now be associated with the closest touch within
  // ui::GestureConfiguration::max_separation_for_gesture_touches_in_pixels
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(11.f, 11.f), -1);
  EXPECT_EQ("0", WindowIDAsString(target));
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(511.f, 11.f), -1);
  EXPECT_EQ("1", WindowIDAsString(target));
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(11.f, 511.f), -1);
  EXPECT_EQ("2", WindowIDAsString(target));
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(511.f, 511.f), -1);
  EXPECT_EQ("3", WindowIDAsString(target));

  // Add a touch in the middle associated with windows[2]
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 500), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         kNumWindows));
  DispatchEventUsingWindowDispatcher(&press);
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(250, 250), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         kNumWindows));
  DispatchEventUsingWindowDispatcher(&move);

  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(250.f, 250.f), -1);
  EXPECT_EQ("2", WindowIDAsString(target));

  // Make sure that ties are broken by distance to a current touch
  // Closer to the point in the bottom right.
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(380.f, 380.f), -1);
  EXPECT_EQ("3", WindowIDAsString(target));

  // This touch is closer to the point in the middle
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(300.f, 300.f), -1);
  EXPECT_EQ("2", WindowIDAsString(target));

  // A touch too far from other touches won't be locked to anything
  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(1000.f, 1000.f), -1);
  EXPECT_TRUE(target == NULL);

  // Move a touch associated with windows[2] to 1000, 1000
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(1000, 1000), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         kNumWindows));
  DispatchEventUsingWindowDispatcher(&move2);

  target =
      gesture_recognizer->GetTargetForLocation(gfx::PointF(1000.f, 1000.f), -1);
  EXPECT_EQ("2", WindowIDAsString(target));

  for (int i = 0; i < kNumWindows; ++i) {
    // Delete windows before deleting delegates.
    delete windows[i];
    delete delegates[i];
  }
}

// Check that a touch's target will not be effected by a touch on a different
// screen.
TEST_F(GestureRecognizerTest, GestureEventTouchLockIgnoresOtherScreens) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  gfx::Rect bounds(0, 0, 10, 10);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowWithDelegate(delegate.get(), 0, bounds, root_window()));

  const int kTouchId1 = 8;
  const int kTouchId2 = 2;
  TimedEvents tes;

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(5, 5), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  ui::EventTestApi test_press1(&press1);
  test_press1.set_source_device_id(1);
  DispatchEventUsingWindowDispatcher(&press1);

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(20, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  ui::EventTestApi test_press2(&press2);
  test_press2.set_source_device_id(2);
  DispatchEventUsingWindowDispatcher(&press2);

  // The second press should not have been locked to the same target as the
  // first, as they occured on different displays.
  EXPECT_NE(
      aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
          press1),
      aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
          press2));
}

// Check that touch events outside the root window are still handled
// by the root window's gesture sequence.
TEST_F(GestureRecognizerTest, GestureEventOutsideRootWindowTap) {
  TimedEvents tes;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithBounds(
      gfx::Rect(-100, -100, 2000, 2000), root_window()));

  gfx::Point pos1(-10, -10);
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, pos1, tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&press1);

  gfx::Point pos2(1000, 1000);
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, pos2, tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  DispatchEventUsingWindowDispatcher(&press2);

  // As these presses were outside the root window, they should be
  // associated with the root window.
  EXPECT_EQ(
      root_window(),
      static_cast<aura::Window*>(
          aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
              press1)));
  EXPECT_EQ(
      root_window(),
      static_cast<aura::Window*>(
          aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
              press2)));
}

TEST_F(GestureRecognizerTest, NoTapWithPreventDefaultedRelease) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->tap_down());
  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_cancel());
}

TEST_F(GestureRecognizerTest, PinchScrollWithPreventDefaultedRelease) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId1 = 7;
  const int kTouchId2 = 5;
  gfx::Rect bounds(10, 20, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  {
    delegate->Reset();
    ui::TouchEvent press(
        ui::ET_TOUCH_PRESSED, gfx::Point(15, 25), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    ui::TouchEvent move(
        ui::ET_TOUCH_MOVED, gfx::Point(20, 95), tes.LeapForward(200),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, gfx::Point(15, 25), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&press);
    DispatchEventUsingWindowDispatcher(&move);
    DispatchEventUsingWindowDispatcher(&release);
    delegate->Reset();

    // Ack the press event.
    delegate->ReceivedAck();
    EXPECT_2_EVENTS(
        delegate->events(), ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_TAP_DOWN);
    delegate->Reset();

    // Ack the move event.
    delegate->ReceivedAck();
    EXPECT_3_EVENTS(delegate->events(),
                    ui::ET_GESTURE_TAP_CANCEL,
                    ui::ET_GESTURE_SCROLL_BEGIN,
                    ui::ET_GESTURE_SCROLL_UPDATE);
    delegate->Reset();

    // Ack the release event. Although the release event has been processed, it
    // should still generate a scroll-end event.
    delegate->ReceivedAckPreventDefaulted();
    EXPECT_2_EVENTS(
        delegate->events(), ui::ET_GESTURE_SCROLL_END, ui::ET_GESTURE_END);
  }

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(15, 25), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 95), tes.LeapForward(200),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(15, 25), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(55, 25), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(145, 85), tes.LeapForward(1000),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(145, 85), tes.LeapForward(14),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));

  // Do a pinch.
  DispatchEventUsingWindowDispatcher(&press);
  DispatchEventUsingWindowDispatcher(&move);
  DispatchEventUsingWindowDispatcher(&press2);
  DispatchEventUsingWindowDispatcher(&move2);
  DispatchEventUsingWindowDispatcher(&release);
  DispatchEventUsingWindowDispatcher(&release2);

  // Ack the press and move events.
  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_TAP_DOWN);

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_3_EVENTS(delegate->events(),
                 ui::ET_GESTURE_TAP_CANCEL,
                 ui::ET_GESTURE_SCROLL_BEGIN,
                 ui::ET_GESTURE_SCROLL_UPDATE);

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_BEGIN);

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_3_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_PINCH_BEGIN, ui::ET_GESTURE_PINCH_UPDATE);

  // Ack the first release. Although the release is processed, it should still
  // generate a pinch-end event.
  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_PINCH_END, ui::ET_GESTURE_END);

  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_SCROLL_END, ui::ET_GESTURE_END);
}

TEST_F(GestureRecognizerTest, GestureEndLocation) {
  GestureEventConsumeDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, -1234, gfx::Rect(10, 10, 300, 300), root_window()));
  ui::test::EventGenerator generator(root_window(), window.get());
  const gfx::Point begin(20, 20);
  const gfx::Point end(150, 150);
  const gfx::Vector2d window_offset =
      window->bounds().origin().OffsetFromOrigin();
  generator.GestureScrollSequence(begin, end,
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);
  EXPECT_EQ((begin - window_offset).ToString(),
            delegate.scroll_begin_position().ToString());
  EXPECT_EQ((end - window_offset).ToString(),
            delegate.gesture_end_location().ToString());
}

TEST_F(GestureRecognizerTest, CaptureSendsGestureEnd) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, gfx::Rect(10, 10, 300, 300), root_window()));
  ui::test::EventGenerator generator(root_window());

  generator.MoveMouseRelativeTo(window.get(), gfx::Point(10, 10));
  generator.PressTouch();
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(delegate->tap_down());

  std::unique_ptr<aura::Window> capture(
      CreateTestWindowWithBounds(gfx::Rect(10, 10, 200, 200), root_window()));
  capture->SetCapture();
  RunAllPendingInMessageLoop();

  EXPECT_TRUE(delegate->end());
  EXPECT_TRUE(delegate->tap_cancel());
}

// Check that previous touch actions that are completely finished (either
// released or cancelled), do not receive extra synthetic cancels upon change of
// capture.
TEST_F(GestureRecognizerTest, CaptureDoesNotCancelFinishedTouches) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  std::unique_ptr<TestEventHandler> handler(new TestEventHandler);
  root_window()->AddPreTargetHandler(handler.get());

  // Create a window and set it as the capture window.
  std::unique_ptr<aura::Window> window1(CreateTestWindowWithDelegate(
      delegate.get(), -1234, gfx::Rect(10, 10, 300, 300), root_window()));
  window1->SetCapture();

  ui::test::EventGenerator generator(root_window());
  TimedEvents tes;

  // Generate two touch-press events on the window.
  std::unique_ptr<ui::TouchEvent> touch0(new ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(20, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0)));
  std::unique_ptr<ui::TouchEvent> touch1(new ui::TouchEvent(
      ui::ET_TOUCH_PRESSED, gfx::Point(30, 30), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1)));
  generator.Dispatch(touch0.get());
  generator.Dispatch(touch1.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(2, handler->touch_pressed_count());

  // Advance time.
  tes.LeapForward(1000);

  // End the two touches, one by a touch-release and one by a touch-cancel; to
  // cover both cases.
  touch0 = std::make_unique<ui::TouchEvent>(
      ui::ET_TOUCH_RELEASED, gfx::Point(20, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  touch1 = std::make_unique<ui::TouchEvent>(
      ui::ET_TOUCH_CANCELLED, gfx::Point(30, 30), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  generator.Dispatch(touch0.get());
  generator.Dispatch(touch1.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1, handler->touch_released_count());
  EXPECT_EQ(1, handler->touch_cancelled_count());

  // Create a new window and set it as the new capture window.
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowWithBounds(gfx::Rect(100, 100, 300, 300), root_window()));
  window2->SetCapture();
  RunAllPendingInMessageLoop();
  // Check that setting capture does not generate any synthetic touch-cancels
  // for the two previously finished touch actions.
  EXPECT_EQ(1, handler->touch_cancelled_count());

  root_window()->RemovePreTargetHandler(handler.get());
}

// Tests that a press with the same touch id as an existing touch is ignored.
TEST_F(GestureRecognizerTest, PressDoesNotCrash) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;

  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, gfx::Rect(10, 10, 300, 300), root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(45, 45), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 7));
  SetTouchRadius(&press, 40, 0);
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_EQ(gfx::Rect(5, 5, 80, 80).ToString(),
            delegate->bounding_box().ToString());
  delegate->Reset();

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(55, 45), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 7));
  DispatchEventUsingWindowDispatcher(&press2);

  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
}

TEST_F(GestureRecognizerTest, TwoFingerTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_TAP_DOWN);

  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_TAP_CANCEL, ui::ET_GESTURE_BEGIN);

  // Little bit of touch move should not affect our state.
  // Moving within slop region doesn't cause scrolling.
  delegate->Reset();
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(102, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move1);
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(131, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_0_EVENTS(delegate->events());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));

  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_TWO_FINGER_TAP, ui::ET_GESTURE_END);

  // Lift second finger.
  // Two fingers have been down at some point during the current touch,
  // single tap doesn't happen while releasing the second finger.
  delegate->Reset();
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(130, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));

  DispatchEventUsingWindowDispatcher(&release2);
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_END);
}

TEST_F(GestureRecognizerTest, TwoFingerTapExpired) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);

  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);

  // Send release event after sufficient delay so that two finger time expires.
  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(1000),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));

  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->two_finger_tap());

  // Lift second finger.
  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(130, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));

  DispatchEventUsingWindowDispatcher(&release2);
  EXPECT_FALSE(delegate->two_finger_tap());
}

TEST_F(GestureRecognizerTest, TwoFingerTapChangesToPinch) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  TimedEvents tes;

  // Test moving first finger
  {
    gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
    std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
        delegate.get(), -1234, bounds, root_window()));

    delegate->Reset();
    ui::TouchEvent press1(
        ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&press1);

    delegate->Reset();
    ui::TouchEvent press2(
        ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&press2);

    tes.SendScrollEvent(event_sink(), 230, 330, kTouchId1, delegate.get());
    EXPECT_FALSE(delegate->two_finger_tap());
    EXPECT_TRUE(delegate->pinch_begin());

    // Make sure there is enough delay before the touch is released so that it
    // is recognized as a tap.
    delegate->Reset();
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));

    DispatchEventUsingWindowDispatcher(&release);
    EXPECT_FALSE(delegate->two_finger_tap());
    EXPECT_TRUE(delegate->pinch_end());
  }

  // Test moving second finger
  {
    gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
    std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
        delegate.get(), -1234, bounds, root_window()));

    delegate->Reset();
    ui::TouchEvent press1(
        ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&press1);

    delegate->Reset();
    ui::TouchEvent press2(
        ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&press2);

    tes.SendScrollEvent(event_sink(), 301, 230, kTouchId2, delegate.get());
    EXPECT_FALSE(delegate->two_finger_tap());
    EXPECT_TRUE(delegate->pinch_begin());

    // Make sure there is enough delay before the touch is released so that it
    // is recognized as a tap.
    delegate->Reset();
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));

    DispatchEventUsingWindowDispatcher(&release);
    EXPECT_FALSE(delegate->two_finger_tap());
    EXPECT_TRUE(delegate->pinch_end());
  }
}

TEST_F(GestureRecognizerTest, NoTwoFingerTapWhenFirstFingerHasScrolled) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  TimedEvents tes;

  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId1, delegate.get());

  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);

  EXPECT_FALSE(delegate->pinch_begin());

  // Make sure there is enough delay before the touch is released so that it
  // is recognized as a tap.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));

  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_FALSE(delegate->two_finger_tap());
  EXPECT_FALSE(delegate->pinch_end());
}

TEST_F(GestureRecognizerTest, MultiFingerSwipe) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;

  gfx::Rect bounds(5, 10, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  const int kSteps = 15;
  const int kTouchPoints = 4;
  gfx::Point points[kTouchPoints] = {
    gfx::Point(10, 30),
    gfx::Point(30, 20),
    gfx::Point(50, 30),
    gfx::Point(80, 50)
  };

  ui::test::EventGenerator generator(root_window(), window.get());

  // The unified gesture recognizer assumes a finger has stopped if it hasn't
  // moved for too long. See ui/events/gesture_detection/velocity_tracker.cc's
  // kAssumePointerStoppedTimeMs.
  for (int count = 2; count <= kTouchPoints; ++count) {
    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 0, -11 * kSteps);
    EXPECT_TRUE(delegate->swipe_up());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 0, 11 * kSteps);
    EXPECT_TRUE(delegate->swipe_down());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, -11 * kSteps, 0);
    EXPECT_TRUE(delegate->swipe_left());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 11 * kSteps, 0);
    EXPECT_TRUE(delegate->swipe_right());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 5 * kSteps, 12 * kSteps);
    EXPECT_FALSE(delegate->swipe_down());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 4 * kSteps, 12 * kSteps);
    EXPECT_TRUE(delegate->swipe_down());
    delegate->Reset();

    generator.GestureMultiFingerScroll(
        count, points, 10, kSteps, 3 * kSteps, 12 * kSteps);
    EXPECT_TRUE(delegate->swipe_down());
    delegate->Reset();
  }
}

TEST_F(GestureRecognizerTest, TwoFingerTapCancelled) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  TimedEvents tes;

  // Test canceling first finger.
  {
    gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
    std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
        delegate.get(), -1234, bounds, root_window()));

    delegate->Reset();
    ui::TouchEvent press1(
        ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&press1);

    delegate->Reset();
    ui::TouchEvent press2(
        ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&press2);

    delegate->Reset();
    ui::TouchEvent cancel(
        ui::ET_TOUCH_CANCELLED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&cancel);
    EXPECT_FALSE(delegate->two_finger_tap());

    // Make sure there is enough delay before the touch is released so that it
    // is recognized as a tap.
    delegate->Reset();
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));

    DispatchEventUsingWindowDispatcher(&release);
    EXPECT_FALSE(delegate->two_finger_tap());
  }

  // Test canceling second finger
  {
    gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
    std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
        delegate.get(), -1234, bounds, root_window()));

    delegate->Reset();
    ui::TouchEvent press1(
        ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));
    DispatchEventUsingWindowDispatcher(&press1);

    delegate->Reset();
    ui::TouchEvent press2(
        ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&press2);

    delegate->Reset();
    ui::TouchEvent cancel(
        ui::ET_TOUCH_CANCELLED, gfx::Point(130, 201), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&cancel);
    EXPECT_FALSE(delegate->two_finger_tap());

    // Make sure there is enough delay before the touch is released so that it
    // is recognized as a tap.
    delegate->Reset();
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId1));

    DispatchEventUsingWindowDispatcher(&release);
    EXPECT_FALSE(delegate->two_finger_tap());
  }
}

TEST_F(GestureRecognizerTest, VeryWideTwoFingerTouchDownShouldBeAPinch) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 523;
  const int kWindowHeight = 45;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->long_press());
  EXPECT_FALSE(delegate->two_finger_tap());

  delegate->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(430, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());  // no touch down for second tap.
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->long_press());
  EXPECT_FALSE(delegate->two_finger_tap());
  EXPECT_FALSE(delegate->pinch_begin());

  delegate->Reset();
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(530, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  // Pinch & Scroll only when there is enough movement.
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->long_press());
  EXPECT_FALSE(delegate->two_finger_tap());
  EXPECT_TRUE(delegate->pinch_begin());
}

// Verifies if a window is the target of multiple touch-ids and we hide the
// window everything is cleaned up correctly.
TEST_F(GestureRecognizerTest, FlushAllOnHide) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowWithDelegate(delegate.get(), 0, bounds, root_window()));
  const int kTouchId1 = 8;
  const int kTouchId2 = 2;
  TimedEvents tes;

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(20, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  window->Hide();
  EXPECT_EQ(
      NULL,
      aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
          press1));
  EXPECT_EQ(
      NULL,
      aura::Env::GetInstance()->gesture_recognizer()->GetTouchLockedTarget(
          press2));
}

TEST_F(GestureRecognizerTest, LongPressTimerStopsOnPreventDefaultedTouchMoves) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  // Scroll around, to cancel the long press
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->tap_down());

  // Wait long enough that long press would have fired if the touchmove hadn't
  // prevented it.
  DelayByLongPressTimeout();

  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  EXPECT_FALSE(delegate->long_press());
}

// Same as GestureEventConsumeDelegate, but consumes all the touch-move events.
class ConsumesTouchMovesDelegate : public GestureEventConsumeDelegate {
 public:
  ConsumesTouchMovesDelegate() : consume_touch_move_(true) {}
  ~ConsumesTouchMovesDelegate() override {}

  void set_consume_touch_move(bool consume) { consume_touch_move_ = consume; }

 private:
  void OnTouchEvent(ui::TouchEvent* touch) override {
    if (consume_touch_move_ && touch->type() == ui::ET_TOUCH_MOVED)
      touch->SetHandled();
    else
      GestureEventConsumeDelegate::OnTouchEvent(touch);
  }

  bool consume_touch_move_;

  DISALLOW_COPY_AND_ASSIGN(ConsumesTouchMovesDelegate);
};

// Same as GestureEventScroll, but tests that the behavior is the same
// even if all the touch-move events are consumed.
TEST_F(GestureRecognizerTest, GestureEventScrollTouchMoveConsumed) {
  std::unique_ptr<ConsumesTouchMovesDelegate> delegate(
      new ConsumesTouchMovesDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 5;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it would normally be considered a
  // scroll. But since the touch-moves will be consumed, the scroll should not
  // start.
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  EXPECT_TRUE(delegate->scroll_begin());

  // Release the touch back at the start point. This should end without causing
  // a tap.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(130, 230), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());

  EXPECT_TRUE(delegate->scroll_end());
}

// Tests the behavior of 2F scroll when some of the touch-move events are
// consumed.
TEST_F(GestureRecognizerTest, GestureEventScrollTwoFingerTouchMoveConsumed) {
  std::unique_ptr<ConsumesTouchMovesDelegate> delegate(
      new ConsumesTouchMovesDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 100;
  const int kTouchId1 = 2;
  const int kTouchId2 = 3;
  TimedEvents tes;

  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  tes.SendScrollEvent(event_sink(), 131, 231, kTouchId1, delegate.get());

  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_SCROLL_BEGIN);

  delegate->Reset();
  // Second finger touches down and moves.
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(130, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  tes.SendScrollEvent(event_sink(), 161, 231, kTouchId2, delegate.get());
  EXPECT_0_EVENTS(delegate->events());

  delegate->Reset();
  // Move first finger again, no PinchUpdate & ScrollUpdate.
  tes.SendScrollEvent(event_sink(), 161, 261, kTouchId1, delegate.get());
  EXPECT_0_EVENTS(delegate->events());

  // Stops consuming touch-move.
  delegate->set_consume_touch_move(false);

  delegate->Reset();
  // Making a pinch gesture.
  tes.SendScrollEvent(event_sink(), 161, 260, kTouchId1, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);

  delegate->Reset();
  tes.SendScrollEvent(event_sink(), 161, 261, kTouchId2, delegate.get());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(130, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&release1);
  DispatchEventUsingWindowDispatcher(&release2);

  EXPECT_3_EVENTS(delegate->events(),
                  ui::ET_GESTURE_END,
                  ui::ET_SCROLL_FLING_START,
                  ui::ET_GESTURE_END);
}

// Like as GestureEventTouchMoveConsumed but tests the different behavior
// depending on whether the events were consumed before or after the scroll
// started.
TEST_F(GestureRecognizerTest, GestureEventScrollTouchMovePartialConsumed) {
  std::unique_ptr<ConsumesTouchMovesDelegate> delegate(
      new ConsumesTouchMovesDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 5;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it would normally be considered a
  // scroll. But since the touch-moves will be consumed, the scroll should not
  // start.
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Consuming the first touch move event won't prevent all future scrolling.
  EXPECT_TRUE(delegate->scroll_begin());

  // Now, stop consuming touch-move events, and move the touch-point again.
  delegate->set_consume_touch_move(false);
  tes.SendScrollEvent(event_sink(), 159, 259, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_end());

  // Scroll not prevented by consumed first touch move.
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(29, delegate->scroll_x());
  EXPECT_EQ(29, delegate->scroll_y());
  EXPECT_EQ(gfx::Point(0, 0).ToString(),
            delegate->scroll_begin_position().ToString());

  // Start consuming touch-move events again.
  delegate->set_consume_touch_move(true);

  // Move some more to generate a few more scroll updates.
  tes.SendScrollEvent(event_sink(), 110, 211, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_y());

  tes.SendScrollEvent(event_sink(), 140, 215, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_y());

  // Release the touch.
  delegate->Reset();
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->fling());

  EXPECT_TRUE(delegate->scroll_end());
}

// Check that appropriate touch events generate double tap gesture events.
TEST_F(GestureRecognizerTest, GestureEventDoubleTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  // First tap (tested in GestureEventTap)
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(104, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(104, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  delegate->Reset();

  // Second tap
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 203), tes.LeapForward(200),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press2);
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(102, 206), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release2);

  EXPECT_TRUE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  EXPECT_EQ(2, delegate->tap_count());
}

// Check that appropriate touch events generate triple tap gesture events.
TEST_F(GestureRecognizerTest, GestureEventTripleTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  // First tap (tested in GestureEventTap)
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(104, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(104, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);

  EXPECT_EQ(1, delegate->tap_count());
  delegate->Reset();

  // Second tap (tested in GestureEventDoubleTap)
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 203), tes.LeapForward(200),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press2);
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(102, 206), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release2);

  EXPECT_EQ(2, delegate->tap_count());
  delegate->Reset();

  // Third tap
  ui::TouchEvent press3(
      ui::ET_TOUCH_PRESSED, gfx::Point(102, 206), tes.LeapForward(200),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press3);
  ui::TouchEvent release3(
      ui::ET_TOUCH_RELEASED, gfx::Point(102, 206), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release3);

  // Third, Fourth and Fifth Taps. Taps after the third should have their
  // |tap_count| wrap around back to 1.
  for (int i = 3; i < 5; ++i) {
    ui::TouchEvent press3(
        ui::ET_TOUCH_PRESSED, gfx::Point(102, 206), tes.LeapForward(200),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&press3);
    ui::TouchEvent release3(
        ui::ET_TOUCH_RELEASED, gfx::Point(102, 206), tes.LeapForward(50),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&release3);

    EXPECT_TRUE(delegate->tap());
    EXPECT_TRUE(delegate->tap_down());
    EXPECT_FALSE(delegate->tap_cancel());
    EXPECT_TRUE(delegate->begin());
    EXPECT_TRUE(delegate->end());
    EXPECT_FALSE(delegate->scroll_begin());
    EXPECT_FALSE(delegate->scroll_update());
    EXPECT_FALSE(delegate->scroll_end());
    EXPECT_EQ(1 + (i % 3), delegate->tap_count());
  }
}

// Check that we don't get a double tap when the two taps are far apart.
TEST_F(GestureRecognizerTest, TwoTapsFarApart) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  // First tap (tested in GestureEventTap)
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  delegate->Reset();

  // Second tap, close in time but far in distance
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(201, 201), tes.LeapForward(200),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press2);
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(201, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release2);

  EXPECT_TRUE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  EXPECT_EQ(1, delegate->tap_count());
}

// Check that we don't get a double tap when the two taps have a long enough
// delay in between.
TEST_F(GestureRecognizerTest, TwoTapsWithDelayBetween) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  // First tap (tested in GestureEventTap)
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  delegate->Reset();

  // Second tap, close in distance but after some delay
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.LeapForward(2000),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press2);
  ui::TouchEvent release2(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release2);

  EXPECT_TRUE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->begin());
  EXPECT_TRUE(delegate->end());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  EXPECT_EQ(1, delegate->tap_count());
}

// Checks that if the bounding-box of a gesture changes because of change in
// radius of a touch-point, and not because of change in position, then there
// are not gesture events from that.
TEST_F(GestureRecognizerTest, BoundingBoxRadiusChange) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 234;
  const int kWindowHeight = 345;
  const int kTouchId = 5, kTouchId2 = 7;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->bounding_box().IsEmpty());

  delegate->Reset();

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(201, 201), tes.LeapForward(400),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  SetTouchRadius(&press2, 5, 0);
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_EQ(gfx::Rect(101, 196, 105, 10).ToString(),
            delegate->bounding_box().ToString());

  delegate->Reset();

  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(50, 50), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  EXPECT_TRUE(delegate->pinch_begin());
  EXPECT_EQ(gfx::Rect(50, 50, 156, 156).ToString(),
            delegate->bounding_box().ToString());

  delegate->Reset();

  // The position doesn't move, but the radius changes.
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(50, 50), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  SetTouchRadius(&move2, 50, 60);
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->pinch_update());

  delegate->Reset();
}

// Checks that slow scrolls deliver the correct deltas.
// In particular, fix for http;//crbug.com/150573.
TEST_F(GestureRecognizerTest, NoDriftInScroll) {
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(3);
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 234;
  const int kWindowHeight = 345;
  const int kTouchId = 5;
  TimedEvents tes;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 208), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->begin());

  delegate->Reset();

  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(101, 206), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  EXPECT_FALSE(delegate->scroll_begin());

  delegate->Reset();

  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(101, 204), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  // 3 px consumed by touch slop region.
  EXPECT_EQ(-1, delegate->scroll_y());
  EXPECT_EQ(-1, delegate->scroll_y_hint());

  delegate->Reset();

  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(101, 204), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move3);
  EXPECT_FALSE(delegate->scroll_update());

  delegate->Reset();

  ui::TouchEvent move4(
      ui::ET_TOUCH_MOVED, gfx::Point(101, 203), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move4);
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(-1, delegate->scroll_y());

  delegate->Reset();
}

// Ensure that move events which are preventDefaulted will cause a tap
// cancel gesture event to be fired if the move would normally cause a
// scroll. See bug http://crbug.com/146397.
TEST_F(GestureRecognizerTest, GestureEventConsumedTouchMoveCanFireTapCancel) {
  std::unique_ptr<ConsumesTouchMovesDelegate> delegate(
      new ConsumesTouchMovesDelegate());
  const int kTouchId = 5;
  gfx::Rect bounds(100, 200, 123, 45);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  TimedEvents tes;

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));

  delegate->set_consume_touch_move(false);
  DispatchEventUsingWindowDispatcher(&press);
  delegate->set_consume_touch_move(true);
  delegate->Reset();
  // Move the touch-point enough so that it would normally be considered a
  // scroll. But since the touch-moves will be consumed, no scrolling should
  // occur.
  // With the unified gesture detector, we will receive a scroll begin gesture,
  // whereas with the aura gesture recognizer we won't.
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_TRUE(delegate->tap_cancel());
  EXPECT_FALSE(delegate->begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
}

TEST_F(GestureRecognizerTest, CancelAllActiveTouches) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 800;
  const int kWindowHeight = 600;
  const int kTouchId1 = 1;
  const int kTouchId2 = 2;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  std::unique_ptr<TestEventHandler> handler(new TestEventHandler());
  window->AddPreTargetHandler(handler.get());

  // Start a gesture sequence on |window|. Then cancel all touches.
  // Make sure |window| receives a touch-cancel event.
  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_TAP_DOWN);
  delegate->Reset();
  ui::TouchEvent p2(
      ui::ET_TOUCH_PRESSED, gfx::Point(50, 50), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&p2);
  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_TAP_CANCEL, ui::ET_GESTURE_BEGIN);
  delegate->Reset();
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(350, 300), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_4_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE, ui::ET_GESTURE_PINCH_BEGIN,
                  ui::ET_GESTURE_PINCH_UPDATE);
  EXPECT_EQ(2, handler->touch_pressed_count());
  delegate->Reset();
  handler->Reset();

  ui::GestureRecognizer* gesture_recognizer =
      aura::Env::GetInstance()->gesture_recognizer();
  EXPECT_EQ(window.get(),
            gesture_recognizer->GetTouchLockedTarget(press));

  aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
      nullptr);

  EXPECT_EQ(NULL, gesture_recognizer->GetTouchLockedTarget(press));
  EXPECT_4_EVENTS(delegate->events(),
                  ui::ET_GESTURE_PINCH_END,
                  ui::ET_GESTURE_SCROLL_END,
                  ui::ET_GESTURE_END,
                  ui::ET_GESTURE_END);
  const std::vector<gfx::PointF>& points = handler->cancelled_touch_points();
  EXPECT_EQ(2U, points.size());
  EXPECT_EQ(gfx::PointF(101.f, 201.f), points[0]);
  EXPECT_EQ(gfx::PointF(350.f, 300.f), points[1]);
  window->RemovePreTargetHandler(handler.get());
}

// Check that appropriate touch events generate show press events
TEST_F(GestureRecognizerTest, GestureEventShowPress) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_TRUE(delegate->begin());
  EXPECT_FALSE(delegate->tap_cancel());

  // We haven't pressed long enough for a show press to occur
  EXPECT_FALSE(delegate->show_press());

  // Wait until the timer runs out
  delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->long_press());

  // Note the tap isn't dispatched until the release
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->tap());
}

// Check that scrolling cancels a show press
TEST_F(GestureRecognizerTest, GestureEventShowPressCancelledByScroll) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 6;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a show press to occur
  EXPECT_FALSE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_cancel());

  // Scroll around, to cancel the show press
  tes.SendScrollEvent(event_sink(), 130, 230, kTouchId, delegate.get());
  // Wait until the timer runs out
  DelayByShowPressTimeout();
  EXPECT_FALSE(delegate->show_press());
  EXPECT_TRUE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(10),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_FALSE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_cancel());
}

// Test that show press events are sent immediately on tap
TEST_F(GestureRecognizerTest, GestureEventShowPressSentOnTap) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 6;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a show press to occur
  EXPECT_FALSE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_cancel());

  delegate->Reset();
  ui::TouchEvent release1(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release1);
  EXPECT_TRUE(delegate->show_press());
  EXPECT_FALSE(delegate->tap_cancel());
  EXPECT_TRUE(delegate->tap());
}

// Test that consuming the first move touch event prevents a scroll.
TEST_F(GestureRecognizerTest, GestureEventConsumedTouchMoveScrollTest) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  delegate->ReceivedAck();

  // A touch move within the slop region is never consumed in web contents. The
  // unified GR won't prevent scroll if a touch move within the slop region is
  // consumed, so make sure this touch move exceeds the slop region.
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  delegate->ReceivedAckPreventDefaulted();

  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(20, 20), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move2);
  delegate->ReceivedAck();

    // With the unified gesture detector, consuming the first touch move event
    // won't prevent all future scrolling.
    EXPECT_TRUE(delegate->scroll_begin());
    EXPECT_TRUE(delegate->scroll_update());
}

// Test that consuming the first move touch doesn't prevent a tap.
TEST_F(GestureRecognizerTest, GestureEventConsumedTouchMoveTapTest) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  delegate->ReceivedAck();

  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(2, 2), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  delegate->ReceivedAckPreventDefaulted();

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(2, 2), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);
  delegate->ReceivedAck();

  EXPECT_TRUE(delegate->tap());
}

// Test that consuming the first move touch doesn't prevent a long press.
TEST_F(GestureRecognizerTest, GestureEventConsumedTouchMoveLongPressTest) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  delegate->ReceivedAck();

  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(103, 203), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move);
  delegate->ReceivedAckPreventDefaulted();

  // Wait until the timer runs out
  delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(delegate->long_press());
}

// Tests that the deltas are correct when leaving the slop region very slowly.
TEST_F(GestureRecognizerTest, TestExceedingSlopSlowly) {
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(3);
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 234;
  const int kWindowHeight = 345;
  const int kTouchId = 5;
  TimedEvents tes;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  delegate->Reset();

  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(11, 10), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_x_hint());
  delegate->Reset();

  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(12, 10), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_x_hint());
  delegate->Reset();

  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  move3.set_location_f(gfx::PointF(13.1f, 10.f));
  move3.set_root_location_f(gfx::PointF(13.1f, 10.f));
  DispatchEventUsingWindowDispatcher(&move3);
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_NEAR(0.1, delegate->scroll_x(), 0.0001);
  EXPECT_NEAR(0.1, delegate->scroll_x_hint(), 0.0001);
  delegate->Reset();

  ui::TouchEvent move4(
      ui::ET_TOUCH_MOVED, gfx::Point(14, 10), tes.LeapForward(40),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move4);
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_NEAR(0.9, delegate->scroll_x(), 0.0001);
  EXPECT_EQ(0.f, delegate->scroll_x_hint());
  delegate->Reset();
}

TEST_F(GestureRecognizerTest, ScrollAlternatelyConsumedTest) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 3000;
  const int kWindowHeight = 3000;
  const int kTouchId = 2;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  delegate->Reset();

  int x = 0;
  int y = 0;

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(x, y), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  delegate->ReceivedAck();
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  delegate->Reset();

  x += 100;
  y += 100;
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&move1);
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  delegate->Reset();

  for (int i = 0; i < 3; ++i) {
    x += 10;
    y += 10;
    ui::TouchEvent move2(
        ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&move2);
    delegate->ReceivedAck();
    EXPECT_FALSE(delegate->scroll_begin());
    EXPECT_TRUE(delegate->scroll_update());
    EXPECT_EQ(10, delegate->scroll_x());
    EXPECT_EQ(10, delegate->scroll_y());
    delegate->Reset();

    x += 20;
    y += 20;
    ui::TouchEvent move3(
        ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
    DispatchEventUsingWindowDispatcher(&move3);
    delegate->ReceivedAckPreventDefaulted();
    EXPECT_FALSE(delegate->scroll_begin());
    EXPECT_FALSE(delegate->scroll_update());
    delegate->Reset();
  }
}

TEST_F(GestureRecognizerTest, PinchAlternatelyConsumedTest) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 3000;
  const int kWindowHeight = 3000;
  const int kTouchId1 = 5;
  const int kTouchId2 = 7;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());
  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  delegate->ReceivedAck();
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  delegate->Reset();

  int x = 0;
  int y = 0;

  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(x, y), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  delegate->ReceivedAck();
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_FALSE(delegate->pinch_update());

  delegate->Reset();

  x += 100;
  y += 100;
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&move1);
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_TRUE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());
  delegate->Reset();

  const float expected_scales[] = {1.5f, 1.2f, 1.125f};

  for (int i = 0; i < 3; ++i) {
    x += 50;
    y += 50;
    ui::TouchEvent move2(
        ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&move2);
    delegate->ReceivedAck();
    EXPECT_FALSE(delegate->scroll_begin());
    EXPECT_TRUE(delegate->scroll_update());
    EXPECT_FALSE(delegate->scroll_end());
    EXPECT_FALSE(delegate->pinch_begin());
    EXPECT_TRUE(delegate->pinch_update());
    EXPECT_FALSE(delegate->pinch_end());
    EXPECT_EQ(25, delegate->scroll_x());
    EXPECT_EQ(25, delegate->scroll_y());
    EXPECT_FLOAT_EQ(expected_scales[i], delegate->scale());
    delegate->Reset();

    x += 100;
    y += 100;
    ui::TouchEvent move3(
        ui::ET_TOUCH_MOVED, gfx::Point(x, y), tes.Now(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           kTouchId2));
    DispatchEventUsingWindowDispatcher(&move3);
    delegate->ReceivedAckPreventDefaulted();
    EXPECT_FALSE(delegate->scroll_begin());
    EXPECT_FALSE(delegate->scroll_update());
    EXPECT_FALSE(delegate->scroll_end());
    EXPECT_FALSE(delegate->pinch_begin());
    EXPECT_FALSE(delegate->pinch_update());
    EXPECT_FALSE(delegate->pinch_end());
    delegate->Reset();
  }
}

// Test that touch event flags are passed through to the gesture event.
TEST_F(GestureRecognizerTest, GestureEventFlagsPassedFromTouchEvent) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 6;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->Reset();

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(delegate->tap_down());

  int default_flags = delegate->flags();

  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(397, 149), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  move1.set_flags(992);

  DispatchEventUsingWindowDispatcher(&move1);
  EXPECT_NE(default_flags, delegate->flags());
}

// A delegate that deletes a window on long press.
class GestureEventDeleteWindowOnLongPress : public GestureEventConsumeDelegate {
 public:
  GestureEventDeleteWindowOnLongPress()
      : window_(NULL) {}

  void set_window(aura::Window** window) { window_ = window; }

  void OnGestureEvent(ui::GestureEvent* gesture) override {
    GestureEventConsumeDelegate::OnGestureEvent(gesture);
    if (gesture->type() != ui::ET_GESTURE_LONG_PRESS)
      return;
    delete *window_;
    *window_ = NULL;
  }

 private:
  aura::Window** window_;
  DISALLOW_COPY_AND_ASSIGN(GestureEventDeleteWindowOnLongPress);
};

// Check that deleting the window in response to a long press gesture doesn't
// crash.
TEST_F(GestureRecognizerTest, GestureEventLongPressDeletingWindow) {
  GestureEventDeleteWindowOnLongPress delegate;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  aura::Window* window(CreateTestWindowWithDelegate(
      &delegate, -1234, bounds, root_window()));
  delegate.set_window(&window);

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press1);
  EXPECT_TRUE(window != NULL);

  // Wait until the timer runs out.
  delegate.WaitUntilReceivedGesture(ui::ET_GESTURE_LONG_PRESS);
  EXPECT_EQ(NULL, window);
}

TEST_F(GestureRecognizerWithSwitchTest, GestureEventSmallPinchDisabled) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  const int kTouchId2 = 5;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);

  // Move the first finger.
  delegate->Reset();
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move1);

  EXPECT_4_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE, ui::ET_GESTURE_PINCH_BEGIN,
                  ui::ET_GESTURE_PINCH_UPDATE);

  // No pinch update occurs, as kCompensateForUnstablePinchZoom is on and
  // |min_pinch_update_span_delta| was nonzero, and this is a very small pinch.
  delegate->Reset();
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
}

TEST_F(GestureRecognizerTest, GestureEventSmallPinchEnabled) {
  std::unique_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  const int kTouchId2 = 5;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);

  // Move the first finger.
  delegate->Reset();
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move1);

  EXPECT_4_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE, ui::ET_GESTURE_PINCH_BEGIN,
                  ui::ET_GESTURE_PINCH_UPDATE);

  delegate->Reset();
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_2_EVENTS(delegate->events(),
                  ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_PINCH_UPDATE);
}

// Tests that delaying the ack of a touch release doesn't trigger a long press
// gesture.
TEST_F(GestureRecognizerTest, EagerGestureDetection) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  delegate->Reset();
  // Ack the touch press.
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->tap_down());

  delegate->Reset();
  // Wait until the long press event would fire (if we weren't eager).
  DelayByLongPressTimeout();

  // Ack the touch release.
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->tap());
  EXPECT_FALSE(delegate->long_press());
}

// This tests crbug.com/405519, in which touch events which the gesture detector
// ignores interfere with gesture recognition.
TEST_F(GestureRecognizerTest, IgnoredEventsDontBreakGestureRecognition) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(101, 301), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  delegate->ReceivedAck();

  EXPECT_2_EVENTS(
      delegate->events(), ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_TAP_DOWN);

  // Move the first finger.
  delegate->Reset();
  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move1);
  delegate->ReceivedAck();

  EXPECT_3_EVENTS(delegate->events(),
                  ui::ET_GESTURE_TAP_CANCEL,
                  ui::ET_GESTURE_SCROLL_BEGIN,
                  ui::ET_GESTURE_SCROLL_UPDATE);

  delegate->Reset();

  // Send a valid event, but don't ack it.
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_0_EVENTS(delegate->events());

  // Send a touchmove event at the same location as the previous touchmove
  // event. This shouldn't do anything.
  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(65, 202), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move3);

  // Ack the previous valid event. The intermediary invalid event shouldn't
  // interfere.
  delegate->ReceivedAck();
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);
}

// Tests that an event stream can have a mix of sync and async acks.
TEST_F(GestureRecognizerTest,
       MixedSyncAndAsyncAcksDontCauseOutOfOrderDispatch) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));
  delegate->set_window(window.get());

  // Start a scroll gesture.
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(0, 0), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press1);
  delegate->ReceivedAck();

  ui::TouchEvent move1(
      ui::ET_TOUCH_MOVED, gfx::Point(100, 100), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move1);
  delegate->ReceivedAck();

  delegate->Reset();
  // Dispatch a synchronously consumed touch move, which should be ignored.
  delegate->set_synchronous_ack_for_next_event(true);
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(200, 200), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_0_EVENTS(delegate->events());

  // Dispatch a touch move, but don't ack it.
  ui::TouchEvent move3(
      ui::ET_TOUCH_MOVED, gfx::Point(300, 300), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move3);

  // Dispatch two synchronously consumed touch moves, which should be ignored.
  delegate->set_synchronous_ack_for_next_event(true);
  ui::TouchEvent move4(
      ui::ET_TOUCH_MOVED, gfx::Point(400, 400), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move4);

  delegate->set_synchronous_ack_for_next_event(true);
  ui::TouchEvent move5(
      ui::ET_TOUCH_MOVED, gfx::Point(500, 500), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move5);

  EXPECT_0_EVENTS(delegate->events());
  EXPECT_EQ(100, delegate->bounding_box().x());
  // Ack the pending touch move, and ensure the most recent gesture event
  // used its co-ordinates.
  delegate->ReceivedAck();
  EXPECT_EQ(300, delegate->bounding_box().x());
  EXPECT_1_EVENT(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE);

  // Dispatch a touch move, but don't ack it.
  delegate->Reset();
  ui::TouchEvent move6(
      ui::ET_TOUCH_MOVED, gfx::Point(600, 600), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move6);

  // Dispatch a synchronously unconsumed touch move.
  delegate->set_synchronous_ack_for_next_event(false);
  ui::TouchEvent move7(
      ui::ET_TOUCH_MOVED, gfx::Point(700, 700), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&move7);

  // The synchronous ack is stuck behind the pending touch move.
  EXPECT_0_EVENTS(delegate->events());

  delegate->ReceivedAck();
  EXPECT_2_EVENTS(delegate->events(), ui::ET_GESTURE_SCROLL_UPDATE,
                  ui::ET_GESTURE_SCROLL_UPDATE);
}

TEST_F(GestureRecognizerTest, GestureEventTwoWindowsActive) {
  std::unique_ptr<QueueTouchEventDelegate> queued_delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 6;
  const int kTouchId2 = 4;
  gfx::Rect bounds(150, 200, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      queued_delegate.get(), -1234, bounds, root_window()));
  queued_delegate->set_window(window.get());

  // Touch down on the window. This should not generate any gesture event.
  queued_delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(151, 201), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId1));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_FALSE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Touch down on the second window. This should not generate any
  // gesture event.
  std::unique_ptr<QueueTouchEventDelegate> queued_delegate2(
      new QueueTouchEventDelegate(host()->dispatcher()));
  gfx::Rect bounds2(0, 0, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window2(CreateTestWindowWithDelegate(
      queued_delegate2.get(), -2345, bounds2, root_window()));
  queued_delegate2->set_window(window2.get());

  queued_delegate2->Reset();
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(1, 1), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId2));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_FALSE(queued_delegate2->tap());
  EXPECT_FALSE(queued_delegate2->tap_down());
  EXPECT_FALSE(queued_delegate2->tap_cancel());
  EXPECT_FALSE(queued_delegate2->begin());
  EXPECT_FALSE(queued_delegate2->scroll_begin());
  EXPECT_FALSE(queued_delegate2->scroll_update());
  EXPECT_FALSE(queued_delegate2->scroll_end());

  // Ack the first window's touch; make sure it is processed by the first
  // window.
  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->show_press());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->tap_cancel());
  EXPECT_TRUE(queued_delegate->begin());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());
  EXPECT_FALSE(queued_delegate->long_press());

  // Ack the second window's touch; make sure it is processed by the second
  // window.
  queued_delegate2->Reset();
  queued_delegate2->ReceivedAck();
  EXPECT_FALSE(queued_delegate2->tap());
  EXPECT_FALSE(queued_delegate2->show_press());
  EXPECT_TRUE(queued_delegate2->tap_down());
  EXPECT_FALSE(queued_delegate2->tap_cancel());
  EXPECT_TRUE(queued_delegate2->begin());
  EXPECT_FALSE(queued_delegate2->scroll_begin());
  EXPECT_FALSE(queued_delegate2->scroll_update());
  EXPECT_FALSE(queued_delegate2->scroll_end());
  EXPECT_FALSE(queued_delegate2->long_press());

  queued_delegate->Reset();
  queued_delegate->WaitUntilReceivedGesture(ui::ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(queued_delegate->show_press());
  EXPECT_FALSE(queued_delegate->tap_down());
}

// Test for crbug/698843. Checks whether the events are routed to the correct
// consumer in the event of TransferEventsTo() function call.
TEST_F(GestureRecognizerTest, TransferEventsToRoutesAckCorrectly) {
  std::unique_ptr<QueueTouchEventDelegate> delegate_1(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);

  std::unique_ptr<aura::Window> window_1(CreateTestWindowWithDelegate(
      delegate_1.get(), -1234, bounds, root_window()));

  delegate_1->set_window(window_1.get());

  delegate_1->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(512, 512), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  // Create a new consumer and Touch event delegate.
  std::unique_ptr<QueueTouchEventDelegate> delegate_2(
      new QueueTouchEventDelegate(host()->dispatcher()));
  std::unique_ptr<aura::Window> window_2(CreateTestWindowWithDelegate(
      delegate_2.get(), -2345, bounds, root_window()));
  delegate_2->set_window(window_2.get());

  // Transfer event sequence from previous window to the new window.
  aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
      window_1.get(), window_2.get(), ui::TransferTouchesBehavior::kDontCancel);

  delegate_1->Reset();
  delegate_1->ReceivedAck();

  // ACK for events that were dispatched before the transfer should go to the
  // original consumer. See crbug/698843 for more details.
  EXPECT_2_EVENTS(delegate_1->events(), ui::ET_GESTURE_BEGIN,
                  ui::ET_GESTURE_TAP_DOWN);

  delegate_1->Reset();

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(550, 512), tes.LeapForward(50),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&release);

  // Events dispatched after the transfer should go to the new window.
  EXPECT_0_EVENTS(delegate_1->events());

  delegate_2->ReceivedAck();

  // The event sequence transfer should mean that the new window receives the
  // gesture sequence state.
  EXPECT_3_EVENTS(delegate_2->events(), ui::ET_GESTURE_SHOW_PRESS,
                  ui::ET_GESTURE_TAP, ui::ET_GESTURE_END);

  EXPECT_TRUE(delegate_2->tap());
}

TEST_F(GestureRecognizerTest, GestureConsumerCleanupBeforeTouchAck) {
  std::unique_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(host()->dispatcher()));
  TimedEvents tes;
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);

  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, root_window()));

  delegate->set_window(window.get());

  delegate->Reset();
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(512, 512), tes.Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, kTouchId));
  DispatchEventUsingWindowDispatcher(&press);

  window->Hide();

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_0_EVENTS(delegate->events());
}

}  // namespace test
}  // namespace aura
