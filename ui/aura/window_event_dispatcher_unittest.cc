// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_event_dispatcher.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/wm/core/capture_controller.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif

namespace aura {
namespace {

// A delegate that always returns a non-client component for hit tests.
class NonClientDelegate : public test::TestWindowDelegate {
 public:
  NonClientDelegate()
      : non_client_count_(0), mouse_event_count_(0), mouse_event_flags_(0x0) {}

  NonClientDelegate(const NonClientDelegate&) = delete;
  NonClientDelegate& operator=(const NonClientDelegate&) = delete;

  ~NonClientDelegate() override = default;

  int non_client_count() const { return non_client_count_; }
  const gfx::Point& non_client_location() const { return non_client_location_; }
  int mouse_event_count() const { return mouse_event_count_; }
  const gfx::Point& mouse_event_location() const {
    return mouse_event_location_;
  }
  int mouse_event_flags() const { return mouse_event_flags_; }

  int GetNonClientComponent(const gfx::Point& location) const override {
    NonClientDelegate* self = const_cast<NonClientDelegate*>(this);
    self->non_client_count_++;
    self->non_client_location_ = location;
    return HTMENU;
  }
  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_event_count_++;
    mouse_event_location_ = event->location();
    mouse_event_flags_ = event->flags();
    event->SetHandled();
  }

 private:
  int non_client_count_;
  gfx::Point non_client_location_;
  int mouse_event_count_;
  gfx::Point mouse_event_location_;
  int mouse_event_flags_;
};

// A simple event handler that consumes key events.
class ConsumeKeyHandler : public ui::test::TestEventHandler {
 public:
  ConsumeKeyHandler() {}

  ConsumeKeyHandler(const ConsumeKeyHandler&) = delete;
  ConsumeKeyHandler& operator=(const ConsumeKeyHandler&) = delete;

  ~ConsumeKeyHandler() override {}

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    ui::test::TestEventHandler::OnKeyEvent(event);
    event->StopPropagation();
  }
};

// ImeKeyEventDispatcher that tracks the events passed to PostIME phase.
class TestImeKeyEventDispatcher : public ui::ImeKeyEventDispatcher {
 public:
  TestImeKeyEventDispatcher() = default;
  ~TestImeKeyEventDispatcher() override = default;

  // ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    ++dispatched_event_count_;
    return ui::EventDispatchDetails();
  }

  int dispatched_event_count() const { return dispatched_event_count_; }

 private:
  int dispatched_event_count_{0};
};

bool IsFocusedWindow(aura::Window* window) {
  return client::GetFocusClient(window)->GetFocusedWindow() == window;
}

gfx::Point GetLastTouchPoint(
    aura::Window* window,
    std::optional<gfx::Point> fallback = std::nullopt) {
  return Env::GetInstance()->GetLastPointerPoint(
      ui::mojom::DragEventSource::kTouch, window, fallback);
}

}  // namespace

using WindowEventDispatcherTest = test::AuraTestBase;

TEST_F(WindowEventDispatcherTest, OnHostMouseEvent) {
  // Create two non-overlapping windows so we don't have to worry about which
  // is on top.
  std::unique_ptr<NonClientDelegate> delegate1(new NonClientDelegate());
  std::unique_ptr<NonClientDelegate> delegate2(new NonClientDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds1(100, 200, kWindowWidth, kWindowHeight);
  gfx::Rect bounds2(300, 400, kWindowWidth, kWindowHeight);
  std::unique_ptr<aura::Window> window1(CreateTestWindowWithDelegate(
      delegate1.get(), -1234, bounds1, root_window()));
  std::unique_ptr<aura::Window> window2(CreateTestWindowWithDelegate(
      delegate2.get(), -5678, bounds2, root_window()));

  // Send a mouse event to window1.
  gfx::Point point(101, 201);
  ui::MouseEvent event1(ui::EventType::kMousePressed, point, point,
                        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                        ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(&event1);

  EXPECT_EQ(gfx::Point(101, 201), Env::GetInstance()->last_mouse_location());

  // Event was tested for non-client area for the target window.
  EXPECT_EQ(1, delegate1->non_client_count());
  EXPECT_EQ(0, delegate2->non_client_count());
  // The non-client component test was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->non_client_location());
  // Mouse event was received by target window.
  EXPECT_EQ(1, delegate1->mouse_event_count());
  EXPECT_EQ(0, delegate2->mouse_event_count());
  // Event was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->mouse_event_location());
  // Non-client flag was set.
  EXPECT_TRUE(delegate1->mouse_event_flags() & ui::EF_IS_NON_CLIENT);
}

TEST_F(WindowEventDispatcherTest, RepostEvent) {
  // Test RepostEvent in RootWindow. It only works for Mouse Press and touch
  // press.
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  gfx::Point point(10, 10);
  ui::MouseEvent event(ui::EventType::kMousePressed, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  host()->dispatcher()->RepostEvent(&event);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  ui::TouchEvent touch_pressed_event(
      ui::EventType::kTouchPressed, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  host()->dispatcher()->RepostEvent(&touch_pressed_event);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(Env::GetInstance()->is_touch_down());
}

// Check that we correctly track whether any touch devices are down in response
// to touch press and release events with two WindowTreeHost.
TEST_F(WindowEventDispatcherTest, TouchDownState) {
  std::unique_ptr<WindowTreeHost> second_host = WindowTreeHost::Create(
      ui::PlatformWindowInitProperties{gfx::Rect(20, 30, 100, 50)});
  second_host->InitHost();
  second_host->window()->Show();

  ui::TouchEvent touch_pressed_event1(
      ui::EventType::kTouchPressed, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent touch_pressed_event2(
      ui::EventType::kTouchPressed, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  ui::TouchEvent touch_released_event1(
      ui::EventType::kTouchReleased, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent touch_released_event2(
      ui::EventType::kTouchReleased, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));

  EXPECT_FALSE(Env::GetInstance()->is_touch_down());
  host()->dispatcher()->OnEventFromSource(&touch_pressed_event1);
  EXPECT_TRUE(Env::GetInstance()->is_touch_down());
  second_host->dispatcher()->OnEventFromSource(&touch_pressed_event2);
  EXPECT_TRUE(Env::GetInstance()->is_touch_down());
  host()->dispatcher()->OnEventFromSource(&touch_released_event1);
  EXPECT_TRUE(Env::GetInstance()->is_touch_down());
  second_host->dispatcher()->OnEventFromSource(&touch_released_event2);
  EXPECT_FALSE(Env::GetInstance()->is_touch_down());
}

// Check that we correctly track the state of the mouse buttons in response to
// button press and release events.
TEST_F(WindowEventDispatcherTest, MouseButtonState) {
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  gfx::Point location;
  std::unique_ptr<ui::MouseEvent> event;

  // Press the left button.
  event = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMousePressed, location, location, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Additionally press the right.
  event = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMousePressed, location, location, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
      ui::EF_RIGHT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the left button.
  event = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMouseReleased, location, location, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the right button.  We should ignore the Shift-is-down flag.
  event = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMouseReleased, location, location, ui::EventTimeForNow(),
      ui::EF_SHIFT_DOWN, ui::EF_RIGHT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  // Press the middle button.
  event = std::make_unique<ui::MouseEvent>(
      ui::EventType::kMousePressed, location, location, ui::EventTimeForNow(),
      ui::EF_MIDDLE_MOUSE_BUTTON, ui::EF_MIDDLE_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());
}

TEST_F(WindowEventDispatcherTest, TranslatedEvent) {
  std::unique_ptr<Window> w1(test::CreateTestWindowWithDelegate(
      NULL, 1, gfx::Rect(50, 50, 100, 100), root_window()));

  gfx::Point origin(100, 100);
  ui::MouseEvent root(ui::EventType::kMousePressed, origin, origin,
                      ui::EventTimeForNow(), 0, 0);

  EXPECT_EQ("100,100", root.location().ToString());
  EXPECT_EQ("100,100", root.root_location().ToString());

  ui::MouseEvent translated_event(root, static_cast<Window*>(root_window()),
                                  w1.get(), ui::EventType::kMouseEntered,
                                  root.flags());
  EXPECT_EQ("50,50", translated_event.location().ToString());
  EXPECT_EQ("100,100", translated_event.root_location().ToString());
}

namespace {

class TestEventClient : public client::EventClient {
 public:
  static const int kNonLockWindowId = 100;
  static const int kLockWindowId = 200;

  explicit TestEventClient(Window* root_window)
      : root_window_(root_window), lock_(false) {
    client::SetEventClient(root_window_, this);
    Window* lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    lock_window->SetId(kLockWindowId);
    Window* non_lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    non_lock_window->SetId(kNonLockWindowId);
  }

  TestEventClient(const TestEventClient&) = delete;
  TestEventClient& operator=(const TestEventClient&) = delete;

  ~TestEventClient() override { client::SetEventClient(root_window_, NULL); }

  // Starts/stops locking. Locking prevents windows other than those inside
  // the lock container from receiving events, getting focus etc.
  void Lock() { lock_ = true; }
  void Unlock() { lock_ = false; }

  Window* GetLockWindow() {
    return const_cast<Window*>(
        static_cast<const TestEventClient*>(this)->GetLockWindow());
  }
  const Window* GetLockWindow() const {
    return root_window_->GetChildById(kLockWindowId);
  }
  Window* GetNonLockWindow() {
    return root_window_->GetChildById(kNonLockWindowId);
  }

 private:
  // Overridden from client::EventClient:
  bool GetCanProcessEventsWithinSubtree(const Window* window) const override {
    return lock_ ? window->Contains(GetLockWindow()) ||
                       GetLockWindow()->Contains(window)
                 : true;
  }

  ui::EventTarget* GetToplevelEventTarget() override { return NULL; }

  raw_ptr<Window> root_window_;
  bool lock_;
};

}  // namespace

TEST_F(WindowEventDispatcherTest, GetCanProcessEventsWithinSubtree) {
  TestEventClient client(root_window());
  test::TestWindowDelegate d;

  ui::test::TestEventHandler nonlock_ef;
  ui::test::TestEventHandler lock_ef;
  client.GetNonLockWindow()->AddPreTargetHandler(&nonlock_ef);
  client.GetLockWindow()->AddPreTargetHandler(&lock_ef);

  Window* w1 = test::CreateTestWindowWithBounds(gfx::Rect(10, 10, 20, 20),
                                                client.GetNonLockWindow());
  w1->SetId(1);
  Window* w2 = test::CreateTestWindowWithBounds(gfx::Rect(30, 30, 20, 20),
                                                client.GetNonLockWindow());
  w2->SetId(2);
  std::unique_ptr<Window> w3(test::CreateTestWindowWithDelegate(
      &d, 3, gfx::Rect(30, 30, 20, 20), client.GetLockWindow()));

  w1->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));

  client.Lock();

  // Since we're locked, the attempt to focus w2 will be ignored.
  w2->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));
  EXPECT_FALSE(IsFocusedWindow(w2));

  {
    // Attempting to send a key event to w1 (not in the lock container) should
    // cause focus to be reset.
    ui::test::EventGenerator generator(root_window());
    generator.PressKey(ui::VKEY_SPACE, 0);
    EXPECT_EQ(NULL, client::GetFocusClient(w1)->GetFocusedWindow());
    EXPECT_FALSE(IsFocusedWindow(w1));
  }

  {
    // Events sent to a window not in the lock container will not be processed.
    // i.e. never sent to the non-lock container's event filter.
    ui::test::EventGenerator generator(root_window(), w1);
    generator.ClickLeftButton();
    EXPECT_EQ(0, nonlock_ef.num_mouse_events());

    // Events sent to a window in the lock container will be processed.
    ui::test::EventGenerator generator3(root_window(), w3.get());
    generator3.PressLeftButton();
    EXPECT_EQ(1, lock_ef.num_mouse_events());
  }

  // Prevent w3 from being deleted by the hierarchy since its delegate is owned
  // by this scope.
  w3->parent()->RemoveChild(w3.get());

  client.GetNonLockWindow()->RemovePreTargetHandler(&nonlock_ef);
  client.GetLockWindow()->RemovePreTargetHandler(&lock_ef);
}

TEST_F(WindowEventDispatcherTest, DontIgnoreUnknownKeys) {
  ui::Event::Properties properties;
#if BUILDFLAG(IS_OZONE)
  ui::SetKeyboardImeFlagProperty(&properties,
                                 ui::kPropertyKeyboardImeIgnoredFlag);
#endif

  ConsumeKeyHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  ui::KeyEvent unknown_event(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN,
                             ui::EF_NONE);
  unknown_event.SetProperties(properties);
  DispatchEventUsingWindowDispatcher(&unknown_event);
  EXPECT_TRUE(unknown_event.handled());
  EXPECT_EQ(1, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent known_event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  known_event.SetProperties(properties);
  DispatchEventUsingWindowDispatcher(&known_event);
  EXPECT_TRUE(known_event.handled());
  EXPECT_EQ(1, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent ime_event(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN,
                         ui::EF_IME_FABRICATED_KEY);
  ime_event.SetProperties(properties);
  DispatchEventUsingWindowDispatcher(&ime_event);
  EXPECT_TRUE(ime_event.handled());
  EXPECT_EQ(1, handler.num_key_events());

  handler.Reset();
  ui::KeyEvent unknown_key_with_char_event(ui::EventType::kKeyPressed,
                                           ui::VKEY_UNKNOWN, ui::EF_NONE);
  unknown_key_with_char_event.set_character(0x00e4 /* "ä" */);
  unknown_key_with_char_event.SetProperties(properties);
  DispatchEventUsingWindowDispatcher(&unknown_key_with_char_event);
  EXPECT_TRUE(unknown_key_with_char_event.handled());
  EXPECT_EQ(1, handler.num_key_events());
  root_window()->RemovePreTargetHandler(&handler);
}

TEST_F(WindowEventDispatcherTest, NoDelegateWindowReceivesKeyEvents) {
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  w1->Show();
  w1->Focus();

  ui::test::TestEventHandler handler;
  w1->AddPreTargetHandler(&handler);
  ui::KeyEvent key_press(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
#if BUILDFLAG(IS_OZONE)
  ui::SetKeyboardImeFlags(&key_press, ui::kPropertyKeyboardImeIgnoredFlag);
#endif

  DispatchEventUsingWindowDispatcher(&key_press);
  EXPECT_TRUE(key_press.handled());
  EXPECT_EQ(1, handler.num_key_events());

  w1->RemovePreTargetHandler(&handler);
}

// Tests that touch-events that are beyond the bounds of the root-window do get
// propagated to the event filters correctly with the root as the target.
TEST_F(WindowEventDispatcherTest, TouchEventsOutsideBounds) {
  ui::test::TestEventHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  gfx::Point position = root_window()->bounds().origin();
  position.Offset(-10, -10);
  ui::TouchEvent press(ui::EventType::kTouchPressed, position,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_touch_events());

  position = root_window()->bounds().origin();
  position.Offset(root_window()->bounds().width() + 10,
                  root_window()->bounds().height() + 10);
  ui::TouchEvent release(ui::EventType::kTouchReleased, position,
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(2, handler.num_touch_events());
  root_window()->RemovePreTargetHandler(&handler);
}

// Tests that scroll events are dispatched correctly.
TEST_F(WindowEventDispatcherTest, ScrollEventDispatch) {
  base::TimeTicks now = ui::EventTimeForNow();
  ui::test::TestEventHandler handler;
  root_window()->AddPreTargetHandler(&handler);

  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  w1->SetBounds(gfx::Rect(20, 20, 40, 40));

  // A scroll event on the root-window itself is dispatched.
  ui::ScrollEvent scroll1(ui::EventType::kScroll, gfx::Point(10, 10), now, 0, 0,
                          -10, 0, -10, 2);
  DispatchEventUsingWindowDispatcher(&scroll1);
  EXPECT_EQ(1, handler.num_scroll_events());

  // Scroll event on a window should be dispatched properly.
  ui::ScrollEvent scroll2(ui::EventType::kScroll, gfx::Point(25, 30), now, 0,
                          -10, 0, -10, 0, 2);
  DispatchEventUsingWindowDispatcher(&scroll2);
  EXPECT_EQ(2, handler.num_scroll_events());
  root_window()->RemovePreTargetHandler(&handler);
}

TEST_F(WindowEventDispatcherTest, PreDispatchKeyEventToIme) {
  TestImeKeyEventDispatcher dispatcher;
  ui::MockInputMethod mock_ime(&dispatcher);
  host()->SetSharedInputMethod(&mock_ime);

  ConsumeKeyHandler handler;
  std::unique_ptr<Window> w(CreateNormalWindow(1, root_window(), nullptr));
  w->AddPostTargetHandler(&handler);
  w->Show();
  w->Focus();

  // The dispatched event went to IME before the event target.
  ui::KeyEvent key_press(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&key_press);
  EXPECT_EQ(0, handler.num_key_events());
  EXPECT_EQ(1, dispatcher.dispatched_event_count());

  // However, for the window with kSkipImeProcessing
  // The event went to the event target at first.
  w->SetProperty(client::kSkipImeProcessing, true);
  ui::KeyEvent key_release(ui::EventType::kKeyReleased, ui::VKEY_A,
                           ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&key_release);
  EXPECT_EQ(1, handler.num_key_events());
  EXPECT_EQ(1, dispatcher.dispatched_event_count());

  host()->SetSharedInputMethod(nullptr);
}

namespace {

// ui::EventHandler that tracks the types of events it's seen.
class EventFilterRecorder : public ui::EventHandler {
 public:
  typedef std::vector<ui::EventType> Events;
  typedef std::vector<gfx::Point> EventLocations;
  typedef std::vector<int> EventFlags;

  EventFilterRecorder()
      : wait_until_event_(ui::EventType::kUnknown),
        last_touch_may_cause_scrolling_(false) {}

  EventFilterRecorder(const EventFilterRecorder&) = delete;
  EventFilterRecorder& operator=(const EventFilterRecorder&) = delete;

  const Events& events() const { return events_; }

  const EventLocations& mouse_locations() const { return mouse_locations_; }
  gfx::Point mouse_location(int i) const { return mouse_locations_[i]; }
  const EventLocations& touch_locations() const { return touch_locations_; }
  const EventLocations& gesture_locations() const { return gesture_locations_; }
  const EventFlags& mouse_event_flags() const { return mouse_event_flags_; }

  void WaitUntilReceivedEvent(ui::EventType type) {
    wait_until_event_ = type;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  Events GetAndResetEvents() {
    Events events = events_;
    Reset();
    return events;
  }

  void Reset() {
    events_.clear();
    mouse_locations_.clear();
    touch_locations_.clear();
    gesture_locations_.clear();
    mouse_event_flags_.clear();
    last_touch_may_cause_scrolling_ = false;
  }

  // ui::EventHandler overrides:
  void OnEvent(ui::Event* event) override {
    ui::EventHandler::OnEvent(event);
    events_.push_back(event->type());
    if (wait_until_event_ == event->type() && run_loop_) {
      run_loop_->Quit();
      wait_until_event_ = ui::EventType::kUnknown;
    }
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    mouse_locations_.push_back(event->location());
    mouse_event_flags_.push_back(event->flags());
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    touch_locations_.push_back(event->location());
    last_touch_may_cause_scrolling_ = event->may_cause_scrolling();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    gesture_locations_.push_back(event->location());
  }

  bool HasReceivedEvent(ui::EventType type) {
    return base::Contains(events_, type);
  }

  bool LastTouchMayCauseScrolling() const {
    return last_touch_may_cause_scrolling_;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ui::EventType wait_until_event_;

  Events events_;
  EventLocations mouse_locations_;
  EventLocations touch_locations_;
  EventLocations gesture_locations_;
  EventFlags mouse_event_flags_;
  bool last_touch_may_cause_scrolling_;
};

// Converts an EventType to a string.
std::string EventTypeToString(ui::EventType type) {
  switch (type) {
    case ui::EventType::kTouchReleased:
      return "TOUCH_RELEASED";

    case ui::EventType::kTouchCancelled:
      return "TOUCH_CANCELLED";

    case ui::EventType::kTouchPressed:
      return "TOUCH_PRESSED";

    case ui::EventType::kTouchMoved:
      return "TOUCH_MOVED";

    case ui::EventType::kMousePressed:
      return "MOUSE_PRESSED";

    case ui::EventType::kMouseDragged:
      return "MOUSE_DRAGGED";

    case ui::EventType::kMouseReleased:
      return "MOUSE_RELEASED";

    case ui::EventType::kMouseMoved:
      return "MOUSE_MOVED";

    case ui::EventType::kMouseEntered:
      return "MOUSE_ENTERED";

    case ui::EventType::kMouseExited:
      return "MOUSE_EXITED";

    case ui::EventType::kGestureScrollBegin:
      return "GESTURE_SCROLL_BEGIN";

    case ui::EventType::kGestureScrollEnd:
      return "GESTURE_SCROLL_END";

    case ui::EventType::kGestureScrollUpdate:
      return "GESTURE_SCROLL_UPDATE";

    case ui::EventType::kGesturePinchBegin:
      return "GESTURE_PINCH_BEGIN";

    case ui::EventType::kGesturePinchEnd:
      return "GESTURE_PINCH_END";

    case ui::EventType::kGesturePinchUpdate:
      return "GESTURE_PINCH_UPDATE";

    case ui::EventType::kGestureTap:
      return "GESTURE_TAP";

    case ui::EventType::kGestureTapDown:
      return "GESTURE_TAP_DOWN";

    case ui::EventType::kGestureTapCancel:
      return "GESTURE_TAP_CANCEL";

    case ui::EventType::kGestureShowPress:
      return "GESTURE_SHOW_PRESS";

    case ui::EventType::kGestureBegin:
      return "GESTURE_BEGIN";

    case ui::EventType::kGestureEnd:
      return "GESTURE_END";

    default:
      // We should explicitly require each event type.
      NOTREACHED_IN_MIGRATION()
          << "Received unexpected event: " << base::to_underlying(type);
      break;
  }
  return "";
}

std::string EventTypesToString(const EventFilterRecorder::Events& events) {
  std::string result;
  for (size_t i = 0; i < events.size(); ++i) {
    if (i != 0)
      result += " ";
    result += EventTypeToString(events[i]);
  }
  return result;
}

}  // namespace

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

// Verifies a repost mouse event targets the window with capture (if there is
// one).
// Flaky on 32-bit Windows bots.  http://crbug.com/388290
TEST_F(WindowEventDispatcherTest, MAYBE(RepostTargetsCaptureWindow)) {
  // Set capture on |window| generate a mouse event (that is reposted) and not
  // over |window| and verify |window| gets it (|window| gets it because it has
  // capture).
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  EventFilterRecorder recorder;
  std::unique_ptr<Window> window(CreateNormalWindow(1, root_window(), NULL));
  window->SetBounds(gfx::Rect(20, 20, 40, 30));
  window->AddPreTargetHandler(&recorder);
  window->SetCapture();
  const ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);
  host()->dispatcher()->RepostEvent(&press_event);
  RunAllPendingInMessageLoop();  // Necessitated by RepostEvent().
  // Mouse moves/enters may be generated. We only care about a pressed.
  EXPECT_TRUE(EventTypesToString(recorder.events()).find("MOUSE_PRESSED") !=
              std::string::npos)
      << EventTypesToString(recorder.events());
  window->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, MouseMovesHeld) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::MouseEvent mouse_move_event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                                  gfx::Point(0, 0), ui::EventTimeForNow(), 0,
                                  0);
  DispatchEventUsingWindowDispatcher(&mouse_move_event);
  // Discard MOUSE_ENTER.
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the MOUSE_DRAGGED event.
  ui::MouseEvent mouse_dragged_event(ui::EventType::kMouseDragged,
                                     gfx::Point(0, 0), gfx::Point(0, 0),
                                     ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  EXPECT_TRUE(recorder.events().empty());

  // Check that we do dispatch the held MOUSE_DRAGGED event before another type
  // of event.
  ui::MouseEvent mouse_pressed_event(ui::EventType::kMousePressed,
                                     gfx::Point(0, 0), gfx::Point(0, 0),
                                     ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();

  // Check that we coalesce held MOUSE_DRAGGED events. Note that here (and
  // elsewhere in this test) we re-define each event prior to dispatch so that
  // it has the correct state (phase, handled, target, etc.).
  mouse_dragged_event =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  ui::MouseEvent mouse_dragged_event2(ui::EventType::kMouseDragged,
                                      gfx::Point(10, 10), gfx::Point(10, 10),
                                      ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  EXPECT_TRUE(recorder.events().empty());
  mouse_pressed_event =
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  mouse_dragged_event =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(recorder.events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(recorder.events()));
  recorder.Reset();

  // However if another message comes in before the dispatch of the posted
  // event, check that the posted event is dispatched before this new event.
  host()->dispatcher()->HoldPointerMoves();
  mouse_dragged_event =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  mouse_pressed_event =
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(recorder.events()));
  recorder.Reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Check that if the other message is another MOUSE_DRAGGED, we still coalesce
  // them.
  host()->dispatcher()->HoldPointerMoves();
  mouse_dragged_event =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  host()->dispatcher()->ReleasePointerMoves();
  mouse_dragged_event2 =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(10, 10),
                     gfx::Point(10, 10), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(recorder.events()));
  recorder.Reset();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Check that synthetic mouse move event has a right location when issued
  // while holding pointer moves.
  mouse_dragged_event =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(0, 0),
                     gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);
  mouse_dragged_event2 =
      ui::MouseEvent(ui::EventType::kMouseDragged, gfx::Point(10, 10),
                     gfx::Point(10, 10), ui::EventTimeForNow(), 0, 0);
  ui::MouseEvent mouse_dragged_event3(ui::EventType::kMouseDragged,
                                      gfx::Point(28, 28), gfx::Point(28, 28),
                                      ui::EventTimeForNow(), 0, 0);
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event2);
  window->SetBounds(gfx::Rect(15, 15, 80, 80));
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  host()->dispatcher()->ReleasePointerMoves();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_MOVED", EventTypesToString(recorder.events()));
  EXPECT_EQ(gfx::Point(13, 13), recorder.mouse_location(0));
  recorder.Reset();
  root_window()->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, TouchMovesHeld) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));

  // Starting the touch and throwing out the first few events, since the system
  // is going to generate synthetic mouse events that are not relevant to the
  // test.
  ui::TouchEvent touch_pressed_event(
      ui::EventType::kTouchPressed, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&touch_pressed_event);
  recorder.WaitUntilReceivedEvent(ui::EventType::kGestureShowPress);
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the TOUCH_MOVED event.
  ui::TouchEvent touch_moved_event(
      ui::EventType::kTouchMoved, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent touch_moved_event2(
      ui::EventType::kTouchMoved, gfx::Point(11, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  ui::TouchEvent touch_moved_event3(
      ui::EventType::kTouchMoved, gfx::Point(12, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  DispatchEventUsingWindowDispatcher(&touch_moved_event);
  EXPECT_TRUE(recorder.events().empty());

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  DispatchEventUsingWindowDispatcher(&touch_moved_event2);
  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(recorder.events().empty());

  RunAllPendingInMessageLoop();
  EXPECT_EQ("TOUCH_MOVED", EventTypesToString(recorder.events()));
  recorder.Reset();

  // If another touch event occurs then the held touch should be dispatched
  // immediately before it.
  ui::TouchEvent touch_released_event(
      ui::EventType::kTouchReleased, gfx::Point(10, 10), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  recorder.Reset();
  host()->dispatcher()->HoldPointerMoves();
  DispatchEventUsingWindowDispatcher(&touch_moved_event3);
  DispatchEventUsingWindowDispatcher(&touch_released_event);
  EXPECT_EQ("TOUCH_MOVED TOUCH_RELEASED GESTURE_TAP GESTURE_END",
            EventTypesToString(recorder.events()));
  recorder.Reset();
  host()->dispatcher()->ReleasePointerMoves();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that mouse move event has a right location
// when there isn't the target window
TEST_F(WindowEventDispatcherTest, MouseEventWithoutTargetWindow) {
  EventFilterRecorder recorder_first;
  EventFilterRecorder recorder_second;

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window_first(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(20, 10, 10, 20), root_window()));
  window_first->Show();
  window_first->AddPreTargetHandler(&recorder_first);

  std::unique_ptr<aura::Window> window_second(CreateTestWindowWithDelegate(
      &delegate, 2, gfx::Rect(20, 30, 10, 20), root_window()));
  window_second->Show();
  window_second->AddPreTargetHandler(&recorder_second);

  const gfx::Point event_location(22, 33);
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                       event_location, ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);

  EXPECT_TRUE(recorder_first.events().empty());
  EXPECT_EQ("MOUSE_ENTERED MOUSE_MOVED",
            EventTypesToString(recorder_second.events()));
  ASSERT_EQ(2u, recorder_second.mouse_locations().size());
  EXPECT_EQ(gfx::Point(2, 3).ToString(),
            recorder_second.mouse_locations()[0].ToString());

  window_first->RemovePreTargetHandler(&recorder_first);
  window_second->RemovePreTargetHandler(&recorder_second);
}

// Tests that a mouse exit is dispatched to the last mouse location when
// the window is hiddden.
TEST_F(WindowEventDispatcherTest, DispatchMouseExitWhenHidingWindow) {
  EventFilterRecorder recorder;

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  window->Show();
  window->AddPreTargetHandler(&recorder);

  // Dispatch a mouse move event into the window.
  const gfx::Point event_location(22, 33);
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                       event_location, ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Hide the window and verify a mouse exit event's location.
  window->Hide();
  EXPECT_FALSE(recorder.events().empty());
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(recorder.events()));
  ASSERT_EQ(1u, recorder.mouse_locations().size());
  EXPECT_EQ(gfx::Point(12, 23).ToString(),
            recorder.mouse_locations()[0].ToString());
  window->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, HeldMovesDispatchMouseExitWhenHidingWindow) {
  EventFilterRecorder recorder;

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  window->Show();
  window->AddPreTargetHandler(&recorder);

  // Dispatch a mouse move event into the window.
  const gfx::Point event_location(22, 33);
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                       event_location, ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Hide the window and verify a mouse exit event's location.
  host()->dispatcher()->HoldPointerMoves();
  window->Hide();
  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_FALSE(recorder.events().empty());
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(recorder.events()));
  ASSERT_EQ(1u, recorder.mouse_locations().size());
  EXPECT_EQ(gfx::Point(12, 23).ToString(),
            recorder.mouse_locations()[0].ToString());
  window->RemovePreTargetHandler(&recorder);
}

// Tests that a mouse-exit event is not synthesized during shutdown.
TEST_F(WindowEventDispatcherTest, NoMouseExitInShutdown) {
  EventFilterRecorder recorder;
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  window->Show();
  window->AddPreTargetHandler(&recorder);

  // Simulate mouse move into the window.
  const gfx::Point event_location = window->bounds().CenterPoint();
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                       event_location, ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Simulate shutdown.
  host()->dispatcher()->Shutdown();

  // Hiding the window does not generate a mouse-exit event.
  window->Hide();
  EXPECT_TRUE(recorder.events().empty());
  window->RemovePreTargetHandler(&recorder);
}

// Verifies that a direct call to ProcessedTouchEvent() does not cause a crash.
TEST_F(WindowEventDispatcherTest, CallToProcessedTouchEvent) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));

  host()->dispatcher()->ProcessedTouchEvent(
      0, window.get(), ui::ER_UNHANDLED,
      false /* is_source_touch_event_set_blocking */);
}

// This event handler requests the dispatcher to start holding pointer-move
// events when it receives the first scroll-update gesture.
class HoldPointerOnScrollHandler : public ui::test::TestEventHandler {
 public:
  HoldPointerOnScrollHandler(WindowEventDispatcher* dispatcher,
                             EventFilterRecorder* filter)
      : dispatcher_(dispatcher), filter_(filter), holding_moves_(false) {}

  HoldPointerOnScrollHandler(const HoldPointerOnScrollHandler&) = delete;
  HoldPointerOnScrollHandler& operator=(const HoldPointerOnScrollHandler&) =
      delete;

  ~HoldPointerOnScrollHandler() override = default;

 private:
  // ui::test::TestEventHandler:
  void OnGestureEvent(ui::GestureEvent* gesture) override {
    if (!holding_moves_ &&
        gesture->type() == ui::EventType::kGestureScrollUpdate) {
      holding_moves_ = true;
      dispatcher_->HoldPointerMoves();
      filter_->Reset();
    } else if (gesture->type() == ui::EventType::kGestureScrollEnd) {
      dispatcher_->ReleasePointerMoves();
      holding_moves_ = false;
    }
  }

  raw_ptr<WindowEventDispatcher> dispatcher_;
  raw_ptr<EventFilterRecorder> filter_;
  bool holding_moves_;
};

// Tests that touch-move events don't contribute to an in-progress scroll
// gesture if touch-move events are being held by the dispatcher.
TEST_F(WindowEventDispatcherTest, TouchMovesHeldOnScroll) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);
  test::TestWindowDelegate delegate;
  HoldPointerOnScrollHandler handler(host()->dispatcher(), &recorder);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  window->AddPreTargetHandler(&handler);

  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(60, 60), gfx::Point(10, 60),
                                  base::Milliseconds(100), 25);

  // |handler| will have reset |filter| and started holding the touch-move
  // events when scrolling started. At the end of the scroll (i.e. upon
  // touch-release), the held touch-move event will have been dispatched first,
  // along with the subsequent events (i.e. touch-release, scroll-end, and
  // gesture-end).
  const EventFilterRecorder::Events& events = recorder.events();
  EXPECT_EQ(
      "TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
      "GESTURE_SCROLL_END GESTURE_END",
      EventTypesToString(events));
  ASSERT_EQ(2u, recorder.touch_locations().size());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[0].ToString());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[1].ToString());
  window->RemovePreTargetHandler(&handler);
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a 'held' touch-event does contribute to gesture event when it is
// dispatched.
TEST_F(WindowEventDispatcherTest, HeldTouchMoveContributesToGesture) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  const gfx::Point location(20, 20);
  ui::TouchEvent press(ui::EventType::kTouchPressed, location,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchPressed));
  recorder.Reset();

  EXPECT_EQ(location, GetLastTouchPoint(root_window()));

  host()->dispatcher()->HoldPointerMoves();

  const gfx::Point next_location = location + gfx::Vector2d(100, 100);
  ui::TouchEvent move(ui::EventType::kTouchMoved, next_location,
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move);

  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kTouchMoved));
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollBegin));

  // The touch location shouldn't be updated yet.
  EXPECT_EQ(location, GetLastTouchPoint(root_window()));
  recorder.Reset();

  host()->dispatcher()->ReleasePointerMoves();
  EXPECT_FALSE(recorder.HasReceivedEvent(ui::EventType::kTouchMoved));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchMoved));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollBegin));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollUpdate));
  // The touch location should be updated after release.
  EXPECT_EQ(next_location, GetLastTouchPoint(root_window()));

  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that synthetic mouse events are ignored when mouse
// events are disabled.
TEST_F(WindowEventDispatcherTest, DispatchSyntheticMouseEvents) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  test::TestCursorClient cursor_client(root_window());

  // Dispatch a non-synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse1(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Dispatch a synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse2(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EventTimeForNow(),
                        ui::EF_IS_SYNTHESIZED, 0);
  DispatchEventUsingWindowDispatcher(&mouse2);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Dispatch a synthetic mouse event when mouse events are disabled.
  cursor_client.DisableMouseEvents();
  DispatchEventUsingWindowDispatcher(&mouse2);
  EXPECT_TRUE(recorder.events().empty());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a mouse-move event is not synthesized when a mouse-button is down.
TEST_F(WindowEventDispatcherTest, DoNotSynthesizeWhileButtonDown) {
  EventFilterRecorder recorder;
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();

  window->AddPreTargetHandler(&recorder);
  // Dispatch a non-synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse1(ui::EventType::kMousePressed, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EventTimeForNow(),
                        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(&mouse1);
  ASSERT_EQ(1u, recorder.events().size());
  EXPECT_EQ(ui::EventType::kMousePressed, recorder.events()[0]);
  window->RemovePreTargetHandler(&recorder);
  recorder.Reset();

  // Move |window| away from underneath the cursor.
  root_window()->AddPreTargetHandler(&recorder);
  window->SetBounds(gfx::Rect(30, 30, 100, 100));
  EXPECT_TRUE(recorder.events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a mouse-press event is not dispatched during shutdown.
TEST_F(WindowEventDispatcherTest, DoNotDispatchInShutdown) {
  EventFilterRecorder recorder;
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->AddPreTargetHandler(&recorder);

  // Simulate shutdown.
  host()->dispatcher()->Shutdown();

  // Attempt to dispatch a mouse press.
  const gfx::Point center = window->bounds().CenterPoint();
  ui::MouseEvent press(ui::EventType::kMousePressed, center, center,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  DispatchEventUsingWindowDispatcher(&press);

  // Event was not dispatched.
  EXPECT_TRUE(recorder.events().empty());
  window->RemovePreTargetHandler(&recorder);
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

// Tests synthetic mouse events generated when window bounds changes such that
// the cursor previously outside the window becomes inside, or vice versa.
// Do not synthesize events if the window ignores events or is invisible.
// Flaky on 32-bit Windows bots.  http://crbug.com/388272
TEST_F(WindowEventDispatcherTest,
       MAYBE(SynthesizeMouseEventsOnWindowBoundsChanged)) {
  test::TestCursorClient cursor_client(root_window());
  cursor_client.ShowCursor();

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  EventFilterRecorder recorder;
  window->AddPreTargetHandler(&recorder);

  // Dispatch a non-synthetic mouse event to place cursor inside window bounds.
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Update the window bounds so that cursor is now outside the window.
  // This should trigger a synthetic MOVED event.
  gfx::Rect bounds1(20, 20, 100, 100);
  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(recorder.events().empty());
  ASSERT_FALSE(recorder.mouse_event_flags().empty());
  EXPECT_EQ(ui::EventType::kMouseMoved, recorder.events().back());
  EXPECT_EQ(ui::EF_IS_SYNTHESIZED, recorder.mouse_event_flags().back());
  recorder.Reset();

  // Update the window bounds so that cursor is back inside the window.
  // The origin of window bounds change so this should trigger a synthetic
  // event.
  gfx::Rect bounds2(5, 5, 100, 100);
  window->SetBounds(bounds2);
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(recorder.events().empty());
  ASSERT_FALSE(recorder.mouse_event_flags().empty());
  EXPECT_EQ(ui::EventType::kMouseMoved, recorder.events().back());
  EXPECT_EQ(ui::EF_IS_SYNTHESIZED, recorder.mouse_event_flags().back());
  recorder.Reset();

  // Update the window bounds so that cursor is still inside the window.
  // The origin of window bounds doesn't change so this should not trigger
  // a synthetic event.
  gfx::Rect bounds3(5, 5, 200, 200);
  window->SetBounds(bounds3);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  recorder.Reset();

  // Set window to ignore events.
  window->SetEventTargetingPolicy(EventTargetingPolicy::kNone);

  // Update the window bounds so that cursor is from outside to inside the
  // window. This should not trigger a synthetic event.
  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  recorder.Reset();
  window->SetBounds(bounds2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());
  recorder.Reset();

  // Set window to accept events but invisible.
  window->SetEventTargetingPolicy(EventTargetingPolicy::kTargetAndDescendants);
  window->Hide();
  recorder.Reset();

  // Update the window bounds so that cursor is outside the window.
  // This should not trigger a synthetic event.
  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Hide the cursor. None of the following scenario should trigger
  // a synthetic event.
  cursor_client.HideCursor();

  window->Show();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  window->SetBounds(bounds2);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  window->SetBounds(bounds1);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  cursor_client.ShowCursor();
  window->SetBounds(bounds2);
  RunAllPendingInMessageLoop();
  ASSERT_FALSE(recorder.events().empty());
  ASSERT_FALSE(recorder.mouse_event_flags().empty());
  EXPECT_EQ(ui::EventType::kMouseMoved, recorder.events().back());
  EXPECT_EQ(ui::EF_IS_SYNTHESIZED, recorder.mouse_event_flags().back());
  recorder.Reset();
  window->RemovePreTargetHandler(&recorder);
}

// Tests that a mouse exit is dispatched to the last known cursor location
// when the cursor becomes invisible.
TEST_F(WindowEventDispatcherTest, DispatchMouseExitWhenCursorHidden) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  gfx::Point window_origin(7, 18);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(window_origin, gfx::Size(100, 100)),
      root_window()));
  window->Show();

  // Dispatch a mouse move event into the window.
  gfx::Point mouse_location(gfx::Point(15, 25));
  ui::MouseEvent mouse1(ui::EventType::kMouseMoved, mouse_location,
                        mouse_location, ui::EventTimeForNow(), 0, 0);
  EXPECT_TRUE(recorder.events().empty());
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  // Hide the cursor and verify a mouse exit was dispatched.
  host()->OnCursorVisibilityChanged(false);
  EXPECT_FALSE(recorder.events().empty());
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(recorder.events()));

  // Verify the mouse exit was dispatched at the correct location
  // (in the correct coordinate space).
  int translated_x = mouse_location.x() - window_origin.x();
  int translated_y = mouse_location.y() - window_origin.y();
  gfx::Point translated_point(translated_x, translated_y);
  EXPECT_EQ(recorder.mouse_location(0).ToString(), translated_point.ToString());

  // Verify the mouse exit with ui::EF_CURSOR_HIDE flags.
  EXPECT_TRUE(recorder.mouse_event_flags()[0] & ui::EF_CURSOR_HIDE);
  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that a synthetic mouse exit is dispatched to the last known cursor
// location after mouse events are disabled on the cursor client.
TEST_F(WindowEventDispatcherTest,
       DispatchSyntheticMouseExitAfterMouseEventsDisabled) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  gfx::Point window_origin(7, 18);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(window_origin, gfx::Size(100, 100)),
      root_window()));
  window->Show();

  // Dispatch a mouse move event into the window.
  gfx::Point mouse_location(gfx::Point(15, 25));
  ui::MouseEvent mouse1(ui::EventType::kMouseMoved, mouse_location,
                        mouse_location, ui::EventTimeForNow(), 0, 0);
  EXPECT_TRUE(recorder.events().empty());
  DispatchEventUsingWindowDispatcher(&mouse1);
  EXPECT_FALSE(recorder.events().empty());
  recorder.Reset();

  test::TestCursorClient cursor_client(root_window());
  cursor_client.DisableMouseEvents();

  gfx::Point mouse_exit_location(gfx::Point(150, 150));
  ui::MouseEvent mouse2(ui::EventType::kMouseExited, gfx::Point(150, 150),
                        gfx::Point(150, 150), ui::EventTimeForNow(),
                        ui::EF_IS_SYNTHESIZED, 0);
  DispatchEventUsingWindowDispatcher(&mouse2);

  EXPECT_FALSE(recorder.events().empty());
  // We get the mouse exited event twice in our filter. Once during the
  // predispatch phase and during the actual dispatch.
  EXPECT_EQ("MOUSE_EXITED MOUSE_EXITED", EventTypesToString(recorder.events()));

  // Verify the mouse exit was dispatched at the correct location
  // (in the correct coordinate space).
  int translated_x = mouse_exit_location.x() - window_origin.x();
  int translated_y = mouse_exit_location.y() - window_origin.y();
  gfx::Point translated_point(translated_x, translated_y);
  EXPECT_EQ(recorder.mouse_location(0).ToString(), translated_point.ToString());
  root_window()->RemovePreTargetHandler(&recorder);
}

class DeletingEventFilter : public ui::EventHandler {
 public:
  DeletingEventFilter() : delete_during_pre_handle_(false) {}

  DeletingEventFilter(const DeletingEventFilter&) = delete;
  DeletingEventFilter& operator=(const DeletingEventFilter&) = delete;

  ~DeletingEventFilter() override = default;

  void Reset(bool delete_during_pre_handle) {
    delete_during_pre_handle_ = delete_during_pre_handle;
  }

 private:
  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (delete_during_pre_handle_)
      delete event->target();
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (delete_during_pre_handle_)
      delete event->target();
  }

  bool delete_during_pre_handle_;
};

class DeletingWindowDelegate : public test::TestWindowDelegate {
 public:
  DeletingWindowDelegate()
      : window_(nullptr), delete_during_handle_(false), got_event_(false) {}

  DeletingWindowDelegate(const DeletingWindowDelegate&) = delete;
  DeletingWindowDelegate& operator=(const DeletingWindowDelegate&) = delete;

  ~DeletingWindowDelegate() override = default;

  void Reset(Window* window, bool delete_during_handle) {
    window_ = window;
    delete_during_handle_ = delete_during_handle;
    got_event_ = false;
  }
  bool got_event() const { return got_event_; }

 private:
  // Overridden from WindowDelegate:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
  }

  raw_ptr<Window, AcrossTasksDanglingUntriaged> window_;
  bool delete_during_handle_;
  bool got_event_;
};

TEST_F(WindowEventDispatcherTest, DeleteWindowDuringDispatch) {
  // Verifies that we can delete a window during each phase of event handling.
  // Deleting the window should not cause a crash, only prevent further
  // processing from occurring.
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DeletingWindowDelegate d11;
  Window* w11 = CreateNormalWindow(11, w1.get(), &d11);
  WindowTracker tracker;
  DeletingEventFilter w1_filter;
  w1->AddPreTargetHandler(&w1_filter);
  client::GetFocusClient(w1.get())->FocusWindow(w11);

  ui::test::EventGenerator generator(root_window(), w11);

  // First up, no one deletes anything.
  tracker.Add(w11);
  d11.Reset(w11, false);

  generator.PressLeftButton();
  EXPECT_TRUE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());
  generator.ReleaseLeftButton();

  // Delegate deletes w11. This will prevent the post-handle step from applying.
  w1_filter.Reset(false);
  d11.Reset(w11, true);
  generator.PressKey(ui::VKEY_A, 0);
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());

  // Pre-handle step deletes w11. This will prevent the delegate and the post-
  // handle steps from applying.
  w11 = CreateNormalWindow(11, w1.get(), &d11);
  w1_filter.Reset(true);
  d11.Reset(w11, false);
  generator.PressLeftButton();
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_FALSE(d11.got_event());

  w1->RemovePreTargetHandler(&w1_filter);
}

namespace {

// A window delegate that detaches the parent of the target's parent window when
// it receives a tap event.
class DetachesParentOnTapDelegate : public test::TestWindowDelegate {
 public:
  DetachesParentOnTapDelegate() {}

  DetachesParentOnTapDelegate(const DetachesParentOnTapDelegate&) = delete;
  DetachesParentOnTapDelegate& operator=(const DetachesParentOnTapDelegate&) =
      delete;

  ~DetachesParentOnTapDelegate() override {}

 private:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTapDown) {
      event->SetHandled();
      return;
    }

    if (event->type() == ui::EventType::kGestureTap) {
      Window* parent = static_cast<Window*>(event->target())->parent();
      parent->parent()->RemoveChild(parent);
      event->SetHandled();
    }
  }
};

}  // namespace

// Tests that the gesture recognizer is reset for all child windows when a
// window hides. No expectations, just checks that the test does not crash.
TEST_F(WindowEventDispatcherTest,
       GestureRecognizerResetsTargetWhenParentHides) {
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DetachesParentOnTapDelegate delegate;
  std::unique_ptr<Window> parent(CreateNormalWindow(22, w1.get(), NULL));
  Window* child = CreateNormalWindow(11, parent.get(), &delegate);
  ui::test::EventGenerator generator(root_window(), child);
  generator.GestureTapAt(gfx::Point(40, 40));
}

namespace {

// A window delegate that processes nested gestures on tap.
class NestedGestureDelegate : public test::TestWindowDelegate {
 public:
  NestedGestureDelegate(ui::test::EventGenerator* generator,
                        const gfx::Point tap_location)
      : generator_(generator),
        tap_location_(tap_location),
        gesture_end_count_(0) {}

  NestedGestureDelegate(const NestedGestureDelegate&) = delete;
  NestedGestureDelegate& operator=(const NestedGestureDelegate&) = delete;

  ~NestedGestureDelegate() override {}

  int gesture_end_count() const { return gesture_end_count_; }

 private:
  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::EventType::kGestureTapDown:
        event->SetHandled();
        break;
      case ui::EventType::kGestureTap:
        if (generator_)
          generator_->GestureTapAt(tap_location_);
        event->SetHandled();
        break;
      case ui::EventType::kGestureEnd:
        ++gesture_end_count_;
        break;
      default:
        break;
    }
  }

  raw_ptr<ui::test::EventGenerator> generator_;
  const gfx::Point tap_location_;
  int gesture_end_count_;
};

}  // namespace

// Tests that gesture end is delivered after nested gesture processing.
TEST_F(WindowEventDispatcherTest, GestureEndDeliveredAfterNestedGestures) {
  NestedGestureDelegate d1(NULL, gfx::Point());
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), &d1));
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));

  ui::test::EventGenerator nested_generator(root_window(), w1.get());
  NestedGestureDelegate d2(&nested_generator, w1->bounds().CenterPoint());
  std::unique_ptr<Window> w2(CreateNormalWindow(1, root_window(), &d2));
  w2->SetBounds(gfx::Rect(100, 0, 100, 100));

  // Tap on w2 which triggers nested gestures for w1.
  ui::test::EventGenerator generator(root_window(), w2.get());
  generator.GestureTapAt(w2->bounds().CenterPoint());

  // Both windows should get their gesture end events.
  EXPECT_EQ(1, d1.gesture_end_count());
  EXPECT_EQ(1, d2.gesture_end_count());
}

// Tests whether we can repost the Tap down gesture event.
TEST_F(WindowEventDispatcherTest, RepostTapdownGestureTest) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::GestureEventDetails details(ui::EventType::kGestureTapDown);
  gfx::Point point(10, 10);
  ui::GestureEvent event(point.x(), point.y(), 0, ui::EventTimeForNow(),
                         details);
  host()->dispatcher()->RepostEvent(&event);
  RunAllPendingInMessageLoop();
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_FALSE(EventTypesToString(recorder.events()).find("GESTURE_TAP_DOWN") !=
               std::string::npos);
  recorder.Reset();
  root_window()->RemovePreTargetHandler(&recorder);
}

// This class inherits from the EventFilterRecorder class which provides a
// facility to record events. This class additionally provides a facility to
// repost the EventType::kGestureTapDown gesture to the target window and
// records events after that.
class RepostGestureEventRecorder : public EventFilterRecorder {
 public:
  RepostGestureEventRecorder(aura::Window* repost_source,
                             aura::Window* repost_target)
      : repost_source_(repost_source),
        repost_target_(repost_target),
        reposted_(false),
        done_cleanup_(false) {}

  RepostGestureEventRecorder(const RepostGestureEventRecorder&) = delete;
  RepostGestureEventRecorder& operator=(const RepostGestureEventRecorder&) =
      delete;

  ~RepostGestureEventRecorder() override {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (reposted_ && event->type() == ui::EventType::kTouchPressed) {
      done_cleanup_ = true;
      Reset();
    }
    EventFilterRecorder::OnTouchEvent(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    EXPECT_EQ(done_cleanup_ ? repost_target_.get() : repost_source_.get(),
              event->target());
    if (event->type() == ui::EventType::kGestureTapDown) {
      if (!reposted_) {
        EXPECT_NE(repost_target_, event->target());
        reposted_ = true;
        repost_target_->GetHost()->dispatcher()->RepostEvent(event);
        // Ensure that the reposted gesture event above goes to the
        // repost_target_;
        repost_source_->GetRootWindow()->RemoveChild(repost_source_);
        return;
      }
    }
    EventFilterRecorder::OnGestureEvent(event);
  }

  // Ignore mouse events as they don't fire at all times. This causes
  // the GestureRepostEventOrder test to fail randomly.
  void OnMouseEvent(ui::MouseEvent* event) override {}

 private:
  raw_ptr<aura::Window> repost_source_;
  raw_ptr<aura::Window> repost_target_;
  // set to true if we reposted the EventType::kGestureTapDown event.
  bool reposted_;
  // set true if we're done cleaning up after hiding repost_source_;
  bool done_cleanup_;
};

// Tests whether events which are generated after the reposted gesture event
// are received after that. In this case the scroll sequence events should
// be received after the reposted gesture event.
TEST_F(WindowEventDispatcherTest, GestureRepostEventOrder) {
  // Expected events at the end for the repost_target window defined below.
  const char kExpectedTargetEvents[] =
      // TODO)(rbyers): Gesture event reposting is disabled - crbug.com/279039.
      // "GESTURE_BEGIN GESTURE_TAP_DOWN "
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_MOVED "
      "GESTURE_SCROLL_UPDATE TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
      "GESTURE_SCROLL_END GESTURE_END";
  // We create two windows.
  // The first window (repost_source) is the one to which the initial tap
  // gesture is sent. It reposts this event to the second window
  // (repost_target).
  // We then generate the scroll sequence for repost_target and look for two
  // EventType::kGestureTapDown events in the event list at the end.
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> repost_target(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  std::unique_ptr<aura::Window> repost_source(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 50, 50), root_window()));

  RepostGestureEventRecorder repost_event_recorder(repost_source.get(),
                                                   repost_target.get());
  root_window()->AddPreTargetHandler(&repost_event_recorder);

  // Generate a tap down gesture for the repost_source. This will be reposted
  // to repost_target.
  ui::test::EventGenerator repost_generator(root_window(), repost_source.get());
  repost_generator.GestureTapAt(gfx::Point(40, 40));
  RunAllPendingInMessageLoop();

  ui::test::EventGenerator scroll_generator(root_window(), repost_target.get());
  scroll_generator.GestureScrollSequence(
      gfx::Point(80, 80), gfx::Point(100, 100), base::Milliseconds(100), 3);
  RunAllPendingInMessageLoop();

  int tap_down_count = 0;
  for (size_t i = 0; i < repost_event_recorder.events().size(); ++i) {
    if (repost_event_recorder.events()[i] == ui::EventType::kGestureTapDown) {
      ++tap_down_count;
    }
  }

  // We expect two tap down events. One from the repost and the other one from
  // the scroll sequence posted above.
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_EQ(1, tap_down_count);

  EXPECT_EQ(kExpectedTargetEvents,
            EventTypesToString(repost_event_recorder.events()));
  root_window()->RemovePreTargetHandler(&repost_event_recorder);
}

// An event filter that deletes the specified object when sees a mouse-exited
// event.
template <class T>
class OnMouseExitDeletingEventFilter : public EventFilterRecorder {
 public:
  explicit OnMouseExitDeletingEventFilter(T* object_to_delete)
      : object_to_delete_(object_to_delete) {}
  OnMouseExitDeletingEventFilter() : object_to_delete_(nullptr) {}

  OnMouseExitDeletingEventFilter(const OnMouseExitDeletingEventFilter&) =
      delete;
  OnMouseExitDeletingEventFilter& operator=(
      const OnMouseExitDeletingEventFilter&) = delete;

  ~OnMouseExitDeletingEventFilter() override {}

  void set_object_to_delete(T* object_to_delete) {
    object_to_delete_ = object_to_delete;
  }

  void set_delete_closure(base::OnceClosure delete_closure) {
    delete_closure_ = std::move(delete_closure);
  }

 private:
  // Overridden from ui::EventFilterRecorder.
  void OnMouseEvent(ui::MouseEvent* event) override {
    EventFilterRecorder::OnMouseEvent(event);
    if (object_to_delete_ && event->type() == ui::EventType::kMouseExited) {
      if (delete_closure_)
        std::move(delete_closure_).Run();
      delete object_to_delete_;
      object_to_delete_ = nullptr;
    }
  }

  // Closure that is run prior to |object_to_delete_| being deleted.
  base::OnceClosure delete_closure_;
  raw_ptr<T, AcrossTasksDanglingUntriaged> object_to_delete_;
};

// Tests that RootWindow drops mouse-moved event that is supposed to be sent to
// a child, but the child is destroyed because of the synthesized mouse-exit
// event generated on the previous mouse_moved_handler_.
TEST_F(WindowEventDispatcherTest, DeleteWindowDuringMouseMovedDispatch) {
  // Create window 1 and set its event filter. Window 1 will take ownership of
  // the event filter.
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  OnMouseExitDeletingEventFilter<Window> w1_filter;
  w1->AddPreTargetHandler(&w1_filter);
  w1->SetBounds(gfx::Rect(20, 20, 60, 60));
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_moved_handler());

  ui::test::EventGenerator generator(root_window(), w1.get());

  // Move mouse over window 1 to set it as the |mouse_moved_handler_| for the
  // root window.
  generator.MoveMouseTo(51, 51);
  EXPECT_EQ(w1.get(), host()->dispatcher()->mouse_moved_handler());

  // Create window 2 under the mouse cursor and stack it above window 1.
  Window* w2 = CreateNormalWindow(2, root_window(), NULL);
  w2->SetBounds(gfx::Rect(30, 30, 40, 40));
  root_window()->StackChildAbove(w2, w1.get());

  // Set window 2 as the window that is to be deleted when a mouse-exited event
  // happens on window 1.
  w1_filter.set_object_to_delete(w2);
  // Move mouse over window 2. This should generate a mouse-exited event for
  // window 1 resulting in deletion of window 2. The original mouse-moved event
  // that was targeted to window 2 should be dropped since window 2 is
  // destroyed. This test passes if no crash happens.
  generator.MoveMouseTo(52, 52);
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_moved_handler());

  // Check events received by window 1.
  EXPECT_EQ("MOUSE_ENTERED MOUSE_MOVED MOUSE_EXITED",
            EventTypesToString(w1_filter.events()));
  w1->RemovePreTargetHandler(&w1_filter);
}

// Tests the case where the event dispatcher is deleted during the pre-dispatch
// phase of dispatching and event.
TEST_F(WindowEventDispatcherTest, DeleteDispatcherDuringPreDispatch) {
  // Create a host for the window hierarchy. This host will be destroyed later
  // on.
  WindowTreeHost* host =
      WindowTreeHost::Create(
          ui::PlatformWindowInitProperties{gfx::Rect(0, 0, 100, 100)})
          .release();
  host->InitHost();
  host->window()->Show();

  // Create two windows.
  Window* w1 = CreateNormalWindow(1, host->window(), nullptr);
  w1->SetBounds(gfx::Rect(20, 20, 60, 60));
  Window* w2 = CreateNormalWindow(2, host->window(), nullptr);
  w2->SetBounds(gfx::Rect(80, 20, 120, 60));
  EXPECT_EQ(nullptr, host->dispatcher()->mouse_moved_handler());

  ui::test::EventGenerator generator(host->window(), w1);

  // Move mouse over window 1 to set it as the |mouse_moved_handler_| for the
  // root window.
  generator.MoveMouseTo(40, 40);
  EXPECT_EQ(w1, host->dispatcher()->mouse_moved_handler());

  // Set appropriate event filters for the two windows with the window tree host
  // as the object that is to be deleted when a mouse-exited event happens on
  // window 1. The windows will take ownership of the event filters.
  OnMouseExitDeletingEventFilter<WindowTreeHost> w1_filter(host);
  w1->AddPreTargetHandler(&w1_filter);
  EventFilterRecorder w2_filter;
  w2->AddPreTargetHandler(&w2_filter);

  w1_filter.set_delete_closure(
      base::BindLambdaForTesting([&w1_filter, &w2_filter, &w1, &w2]() {
        w1->RemovePreTargetHandler(&w1_filter);
        w2->RemovePreTargetHandler(&w2_filter);
      }));

  // Move mouse over window 2. This should generate a mouse-exited event for
  // window 1 resulting in deletion of window tree host and its event
  // dispatcher. The event dispatching should abort since the dispatcher is
  // destroyed. This test passes if no crash happens.
  // Here we can't use EventGenerator since it expects that the dispatcher is
  // not destroyed at the end of the dispatch.
  ui::MouseEvent mouse_move(ui::EventType::kMouseMoved, gfx::Point(20, 20),
                            gfx::Point(20, 20), base::TimeTicks(), 0, 0);
  ui::EventDispatchDetails details =
      host->dispatcher()->DispatchEvent(w2, &mouse_move);
  EXPECT_TRUE(details.dispatcher_destroyed);

  // Check events received by the two windows.
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(w1_filter.events()));
  EXPECT_EQ(std::string(), EventTypesToString(w2_filter.events()));
}

namespace {

// Used to track if OnWindowDestroying() is invoked and if there is a valid
// RootWindow at such time.
class ValidRootDuringDestructionWindowObserver : public aura::WindowObserver {
 public:
  ValidRootDuringDestructionWindowObserver(bool* got_destroying,
                                           bool* has_valid_root)
      : got_destroying_(got_destroying), has_valid_root_(has_valid_root) {}

  ValidRootDuringDestructionWindowObserver(
      const ValidRootDuringDestructionWindowObserver&) = delete;
  ValidRootDuringDestructionWindowObserver& operator=(
      const ValidRootDuringDestructionWindowObserver&) = delete;

  // WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    *got_destroying_ = true;
    *has_valid_root_ = (window->GetRootWindow() != NULL);
  }

 private:
  raw_ptr<bool> got_destroying_;
  raw_ptr<bool> has_valid_root_;
};

}  // namespace

// Verifies GetRootWindow() from ~Window returns a valid root.
TEST_F(WindowEventDispatcherTest, ValidRootDuringDestruction) {
  bool got_destroying = false;
  bool has_valid_root = false;
  ValidRootDuringDestructionWindowObserver observer(&got_destroying,
                                                    &has_valid_root);
  {
    std::unique_ptr<WindowTreeHost> host = WindowTreeHost::Create(
        ui::PlatformWindowInitProperties{gfx::Rect(0, 0, 100, 100)});
    host->InitHost();
    host->window()->Show();
    // Owned by WindowEventDispatcher.
    Window* w1 = CreateNormalWindow(1, host->window(), NULL);
    w1->AddObserver(&observer);
  }
  EXPECT_TRUE(got_destroying);
  EXPECT_TRUE(has_valid_root);
}

namespace {

// See description above DontResetHeldEvent for details.
class DontResetHeldEventWindowDelegate : public test::TestWindowDelegate {
 public:
  explicit DontResetHeldEventWindowDelegate(aura::Window* root)
      : root_(root), mouse_event_count_(0) {}

  DontResetHeldEventWindowDelegate(const DontResetHeldEventWindowDelegate&) =
      delete;
  DontResetHeldEventWindowDelegate& operator=(
      const DontResetHeldEventWindowDelegate&) = delete;

  ~DontResetHeldEventWindowDelegate() override = default;

  int mouse_event_count() const { return mouse_event_count_; }

  // TestWindowDelegate:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        mouse_event_count_++ == 0) {
      ui::MouseEvent mouse_event(ui::EventType::kMousePressed,
                                 gfx::Point(10, 10), gfx::Point(10, 10),
                                 ui::EventTimeForNow(), ui::EF_SHIFT_DOWN, 0);
      root_->GetHost()->dispatcher()->RepostEvent(&mouse_event);
    }
  }

 private:
  raw_ptr<Window> root_;
  int mouse_event_count_;
};

}  // namespace

// Verifies RootWindow doesn't reset |RootWindow::held_repostable_event_| after
// dispatching. This is done by using DontResetHeldEventWindowDelegate, which
// tracks the number of events with ui::EF_SHIFT_DOWN set (all reposted events
// have EF_SHIFT_DOWN). When the first event is seen RepostEvent() is used to
// schedule another reposted event.
TEST_F(WindowEventDispatcherTest, DontResetHeldEvent) {
  DontResetHeldEventWindowDelegate delegate(root_window());
  std::unique_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(10, 10),
                         gfx::Point(10, 10), ui::EventTimeForNow(),
                         ui::EF_SHIFT_DOWN, 0);
  root_window()->GetHost()->dispatcher()->RepostEvent(&pressed);
  ui::MouseEvent pressed2(ui::EventType::kMousePressed, gfx::Point(10, 10),
                          gfx::Point(10, 10), ui::EventTimeForNow(), 0, 0);
  // Dispatch an event to flush event scheduled by way of RepostEvent().
  DispatchEventUsingWindowDispatcher(&pressed2);
  // Delegate should have seen reposted event (identified by way of
  // EF_SHIFT_DOWN). Dispatch another event to flush the second
  // RepostedEvent().
  EXPECT_EQ(1, delegate.mouse_event_count());
  DispatchEventUsingWindowDispatcher(&pressed2);
  EXPECT_EQ(2, delegate.mouse_event_count());
}

namespace {

// See description above DeleteHostFromHeldMouseEvent for details.
class DeleteHostFromHeldMouseEventDelegate : public test::TestWindowDelegate {
 public:
  explicit DeleteHostFromHeldMouseEventDelegate(WindowTreeHost* host)
      : host_(host), got_mouse_event_(false), got_destroy_(false) {}

  DeleteHostFromHeldMouseEventDelegate(
      const DeleteHostFromHeldMouseEventDelegate&) = delete;
  DeleteHostFromHeldMouseEventDelegate& operator=(
      const DeleteHostFromHeldMouseEventDelegate&) = delete;

  ~DeleteHostFromHeldMouseEventDelegate() override = default;

  bool got_mouse_event() const { return got_mouse_event_; }
  bool got_destroy() const { return got_destroy_; }

  // TestWindowDelegate:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0) {
      got_mouse_event_ = true;
      delete host_;
    }
  }
  void OnWindowDestroyed(Window* window) override { got_destroy_ = true; }

 private:
  raw_ptr<WindowTreeHost, AcrossTasksDanglingUntriaged> host_;
  bool got_mouse_event_;
  bool got_destroy_;
};

}  // namespace

// Verifies if a WindowTreeHost is deleted from dispatching a held mouse event
// we don't crash.
TEST_F(WindowEventDispatcherTest, DeleteHostFromHeldMouseEvent) {
  // Should be deleted by |delegate|.
  WindowTreeHost* h2 = WindowTreeHost::Create(ui::PlatformWindowInitProperties{
                                                  gfx::Rect(0, 0, 100, 100)})
                           .release();
  h2->InitHost();
  h2->window()->Show();
  DeleteHostFromHeldMouseEventDelegate delegate(h2);
  // Owned by |h2|.
  Window* w1 = CreateNormalWindow(1, h2->window(), &delegate);
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(10, 10),
                         gfx::Point(10, 10), ui::EventTimeForNow(),
                         ui::EF_SHIFT_DOWN, 0);
  h2->dispatcher()->RepostEvent(&pressed);
  // RunAllPendingInMessageLoop() to make sure the |pressed| is run.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(delegate.got_mouse_event());
  EXPECT_TRUE(delegate.got_destroy());
}

TEST_F(WindowEventDispatcherTest, WindowHideCancelsActiveTouches) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  ui::TouchEvent press(ui::EventType::kTouchPressed, position1,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN",
            EventTypesToString(recorder.GetAndResetEvents()));

  window->Hide();

  EXPECT_EQ(ui::EventType::kTouchCancelled, recorder.events()[0]);
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureTapCancel));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureEnd));
  EXPECT_EQ(3U, recorder.events().size());
  root_window()->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, WindowHideCancelsActiveGestures) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  gfx::Point position2 = root_window()->bounds().CenterPoint();
  ui::TouchEvent press(ui::EventType::kTouchPressed, position1,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);

  ui::TouchEvent move(ui::EventType::kTouchMoved, position2,
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move);

  ui::TouchEvent press2(ui::EventType::kTouchPressed, position1,
                        ui::EventTimeForNow(),
                        ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  DispatchEventUsingWindowDispatcher(&press2);

  // TODO(tdresser): once the unified Gesture Recognizer has stuck, remove the
  // special casing here. See crbug.com/332418 for details.
  std::string expected =
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_PINCH_BEGIN";

  std::string expected_ugr =
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_PRESSED GESTURE_BEGIN";

  std::string events_string = EventTypesToString(recorder.GetAndResetEvents());
  EXPECT_TRUE((expected == events_string) || (expected_ugr == events_string));

  window->Hide();

  expected =
      "TOUCH_CANCELLED GESTURE_PINCH_END GESTURE_END TOUCH_CANCELLED "
      "GESTURE_SCROLL_END GESTURE_END";
  expected_ugr =
      "TOUCH_CANCELLED GESTURE_SCROLL_END GESTURE_END TOUCH_CANCELLED "
      "GESTURE_END";

  events_string = EventTypesToString(recorder.GetAndResetEvents());
  EXPECT_TRUE((expected == events_string) || (expected_ugr == events_string));

  root_window()->RemovePreTargetHandler(&recorder);
}

// Places two windows side by side.  Starts a pinch in one window, then sets
// capture to the other window.  Ensures that subsequent pinch events are
// sent to the window which gained capture.
TEST_F(WindowEventDispatcherTest, TouchpadPinchEventsRetargetOnCapture) {
  EventFilterRecorder recorder1;
  EventFilterRecorder recorder2;
  std::unique_ptr<Window> window1(
      CreateNormalWindow(1, root_window(), nullptr));
  window1->SetBounds(gfx::Rect(0, 0, 40, 40));

  std::unique_ptr<Window> window2(
      CreateNormalWindow(2, root_window(), nullptr));
  window2->SetBounds(gfx::Rect(40, 0, 40, 40));

  window1->AddPreTargetHandler(&recorder1);
  window2->AddPreTargetHandler(&recorder2);

  gfx::Point position1 = window1->bounds().CenterPoint();

  ui::GestureEventDetails begin_details(ui::EventType::kGesturePinchBegin);
  begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent begin(position1.x(), position1.y(), 0, ui::EventTimeForNow(),
                         begin_details);
  DispatchEventUsingWindowDispatcher(&begin);

  window2->SetCapture();

  ui::GestureEventDetails update_details(ui::EventType::kGesturePinchUpdate);
  update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent update(position1.x(), position1.y(), 0,
                          ui::EventTimeForNow(), update_details);
  DispatchEventUsingWindowDispatcher(&update);

  ui::GestureEventDetails end_details(ui::EventType::kGesturePinchEnd);
  end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent end(position1.x(), position1.y(), 0, ui::EventTimeForNow(),
                       end_details);
  DispatchEventUsingWindowDispatcher(&end);

  EXPECT_EQ("GESTURE_PINCH_BEGIN", EventTypesToString(recorder1.events()));

  EXPECT_EQ("GESTURE_PINCH_UPDATE GESTURE_PINCH_END",
            EventTypesToString(recorder2.events()));
  window1->RemovePreTargetHandler(&recorder1);
  window2->RemovePreTargetHandler(&recorder2);
}

// Places two windows side by side. Presses down on one window, and starts a
// scroll. Sets capture on the other window and ensures that the "ending" events
// aren't sent to the window which gained capture.
TEST_F(WindowEventDispatcherTest, EndingEventDoesntRetarget) {
  EventFilterRecorder recorder1;
  EventFilterRecorder recorder2;
  std::unique_ptr<Window> window1(CreateNormalWindow(1, root_window(), NULL));
  window1->SetBounds(gfx::Rect(0, 0, 40, 40));

  std::unique_ptr<Window> window2(CreateNormalWindow(2, root_window(), NULL));
  window2->SetBounds(gfx::Rect(40, 0, 40, 40));

  window1->AddPreTargetHandler(&recorder1);
  window2->AddPreTargetHandler(&recorder2);

  gfx::Point position = window1->bounds().origin();
  ui::TouchEvent press(ui::EventType::kTouchPressed, position,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);

  gfx::Point position2 = window1->bounds().CenterPoint();
  ui::TouchEvent move(ui::EventType::kTouchMoved, position2,
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move);

  window2->SetCapture();

  EXPECT_EQ(
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
      "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
      "TOUCH_CANCELLED GESTURE_SCROLL_END GESTURE_END",
      EventTypesToString(recorder1.events()));

  EXPECT_TRUE(recorder2.events().empty());

  window1->RemovePreTargetHandler(&recorder1);
  window2->RemovePreTargetHandler(&recorder2);
}

namespace {

// This class creates and manages a window which is destroyed as soon as
// capture is lost. This is the case for the drag and drop capture window.
class CaptureWindowTracker : public test::TestWindowDelegate {
 public:
  CaptureWindowTracker() {}

  CaptureWindowTracker(const CaptureWindowTracker&) = delete;
  CaptureWindowTracker& operator=(const CaptureWindowTracker&) = delete;

  ~CaptureWindowTracker() override {}

  void CreateCaptureWindow(aura::Window* root_window) {
    capture_window_.reset(test::CreateTestWindowWithDelegate(
        this, -1234, gfx::Rect(20, 20, 20, 20), root_window));
    capture_window_->SetCapture();
  }

  void reset() { capture_window_.reset(); }

  void OnCaptureLost() override { capture_window_.reset(); }

  void OnWindowDestroyed(Window* window) override {
    TestWindowDelegate::OnWindowDestroyed(window);
    capture_window_.reset();
  }

  aura::Window* capture_window() { return capture_window_.get(); }

 private:
  std::unique_ptr<aura::Window> capture_window_;
};

}  // namespace

// Verifies handling loss of capture by the capture window being hidden.
TEST_F(WindowEventDispatcherTest, CaptureWindowHidden) {
  CaptureWindowTracker capture_window_tracker;
  capture_window_tracker.CreateCaptureWindow(root_window());
  capture_window_tracker.capture_window()->Hide();
  EXPECT_EQ(NULL, capture_window_tracker.capture_window());
}

// Verifies handling loss of capture by the capture window being destroyed.
TEST_F(WindowEventDispatcherTest, CaptureWindowDestroyed) {
  CaptureWindowTracker capture_window_tracker;
  capture_window_tracker.CreateCaptureWindow(root_window());
  capture_window_tracker.reset();
  EXPECT_EQ(NULL, capture_window_tracker.capture_window());
}

namespace {

class RunLoopHandler : public ui::EventHandler {
 public:
  explicit RunLoopHandler(aura::Window* target)
      : run_loop_(base::RunLoop::Type::kNestableTasksAllowed), target_(target) {
    target_->AddPreTargetHandler(this);
  }

  RunLoopHandler(const RunLoopHandler&) = delete;
  RunLoopHandler& operator=(const RunLoopHandler&) = delete;

  ~RunLoopHandler() override { target_->RemovePreTargetHandler(this); }
  int num_scroll_updates() const { return num_scroll_updates_; }

 private:
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() != ui::EventType::kGestureScrollUpdate) {
      return;
    }
    num_scroll_updates_++;
    if (running_) {
      run_loop_.QuitWhenIdle();
    } else {
      running_ = true;
      run_loop_.Run();
    }
  }

  base::RunLoop run_loop_;
  bool running_ = false;
  int num_scroll_updates_ = 0;

  raw_ptr<aura::Window> target_;
};

}  // namespace

TEST_F(WindowEventDispatcherTest, HeldTouchMoveWithRunLoop) {
  RunLoopHandler handler(root_window());

  host()->dispatcher()->HoldPointerMoves();

  gfx::Point point = root_window()->GetBoundsInScreen().CenterPoint();
  ui::TouchEvent ev0(ui::EventType::kTouchPressed, point, ui::EventTimeForNow(),
                     ui::PointerDetails());
  DispatchEventUsingWindowDispatcher(&ev0);

  point.Offset(10, 10);
  ui::TouchEvent ev1(ui::EventType::kTouchMoved, point, ui::EventTimeForNow(),
                     ui::PointerDetails());
  DispatchEventUsingWindowDispatcher(&ev1);
  // The move event is held, so SCROLL_UPDATE does not happen yet.
  EXPECT_EQ(0, handler.num_scroll_updates());

  // ReleasePointerMoves() will post DispatchHeldEvent() asynchronously.
  host()->dispatcher()->ReleasePointerMoves();
  point.Offset(10, 10);
  // Schedule another move event which should cause another SCROLL_UPDATE and
  // quit the run_loop within the handler.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ui::TouchEvent ev2(ui::EventType::kTouchMoved, point,
                           base::TimeTicks::Now(), ui::PointerDetails());
        DispatchEventUsingWindowDispatcher(&ev2);
      }));
  // Wait for both DispatchHeldEvent() and dispatch of |ev2|.
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();

  // Makes sure that the run_loop ran and then ended.
  EXPECT_EQ(2, handler.num_scroll_updates());
}

class ExitMessageLoopOnMousePress : public ui::test::TestEventHandler {
 public:
  ExitMessageLoopOnMousePress() {}

  ExitMessageLoopOnMousePress(const ExitMessageLoopOnMousePress&) = delete;
  ExitMessageLoopOnMousePress& operator=(const ExitMessageLoopOnMousePress&) =
      delete;

  ~ExitMessageLoopOnMousePress() override {}

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 protected:
  void OnMouseEvent(ui::MouseEvent* event) override {
    ui::test::TestEventHandler::OnMouseEvent(event);
    if (event->type() == ui::EventType::kMousePressed) {
      if (!quit_closure_.is_null()) {
        std::move(quit_closure_).Run();
      }
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

class WindowEventDispatcherTestWithMessageLoop
    : public WindowEventDispatcherTest {
 public:
  WindowEventDispatcherTestWithMessageLoop() {}

  WindowEventDispatcherTestWithMessageLoop(
      const WindowEventDispatcherTestWithMessageLoop&) = delete;
  WindowEventDispatcherTestWithMessageLoop& operator=(
      const WindowEventDispatcherTestWithMessageLoop&) = delete;

  ~WindowEventDispatcherTestWithMessageLoop() override {}

  void RunTest(base::OnceClosure outer_loop_quit) {
    // Reset any event the window may have received when bringing up the window
    // (e.g. mouse-move events if the mouse cursor is over the window).
    handler_.Reset();

    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    handler_.set_quit_closure(std::move(outer_loop_quit));

    // Start a nested message-loop, post an event to be dispatched, and then
    // terminate the message-loop. When the message-loop unwinds and gets back,
    // the reposted event should not have fired.
    std::unique_ptr<ui::MouseEvent> mouse(new ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(10, 10), gfx::Point(10, 10),
        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WindowEventDispatcherTestWithMessageLoop::RepostEventHelper,
            host()->dispatcher(), std::move(mouse)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitWhenIdleClosure());

    loop.Run();
    EXPECT_EQ(0, handler_.num_mouse_events());

    // Let the current message-loop run. The event-handler will terminate the
    // message-loop when it receives the reposted event.
  }

 protected:
  void SetUp() override {
    WindowEventDispatcherTest::SetUp();
    window_.reset(CreateNormalWindow(1, root_window(), NULL));
    window_->AddPreTargetHandler(&handler_);
  }

  void TearDown() override {
    window_->RemovePreTargetHandler(&handler_);
    window_.reset();
    WindowEventDispatcherTest::TearDown();
  }

 private:
  // Used to avoid a copying |event| when binding to a closure.
  static void RepostEventHelper(WindowEventDispatcher* dispatcher,
                                std::unique_ptr<ui::MouseEvent> event) {
    dispatcher->RepostEvent(event.get());
  }

  std::unique_ptr<Window> window_;
  ExitMessageLoopOnMousePress handler_;
};

TEST_F(WindowEventDispatcherTestWithMessageLoop, EventRepostedInNonNestedLoop) {
  ASSERT_FALSE(base::RunLoop::IsRunningOnCurrentThread());
  // Perform the test in a callback, so that it runs after the message-loop
  // starts.
  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowEventDispatcherTestWithMessageLoop::RunTest,
                     base::Unretained(this), loop.QuitWhenIdleClosure()));
  loop.Run();
}

class WindowEventDispatcherTestInHighDPI : public WindowEventDispatcherTest {
 public:
  WindowEventDispatcherTestInHighDPI() {}
  ~WindowEventDispatcherTestInHighDPI() override {}

  void DispatchEvent(ui::Event* event) {
    DispatchEventUsingWindowDispatcher(event);
  }

 protected:
  void SetUp() override {
    WindowEventDispatcherTest::SetUp();
    test_screen()->SetDeviceScaleFactor(2.f);
  }
};

TEST_F(WindowEventDispatcherTestInHighDPI, EventLocationTransform) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> child(test::CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(20, 20, 100, 100), root_window()));
  child->Show();

  ui::test::TestEventHandler handler_child;
  ui::test::TestEventHandler handler_root;
  root_window()->AddPreTargetHandler(&handler_root);
  child->AddPreTargetHandler(&handler_child);

  {
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(30, 30),
                        gfx::Point(30, 30), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    DispatchEventUsingWindowDispatcher(&move);
    EXPECT_EQ(0, handler_child.num_mouse_events());
    EXPECT_EQ(1, handler_root.num_mouse_events());
  }

  {
    ui::MouseEvent move(ui::EventType::kMouseMoved, gfx::Point(50, 50),
                        gfx::Point(50, 50), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    DispatchEventUsingWindowDispatcher(&move);
    // The child receives an ENTER, and a MOVED event.
    EXPECT_EQ(2, handler_child.num_mouse_events());
    // The root receives both the ENTER and the MOVED events dispatched to
    // |child|, as well as an EXIT event.
    EXPECT_EQ(3, handler_root.num_mouse_events());
  }

  child->RemovePreTargetHandler(&handler_child);
  root_window()->RemovePreTargetHandler(&handler_root);
}

TEST_F(WindowEventDispatcherTestInHighDPI, TouchMovesHeldOnScroll) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);
  test::TestWindowDelegate delegate;
  HoldPointerOnScrollHandler handler(host()->dispatcher(), &recorder);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  window->AddPreTargetHandler(&handler);

  ui::test::EventGenerator generator(root_window());
  generator.GestureScrollSequence(gfx::Point(120, 120), gfx::Point(20, 120),
                                  base::Milliseconds(100), 25);

  // |handler| will have reset |filter| and started holding the touch-move
  // events when scrolling started. At the end of the scroll (i.e. upon
  // touch-release), the held touch-move event will have been dispatched first,
  // along with the subsequent events (i.e. touch-release, scroll-end, and
  // gesture-end).
  const EventFilterRecorder::Events& events = recorder.events();
  EXPECT_EQ(
      "TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
      "GESTURE_SCROLL_END GESTURE_END",
      EventTypesToString(events));
  ASSERT_EQ(2u, recorder.touch_locations().size());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[0].ToString());
  EXPECT_EQ(gfx::Point(-40, 10).ToString(),
            recorder.touch_locations()[1].ToString());
  root_window()->RemovePreTargetHandler(&recorder);
  window->RemovePreTargetHandler(&handler);
}

// This handler triggers a nested run loop when it receives a right click
// event, and runs a single callback in the nested run loop.
class TriggerNestedLoopOnRightMousePress : public ui::test::TestEventHandler {
 public:
  explicit TriggerNestedLoopOnRightMousePress(
      const base::RepeatingClosure& callback)
      : callback_(callback) {}

  TriggerNestedLoopOnRightMousePress(
      const TriggerNestedLoopOnRightMousePress&) = delete;
  TriggerNestedLoopOnRightMousePress& operator=(
      const TriggerNestedLoopOnRightMousePress&) = delete;

  ~TriggerNestedLoopOnRightMousePress() override {}

  const gfx::Point mouse_move_location() const { return mouse_move_location_; }

 private:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    TestEventHandler::OnMouseEvent(mouse);
    if (mouse->type() == ui::EventType::kMousePressed &&
        mouse->IsOnlyRightMouseButton()) {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      scoped_refptr<base::TaskRunner> task_runner =
          base::SingleThreadTaskRunner::GetCurrentDefault();
      if (!callback_.is_null())
        task_runner->PostTask(FROM_HERE, callback_);
      task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    } else if (mouse->type() == ui::EventType::kMouseMoved) {
      mouse_move_location_ = mouse->location();
    }
  }

  base::RepeatingClosure callback_;
  gfx::Point mouse_move_location_;
};

// Tests that if dispatching a 'held' event triggers a nested run loop, then
// the events that are dispatched from the nested run loop are transformed
// correctly.
TEST_F(WindowEventDispatcherTestInHighDPI,
       EventsTransformedInRepostedEventTriggeredNestedLoop) {
  std::unique_ptr<Window> window(CreateNormalWindow(1, root_window(), NULL));
  // Make sure the window is visible.
  RunAllPendingInMessageLoop();

  ui::MouseEvent mouse_move(ui::EventType::kMouseMoved, gfx::Point(80, 80),
                            gfx::Point(80, 80), ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  base::RepeatingClosure callback_on_right_click = base::BindRepeating(
      base::IgnoreResult(&WindowEventDispatcherTestInHighDPI::DispatchEvent),
      base::Unretained(this), base::Unretained(&mouse_move));
  TriggerNestedLoopOnRightMousePress handler(callback_on_right_click);
  window->AddPreTargetHandler(&handler);

  std::unique_ptr<ui::MouseEvent> mouse(
      new ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(10, 10),
                         gfx::Point(10, 10), ui::EventTimeForNow(),
                         ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  host()->dispatcher()->RepostEvent(mouse.get());
  EXPECT_EQ(0, handler.num_mouse_events());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // The window should receive the mouse-press and the mouse-move events.
  EXPECT_EQ(2, handler.num_mouse_events());
  // The mouse-move event location should be transformed because of the DSF
  // before it reaches the window.
  EXPECT_EQ(gfx::Point(40, 40).ToString(),
            handler.mouse_move_location().ToString());
  EXPECT_EQ(gfx::Point(40, 40).ToString(),
            Env::GetInstance()->last_mouse_location().ToString());
  window->RemovePreTargetHandler(&handler);
}

class SelfDestructDelegate : public test::TestWindowDelegate {
 public:
  SelfDestructDelegate() {}

  SelfDestructDelegate(const SelfDestructDelegate&) = delete;
  SelfDestructDelegate& operator=(const SelfDestructDelegate&) = delete;

  ~SelfDestructDelegate() override {}

  void OnMouseEvent(ui::MouseEvent* event) override { window_.reset(); }

  void set_window(std::unique_ptr<aura::Window> window) {
    window_ = std::move(window);
  }
  bool has_window() const { return !!window_.get(); }

 private:
  std::unique_ptr<aura::Window> window_;
};

TEST_F(WindowEventDispatcherTest, SynthesizedLocatedEvent) {
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(10, 10);
  EXPECT_EQ("10,10", Env::GetInstance()->last_mouse_location().ToString());

  // Synthesized event should not update the mouse location.
  ui::MouseEvent mouseev(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0);
  generator.Dispatch(&mouseev);
  EXPECT_EQ("10,10", Env::GetInstance()->last_mouse_location().ToString());

  generator.MoveMouseTo(0, 0);
  EXPECT_EQ("0,0", Env::GetInstance()->last_mouse_location().ToString());

  // Make sure the location gets updated when a syntheiszed enter
  // event destroyed the window.
  SelfDestructDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(50, 50, 100, 100), root_window()));
  delegate.set_window(std::move(window));
  EXPECT_TRUE(delegate.has_window());

  generator.MoveMouseTo(100, 100);
  EXPECT_FALSE(delegate.has_window());
  EXPECT_EQ("100,100", Env::GetInstance()->last_mouse_location().ToString());
}

// Tests that the window which has capture can get destroyed as a result of
// ui::EventType::kMouseCaptureChanged event dispatched in
// WindowEventDispatcher::UpdateCapture without causing a "use after free".
TEST_F(WindowEventDispatcherTest, DestroyWindowOnCaptureChanged) {
  SelfDestructDelegate delegate;
  std::unique_ptr<aura::Window> window_first(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(20, 10, 10, 20), root_window()));
  Window* window_first_raw = window_first.get();
  window_first->Show();
  window_first->SetCapture();
  delegate.set_window(std::move(window_first));
  EXPECT_TRUE(delegate.has_window());

  std::unique_ptr<aura::Window> window_second(
      test::CreateTestWindowWithId(2, root_window()));
  window_second->Show();

  client::CaptureDelegate* capture_delegate = host()->dispatcher();
  capture_delegate->UpdateCapture(window_first_raw, window_second.get());
  EXPECT_FALSE(delegate.has_window());
}

class StaticFocusClient : public client::FocusClient {
 public:
  explicit StaticFocusClient(Window* focused) : focused_(focused) {}

  StaticFocusClient(const StaticFocusClient&) = delete;
  StaticFocusClient& operator=(const StaticFocusClient&) = delete;

  ~StaticFocusClient() override = default;

 private:
  // client::FocusClient:
  void AddObserver(client::FocusChangeObserver* observer) override {}
  void RemoveObserver(client::FocusChangeObserver* observer) override {}
  void FocusWindow(Window* window) override {}
  void ResetFocusWithinActiveWindow(Window* window) override {}
  Window* GetFocusedWindow() override { return focused_; }

  raw_ptr<Window> focused_;
};

// Tests that host-cancel-mode event can be dispatched to a dispatcher safely
// when the focused window does not live in the dispatcher's tree.
TEST_F(WindowEventDispatcherTest, HostCancelModeWithFocusedWindowOutside) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> focused(CreateTestWindowWithDelegate(
      &delegate, 123, gfx::Rect(20, 30, 100, 50), NULL));
  StaticFocusClient focus_client(focused.get());
  client::SetFocusClient(root_window(), &focus_client);
  EXPECT_FALSE(root_window()->Contains(focused.get()));
  EXPECT_EQ(focused.get(),
            client::GetFocusClient(root_window())->GetFocusedWindow());
  host()->dispatcher()->DispatchCancelModeEvent();
  EXPECT_EQ(focused.get(),
            client::GetFocusClient(root_window())->GetFocusedWindow());
}

// Dispatches a mouse-move event to |target| when it receives a mouse-move
// event.
class DispatchEventHandler : public ui::EventHandler {
 public:
  explicit DispatchEventHandler(Window* target)
      : target_(target), dispatched_(false) {}

  DispatchEventHandler(const DispatchEventHandler&) = delete;
  DispatchEventHandler& operator=(const DispatchEventHandler&) = delete;

  ~DispatchEventHandler() override = default;

  bool dispatched() const { return dispatched_; }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    if (mouse->type() == ui::EventType::kMouseMoved) {
      ui::MouseEvent move(ui::EventType::kMouseMoved,
                          target_->bounds().CenterPoint(),
                          target_->bounds().CenterPoint(),
                          ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
      ui::EventDispatchDetails details =
          target_->GetHost()->dispatcher()->OnEventFromSource(&move);
      ASSERT_FALSE(details.dispatcher_destroyed);
      EXPECT_FALSE(details.target_destroyed);
      EXPECT_EQ(target_, move.target());
      dispatched_ = true;
    }
    ui::EventHandler::OnMouseEvent(mouse);
  }

  raw_ptr<Window> target_;
  bool dispatched_;
};

// Moves |window| to |root_window| when it receives a mouse-move event.
class MoveWindowHandler : public ui::EventHandler {
 public:
  MoveWindowHandler(Window* window, Window* root_window)
      : window_to_move_(window), root_window_to_move_to_(root_window) {}

  MoveWindowHandler(const MoveWindowHandler&) = delete;
  MoveWindowHandler& operator=(const MoveWindowHandler&) = delete;

  ~MoveWindowHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    if (mouse->type() == ui::EventType::kMouseMoved) {
      root_window_to_move_to_->AddChild(window_to_move_);
    }
    ui::EventHandler::OnMouseEvent(mouse);
  }

  raw_ptr<Window> window_to_move_;
  raw_ptr<Window> root_window_to_move_to_;
};

// Tests that nested event dispatch works correctly if the target of the older
// event being dispatched is moved to a different dispatcher in response to an
// event in the inner loop.
TEST_F(WindowEventDispatcherTest, NestedEventDispatchTargetMoved) {
  std::unique_ptr<WindowTreeHost> second_host = WindowTreeHost::Create(
      ui::PlatformWindowInitProperties{gfx::Rect(20, 30, 100, 50)});
  second_host->InitHost();
  second_host->window()->Show();
  Window* second_root = second_host->window();

  // Create two windows parented to |root_window()|.
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> first(CreateTestWindowWithDelegate(
      &delegate, 123, gfx::Rect(20, 10, 10, 20), root_window()));
  std::unique_ptr<Window> second(CreateTestWindowWithDelegate(
      &delegate, 234, gfx::Rect(40, 10, 50, 20), root_window()));

  // Setup a handler on |first| so that it dispatches an event to |second| when
  // |first| receives an event.
  DispatchEventHandler dispatch_event(second.get());
  first->AddPreTargetHandler(&dispatch_event);

  // Setup a handler on |second| so that it moves |first| into |second_root|
  // when |second| receives an event.
  MoveWindowHandler move_window(first.get(), second_root);
  second->AddPreTargetHandler(&move_window);

  // Some sanity checks: |first| is inside |root_window()|'s tree.
  EXPECT_EQ(root_window(), first->GetRootWindow());
  // The two root windows are different.
  EXPECT_NE(root_window(), second_root);

  // Dispatch an event to |first|.
  ui::MouseEvent move(ui::EventType::kMouseMoved, first->bounds().CenterPoint(),
                      first->bounds().CenterPoint(), ui::EventTimeForNow(),
                      ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      host()->dispatcher()->OnEventFromSource(&move);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_EQ(first.get(), move.target());
  EXPECT_TRUE(dispatch_event.dispatched());
  EXPECT_EQ(second_root, first->GetRootWindow());

  first->RemovePreTargetHandler(&dispatch_event);
  second->RemovePreTargetHandler(&move_window);
}

class AlwaysMouseDownInputStateLookup : public InputStateLookup {
 public:
  AlwaysMouseDownInputStateLookup() {}

  AlwaysMouseDownInputStateLookup(const AlwaysMouseDownInputStateLookup&) =
      delete;
  AlwaysMouseDownInputStateLookup& operator=(
      const AlwaysMouseDownInputStateLookup&) = delete;

  ~AlwaysMouseDownInputStateLookup() override {}

 private:
  // InputStateLookup:
  bool IsMouseButtonDown() const override { return true; }
};

TEST_F(WindowEventDispatcherTest,
       CursorVisibilityChangedWhileCaptureWindowInAnotherDispatcher) {
  test::EventCountDelegate delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 123, gfx::Rect(20, 10, 10, 20), root_window()));
  window->Show();

  std::unique_ptr<WindowTreeHost> second_host = WindowTreeHost::Create(
      ui::PlatformWindowInitProperties{gfx::Rect(20, 30, 100, 50)});
  second_host->InitHost();
  second_host->window()->Show();
  WindowEventDispatcher* second_dispatcher = second_host->dispatcher();

  // Install an InputStateLookup on the Env that always claims that a
  // mouse-button is down.
  test::EnvTestHelper(Env::GetInstance())
      .SetInputStateLookup(std::unique_ptr<InputStateLookup>(
          new AlwaysMouseDownInputStateLookup()));

  window->SetCapture();

  // Because the mouse button is down, setting the capture on |window| will set
  // it as the mouse-move handler for |root_window()|.
  EXPECT_EQ(window.get(), host()->dispatcher()->mouse_moved_handler());

  // This does not set |window| as the mouse-move handler for the second
  // dispatcher.
  EXPECT_EQ(NULL, second_dispatcher->mouse_moved_handler());

  // However, some capture-client updates the capture in each root-window on a
  // capture. Emulate that here. Because of this, the second dispatcher also has
  // |window| as the mouse-move handler.
  client::CaptureDelegate* second_capture_delegate = second_dispatcher;
  second_capture_delegate->UpdateCapture(NULL, window.get());
  EXPECT_EQ(window.get(), second_dispatcher->mouse_moved_handler());

  // Reset the mouse-event counts for |window|.
  delegate.GetMouseMotionCountsAndReset();

  // Notify both hosts that the cursor is now hidden. This should send a single
  // mouse-exit event to |window|.
  host()->OnCursorVisibilityChanged(false);
  second_host->OnCursorVisibilityChanged(false);
  EXPECT_EQ("0 0 1", delegate.GetMouseMotionCountsAndReset());
}

TEST_F(WindowEventDispatcherTest,
       RedirectedEventToDifferentDispatcherLocation) {
  std::unique_ptr<WindowTreeHost> second_host = WindowTreeHost::Create(
      ui::PlatformWindowInitProperties{gfx::Rect(20, 30, 100, 50)});
  second_host->InitHost();
  second_host->window()->Show();

  // AuraTestBase sets up a DefaultCaptureClient for root_window(), but that
  // can't deal with capture between different root windows.  Instead we need to
  // use the wm::CaptureController instance that also exists, which can handle
  // this situation.  Exchange the capture clients and put the old one back at
  // the end.
  client::CaptureClient* const old_capture_client =
      client::GetCaptureClient(root_window());
  {
    wm::ScopedCaptureClient scoped_capture_first(root_window());
    wm::ScopedCaptureClient scoped_capture_second(second_host->window());

    test::EventCountDelegate delegate;
    std::unique_ptr<Window> window_first(CreateTestWindowWithDelegate(
        &delegate, 123, gfx::Rect(20, 10, 10, 20), root_window()));
    window_first->Show();

    std::unique_ptr<Window> window_second(CreateTestWindowWithDelegate(
        &delegate, 12, gfx::Rect(10, 10, 20, 30), second_host->window()));
    window_second->Show();

    window_second->SetCapture();

    // Send an event to the first host. Make sure it goes to |window_second| in
    // |second_host| instead (since it has capture).
    EventFilterRecorder recorder_first;
    window_first->AddPreTargetHandler(&recorder_first);
    EventFilterRecorder recorder_second;
    window_second->AddPreTargetHandler(&recorder_second);
    const gfx::Point event_location(25, 15);
    ui::MouseEvent mouse(ui::EventType::kMousePressed, event_location,
                         event_location, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    DispatchEventUsingWindowDispatcher(&mouse);
    EXPECT_TRUE(recorder_first.events().empty());
    ASSERT_EQ(1u, recorder_second.events().size());
    EXPECT_EQ(ui::EventType::kMousePressed, recorder_second.events()[0]);
    EXPECT_EQ(event_location.ToString(),
              recorder_second.mouse_locations()[0].ToString());
    window_first->RemovePreTargetHandler(&recorder_first);
    window_second->RemovePreTargetHandler(&recorder_second);
  }
  client::SetCaptureClient(root_window(), old_capture_client);
}

class AsyncWindowDelegate : public test::TestWindowDelegate {
 public:
  AsyncWindowDelegate(WindowEventDispatcher* dispatcher)
      : dispatcher_(dispatcher), window_(nullptr) {}

  AsyncWindowDelegate(const AsyncWindowDelegate&) = delete;
  AsyncWindowDelegate& operator=(const AsyncWindowDelegate&) = delete;

  void set_window(Window* window) { window_ = window; }

 private:
  void OnTouchEvent(ui::TouchEvent* event) override {
    // Convert touch event back to root window coordinates.
    event->ConvertLocationToTarget(window_.get(), window_->GetRootWindow());
    event->DisableSynchronousHandling();
    dispatcher_->ProcessedTouchEvent(
        event->unique_event_id(), window_, ui::ER_UNHANDLED,
        false /* is_source_touch_event_set_blocking */);
    event->StopPropagation();
  }

  raw_ptr<WindowEventDispatcher> dispatcher_;
  raw_ptr<Window, AcrossTasksDanglingUntriaged> window_;
};

// Tests that gesture events dispatched through the asynchronous flow have
// co-ordinates in the right co-ordinate space.
TEST_F(WindowEventDispatcherTest, GestureEventCoordinates) {
  const float kX = 67.3f;
  const float kY = 97.8f;

  const int kWindowOffset = 50;
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);
  AsyncWindowDelegate delegate(host()->dispatcher());
  HoldPointerOnScrollHandler handler(host()->dispatcher(), &recorder);
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(kWindowOffset, kWindowOffset, 100, 100),
      root_window()));
  window->AddPreTargetHandler(&handler);

  delegate.set_window(window.get());

  ui::TouchEvent touch_pressed_event(
      ui::EventType::kTouchPressed, gfx::Point(), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  touch_pressed_event.set_location_f(gfx::PointF(kX, kY));
  touch_pressed_event.set_root_location_f(gfx::PointF(kX, kY));

  DispatchEventUsingWindowDispatcher(&touch_pressed_event);

  ASSERT_EQ(1u, recorder.touch_locations().size());
  EXPECT_EQ(gfx::Point(kX - kWindowOffset, kY - kWindowOffset).ToString(),
            recorder.touch_locations()[0].ToString());

  ASSERT_EQ(2u, recorder.gesture_locations().size());
  EXPECT_EQ(gfx::Point(kX - kWindowOffset, kY - kWindowOffset).ToString(),
            recorder.gesture_locations()[0].ToString());
  root_window()->RemovePreTargetHandler(&recorder);
  window->RemovePreTargetHandler(&handler);
}

// Tests that a scroll-generating touch-event is marked as such.
TEST_F(WindowEventDispatcherTest, TouchMovesMarkedWhenCausingScroll) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  const gfx::Point location(20, 20);
  ui::TouchEvent press(ui::EventType::kTouchPressed, location,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_FALSE(recorder.LastTouchMayCauseScrolling());
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchPressed));
  recorder.Reset();

  ui::TouchEvent move(ui::EventType::kTouchMoved,
                      location + gfx::Vector2d(100, 100), ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_TRUE(recorder.LastTouchMayCauseScrolling());
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchMoved));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollBegin));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollUpdate));
  recorder.Reset();

  ui::TouchEvent move2(ui::EventType::kTouchMoved,
                       location + gfx::Vector2d(200, 200),
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_TRUE(recorder.LastTouchMayCauseScrolling());
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchMoved));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollUpdate));
  recorder.Reset();

  // Delay the release to avoid fling generation.
  ui::TouchEvent release(ui::EventType::kTouchReleased,
                         location + gfx::Vector2d(200, 200),
                         ui::EventTimeForNow() + base::Seconds(1),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_TRUE(recorder.LastTouchMayCauseScrolling());
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kTouchReleased));
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kGestureScrollEnd));

  root_window()->RemovePreTargetHandler(&recorder);
}

// OnCursorMovedToRootLocation() is sometimes called instead of
// WindowTreeHost::MoveCursorTo() when the cursor did not move but the
// cursor's position in root coordinates has changed (e.g. when the displays's
// scale factor changed). Test that hover effects are properly updated.
TEST_F(WindowEventDispatcherTest, OnCursorMovedToRootLocationUpdatesHover) {
  WindowEventDispatcher* dispatcher = host()->dispatcher();
  test::TestCursorClient cursor_client(root_window());
  cursor_client.ShowCursor();

  std::unique_ptr<Window> w(CreateNormalWindow(1, root_window(), nullptr));
  w->SetBounds(gfx::Rect(20, 20, 20, 20));
  w->Show();

  // Move the cursor off of |w|.
  dispatcher->OnCursorMovedToRootLocation(gfx::Point(100, 100));

  EventFilterRecorder recorder;
  w->AddPreTargetHandler(&recorder);
  dispatcher->OnCursorMovedToRootLocation(gfx::Point(22, 22));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kMouseEntered));
  recorder.Reset();

  // The cursor should not be over |w| after changing the device scale factor to
  // 2x. A EventType::kMouseExited event should have been sent to |w|.
  test_screen()->SetDeviceScaleFactor(2.f);
  dispatcher->OnCursorMovedToRootLocation(gfx::Point(11, 11));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kMouseExited));
  recorder.Reset();

  // Hide the cursor, synthetic event will not be sent.
  cursor_client.HideCursor();
  dispatcher->OnCursorMovedToRootLocation(gfx::Point(22, 22));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.events().empty());

  // Cursor is hidden when locked, but synthetic move event still be dispatched.
  cursor_client.LockCursor();
  dispatcher->OnCursorMovedToRootLocation(gfx::Point(33, 33));
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(recorder.HasReceivedEvent(ui::EventType::kMouseMoved));
  recorder.Reset();

  w->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, TouchEventWithScaledWindow) {
  WindowEventDispatcher* dispatcher = host()->dispatcher();

  EventFilterRecorder root_recorder;
  root_window()->AddPreTargetHandler(&root_recorder);

  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> child(
      CreateNormalWindow(1, root_window(), &delegate));

  const gfx::Point child_position(-10, -10);
  const gfx::Rect& root_bounds = root_window()->bounds();
  gfx::Rect child_bounds(child_position,
                         gfx::ScaleToCeiledSize(root_bounds.size(), 2));
  child->SetBounds(child_bounds);
  gfx::Transform transform;
  transform.Scale(0.5, 0.5);
  child->SetTransform(transform);

  EventFilterRecorder child_recorder;
  child->AddPreTargetHandler(&child_recorder);

  std::string expected_events =
      "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_RELEASED "
      "GESTURE_SHOW_PRESS GESTURE_TAP GESTURE_END";
  {
    // Touch events are outside of the root window, but inside of the child
    // window.
    const gfx::Point touch_position(-5, -5);
    ui::TouchEvent pressed_event(
        ui::EventType::kTouchPressed, touch_position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    ui::TouchEvent released_event(
        ui::EventType::kTouchReleased, touch_position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    dispatcher->OnEventFromSource(&pressed_event);
    dispatcher->OnEventFromSource(&released_event);
    EXPECT_EQ(expected_events, EventTypesToString(root_recorder.events()));
    EXPECT_EQ("", EventTypesToString(child_recorder.events()));
    root_recorder.Reset();
    child_recorder.Reset();
  }

  {
    // |touch_position| value is in the bounds of both the root window and the
    // child window.
    const gfx::Point touch_position(5, 5);
    ui::TouchEvent pressed_event(
        ui::EventType::kTouchPressed, touch_position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    ui::TouchEvent released_event(
        ui::EventType::kTouchReleased, touch_position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    dispatcher->OnEventFromSource(&pressed_event);
    dispatcher->OnEventFromSource(&released_event);
    EXPECT_EQ(expected_events, EventTypesToString(root_recorder.events()));
    EXPECT_EQ(expected_events, EventTypesToString(child_recorder.events()));
    root_recorder.Reset();
    child_recorder.Reset();
  }

  child->RemovePreTargetHandler(&child_recorder);
  root_window()->RemovePreTargetHandler(&root_recorder);
}

// A test case for crbug.com/1099985
TEST_F(WindowEventDispatcherTest, TargetIsDestroyedByHeldEvent) {
  EventFilterRecorder recorder;
  root_window()->AddPreTargetHandler(&recorder);

  // Create a window which should be a target of all MouseEvent in this tests.
  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> mouse_target(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  // Create a window which has a focus, so should receive all KeyEvents.
  ConsumeKeyHandler key_handler;
  // Not using std::unique_ptr<> intentionally
  aura::Window* focused(test::CreateTestWindowWithBounds(
      gfx::Rect(200, 200, 100, 100), root_window()));
  focused->SetProperty(client::kSkipImeProcessing, true);
  focused->AddPostTargetHandler(&key_handler);
  focused->Show();
  focused->Focus();

  // Make sure that the key event goes to the |focused| window.
  ui::KeyEvent key_press(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&key_press);
  EXPECT_EQ(1, key_handler.num_key_events());
  key_handler.Reset();

  ui::MouseEvent mouse_move_event(ui::EventType::kMouseMoved, gfx::Point(1, 1),
                                  gfx::Point(1, 1), ui::EventTimeForNow(), 0,
                                  0);
  DispatchEventUsingWindowDispatcher(&mouse_move_event);
  // Discard MOUSE_ENTER.
  recorder.Reset();

  host()->dispatcher()->HoldPointerMoves();

  // The dragged event should not be sent to the |target| window because
  // WindowEventDispatcher is holding it now.
  ui::MouseEvent mouse_dragged_event(ui::EventType::kMouseDragged,
                                     gfx::Point(0, 0), gfx::Point(0, 0),
                                     ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&mouse_dragged_event);
  EXPECT_TRUE(recorder.events().empty());

  // Create a event handler which destroys the |focused| window when it sees any
  // mouse event.
  class Handler : public ui::test::TestEventHandler {
   public:
    explicit Handler(aura::Window* focused) : focused_(focused) {}
    ~Handler() override = default;

    Handler(const Handler&) = delete;
    Handler& operator=(const Handler&) = delete;

    // Overridden from ui::EventHandler:
    void OnMouseEvent(ui::MouseEvent* event) override {
      ui::test::TestEventHandler::OnMouseEvent(event);
      LOG(ERROR) << "|focused_| is being deleted";
      // !!!
      delete focused_;
    }

   private:
    raw_ptr<aura::Window, AcrossTasksDanglingUntriaged> focused_;
  };
  Handler mouse_handler(focused);
  mouse_target->AddPostTargetHandler(&mouse_handler);

  // Sending a key event should stop the hold and the mouse event goes to the
  // |target| window.
  // The key event should not be sent to the handler because the focused window
  // is destroyed before the event is dispatched.
  ui::KeyEvent key_press2(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&key_press2);
  EXPECT_EQ(1u, recorder.events().size());
  EXPECT_EQ(0, key_handler.num_key_events());
  EXPECT_EQ(1, mouse_handler.num_mouse_events());

  root_window()->RemovePreTargetHandler(&recorder);
}

// Tests that touch event can be filtered by `StopPropagation`, but can still
// be processed by GestureRecogtnizer with `ForceProcessGesture`.
TEST_F(WindowEventDispatcherTest, FilteredTouchProcessGesture) {
  // A event handler that stops propagation, but still allow gesture
  // processing.
  class : public ui::EventHandler {
   public:
    void OnTouchEvent(ui::TouchEvent* event) override {
      event->StopPropagation();
      event->ForceProcessGesture();
    }
  } handler;

  root_window()->AddPreTargetHandler(&handler);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(test::CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(100, 100), root_window()));

  EventFilterRecorder recorder;
  window->AddPreTargetHandler(&recorder);

  ui::test::EventGenerator generator(root_window());

  generator.PressTouch(gfx::Point(50, 50));
  generator.ReleaseTouch();

  EXPECT_EQ(0u, recorder.touch_locations().size());
  EXPECT_EQ(5u, recorder.gesture_locations().size());
  EXPECT_EQ(gfx::Point(50, 50), recorder.gesture_locations()[0]);

  root_window()->RemovePreTargetHandler(&handler);
  window->RemovePreTargetHandler(&recorder);
}

TEST_F(WindowEventDispatcherTest, LastTouchPoint) {
  class : public ui::EventHandler {
   public:
    void OnTouchEvent(ui::TouchEvent* event) override { event->SetHandled(); }
  } skip_gesture_handler;
  auto* env = Env::GetInstance();
  env->AddPreTargetHandler(&skip_gesture_handler);

  test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(10, 10, 100, 100), root_window()));

  constexpr gfx::Point fallback(-100, -100);
  EXPECT_EQ(fallback, GetLastTouchPoint(root_window(), fallback));

  constexpr gfx::Point location1(20, 20);
  ui::TouchEvent pressed(ui::EventType::kTouchPressed, location1,
                         ui::EventTimeForNow(),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&pressed);

  EXPECT_EQ(location1, GetLastTouchPoint(window.get(), fallback));
  EXPECT_EQ(fallback, GetLastTouchPoint(root_window(), fallback));

  constexpr gfx::Point location2(30, 30);
  ui::TouchEvent move(ui::EventType::kTouchMoved, location2,
                      ui::EventTimeForNow(),
                      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_EQ(location2, GetLastTouchPoint(window.get(), fallback));
  EXPECT_EQ(fallback, GetLastTouchPoint(root_window(), fallback));

  constexpr gfx::Point location3(00, 00);
  ui::TouchEvent move2(ui::EventType::kTouchMoved, location3,
                       ui::EventTimeForNow(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_EQ(location3, GetLastTouchPoint(window.get(), fallback));
  EXPECT_EQ(fallback, GetLastTouchPoint(root_window(), fallback));

  // Delay the release to avoid fling generation.
  ui::TouchEvent release(ui::EventType::kTouchReleased, location3,
                         ui::EventTimeForNow() + base::Seconds(1),
                         ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  DispatchEventUsingWindowDispatcher(&release);

  EXPECT_EQ(fallback, GetLastTouchPoint(root_window(), fallback));
  EXPECT_EQ(fallback, GetLastTouchPoint(window.get(), fallback));

  env->RemovePreTargetHandler(&skip_gesture_handler);
}

}  // namespace aura
