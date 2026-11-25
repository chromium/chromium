// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_pointer_grab.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_loop_handler.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

namespace ui {

namespace {

using ::testing::_;

constexpr int kPointerDeviceId = 1;

ACTION_P(StoreWidget, widget_ptr) {
  *widget_ptr = arg0;
}

ACTION_P(CloneEvent, event_ptr) {
  *event_ptr = arg0->Clone();
}

// TestScreen implementation. We need to set a screen instance, because
// X11Window requires it. And as long as depending on views is a dependency
// violation, keep own implementation here. Otherwise, we could just use
// ScreenOzone, but it is impossible.
// We are not really interested in sending back real displays. Thus, default one
// is more than enough.
class TestScreen : public display::ScreenBase {
 public:
  TestScreen() {
    ProcessDisplayChanged(display::Display(display::kDefaultDisplayId), true);
  }
  ~TestScreen() override = default;
  TestScreen(const TestScreen& screen) = delete;
  TestScreen& operator=(const TestScreen& screen) = delete;

  void SetScaleAndBoundsForPrimaryDisplay(float scale,
                                          const gfx::Rect& bounds_in_pixels) {
    auto display = GetPrimaryDisplay();
    display.SetScaleAndBounds(scale, bounds_in_pixels);
    ProcessDisplayChanged(display, true);
  }
};

}  // namespace

class X11WindowOzoneTest : public testing::Test {
 public:
  X11WindowOzoneTest()
      : task_env_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI)) {}

  X11WindowOzoneTest(const X11WindowOzoneTest&) = delete;
  X11WindowOzoneTest& operator=(const X11WindowOzoneTest&) = delete;

  ~X11WindowOzoneTest() override = default;

  void SetUp() override {
    event_source_ = std::make_unique<X11EventSource>(x11::Connection::Get());

    display::Screen::SetScreenInstance(&test_screen_);

    TouchFactory::GetInstance()->SetPointerDeviceForTest({kPointerDeviceId});
  }

  void TearDown() override { display::Screen::SetScreenInstance(nullptr); }

 protected:
  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      MockPlatformWindowDelegate* delegate,
      const gfx::Rect& bounds,
      gfx::AcceleratedWidget* widget,
      X11ExtensionDelegate* x11_extension_delegate) {
    EXPECT_CALL(*delegate, OnAcceleratedWidgetAvailable(_))
        .WillOnce(StoreWidget(widget));
    PlatformWindowInitProperties init_params(bounds);
    init_params.x11_extension_delegate = x11_extension_delegate;
    auto window = std::make_unique<X11Window>(delegate);
    window->Initialize(std::move(init_params));
    return std::move(window);
  }

  void DispatchXEvent(x11::Event* event, gfx::AcceleratedWidget widget) {
    auto* device_event = event->As<x11::Input::DeviceEvent>();
    DCHECK(device_event);
    device_event->event = static_cast<x11::Window>(widget);
    x11::Connection::Get()->DispatchEvent(*event);
  }

  X11WindowManager* window_manager() const {
    auto* window_manager = X11WindowManager::GetInstance();
    DCHECK(window_manager);
    return window_manager;
  }

  TestScreen test_screen_;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_env_;
  std::unique_ptr<X11EventSource> event_source_;
};

// This test ensures that events are handled by a right target(widget).
TEST_F(X11WindowOzoneTest, SendPlatformEventToRightTarget) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget, nullptr);

  ScopedXI2Event xi_event;
  xi_event.InitGenericButtonEvent(kPointerDeviceId, EventType::kMousePressed,
                                  gfx::Point(218, 290), EF_NONE);

  // First check events can be received by a target window.
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget);
  EXPECT_EQ(EventType::kMousePressed, event->type());
  testing::Mock::VerifyAndClearExpectations(&delegate);

  MockPlatformWindowDelegate delegate_2;
  gfx::AcceleratedWidget widget_2;
  gfx::Rect bounds_2(525, 155, 296, 407);

  auto window_2 =
      CreatePlatformWindow(&delegate_2, bounds_2, &widget_2, nullptr);

  // Check event goes to right target without capture being set.
  event.reset();
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);
  EXPECT_CALL(delegate_2, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget_2);
  EXPECT_EQ(EventType::kMousePressed, event->type());

  EXPECT_CALL(delegate, OnClosed()).Times(1);
  EXPECT_CALL(delegate_2, OnClosed()).Times(1);
}

// This test case ensures that events are consumed by a window with explicit
// capture, even though the event is sent to other window.
TEST_F(X11WindowOzoneTest, SendPlatformEventToCapturedWindow) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  EXPECT_CALL(delegate, OnClosed()).Times(1);
  auto window = CreatePlatformWindow(&delegate, bounds, &widget, nullptr);

  MockPlatformWindowDelegate delegate_2;
  gfx::AcceleratedWidget widget_2;
  gfx::Rect bounds_2(525, 155, 296, 407);
  EXPECT_CALL(delegate_2, OnClosed()).Times(1);
  auto window_2 =
      CreatePlatformWindow(&delegate_2, bounds_2, &widget_2, nullptr);

  ScopedXI2Event xi_event;
  xi_event.InitGenericButtonEvent(kPointerDeviceId, EventType::kMousePressed,
                                  gfx::Point(218, 290), EF_NONE);

  // Set capture to the second window, but send an event to another window
  // target. The event must have its location converted and received by the
  // captured window instead.
  window_2->SetCapture();
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);
  EXPECT_CALL(delegate_2, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  DispatchXEvent(xi_event, widget);
  EXPECT_TRUE(event.get());
  EXPECT_EQ(EventType::kMousePressed, event->type());
  EXPECT_EQ(gfx::Point(-277, 215), event->AsLocatedEvent()->location());
}

// XInput 2.4 touchpad pinch events must be dispatched as touchpad pinch
// gestures whose scale is relative to the previous update.
TEST_F(X11WindowOzoneTest, TouchpadPinch) {
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  auto window = CreatePlatformWindow(&delegate, gfx::Rect(30, 80, 800, 600),
                                     &widget, nullptr);

  std::vector<std::pair<EventType, float>> received;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillRepeatedly([&](Event* event) {
    ASSERT_TRUE(event->IsGestureEvent());
    auto* gesture = event->AsGestureEvent();
    EXPECT_EQ(GestureDeviceType::DEVICE_TOUCHPAD,
              gesture->details().device_type());
    EXPECT_EQ(gfx::PointF(100, 50), gesture->location_f());
    received.emplace_back(gesture->type(),
                          gesture->type() == EventType::kGesturePinchUpdate
                              ? gesture->details().scale()
                              : 0.0f);
  });

  // XInput 2 sends coordinates and the pinch scale as FP1616 (16.16 fixed
  // point) values, as it does for all XI2 device and gesture events.
  auto to_fp1616 = [](double value) {
    return static_cast<x11::Input::Fp1616>(value * (1 << 16));
  };
  auto dispatch_pinch = [&](x11::Input::GesturePinchEvent::Opcode opcode,
                            uint16_t source_id, double scale) {
    x11::Input::GesturePinchEvent pinch{
        .opcode = opcode,
        .event = static_cast<x11::Window>(widget),
        .event_x = to_fp1616(100),
        .event_y = to_fp1616(50),
        .scale = to_fp1616(scale),
        .sourceid = static_cast<x11::Input::DeviceId>(source_id),
    };
    x11::Connection::Get()->DispatchEvent(x11::Event(false, std::move(pinch)));
  };
  // Pinches on two touchpads overlap and must be tracked separately.
  dispatch_pinch(x11::Input::GesturePinchEvent::Begin, 2, 1.0);
  dispatch_pinch(x11::Input::GesturePinchEvent::Update, 2, 2.0);
  dispatch_pinch(x11::Input::GesturePinchEvent::Begin, 3, 1.0);
  dispatch_pinch(x11::Input::GesturePinchEvent::Update, 3, 0.5);
  dispatch_pinch(x11::Input::GesturePinchEvent::Update, 2, 3.0);
  dispatch_pinch(x11::Input::GesturePinchEvent::End, 2, 3.0);
  dispatch_pinch(x11::Input::GesturePinchEvent::End, 3, 0.5);

  EXPECT_EQ((std::vector<std::pair<EventType, float>>{
                {EventType::kGesturePinchBegin, 0.0f},
                {EventType::kGesturePinchUpdate, 2.0f},
                {EventType::kGesturePinchBegin, 0.0f},
                {EventType::kGesturePinchUpdate, 0.5f},
                {EventType::kGesturePinchUpdate, 1.5f},
                {EventType::kGesturePinchEnd, 0.0f},
                {EventType::kGesturePinchEnd, 0.0f},
            }),
            received);

  EXPECT_CALL(delegate, OnClosed()).Times(1);
}

// This test case ensures window_manager properly provides X11Window instances
// as they are created/destroyed.
TEST_F(X11WindowOzoneTest, GetWindowFromAcceleratedWigets) {
  MockPlatformWindowDelegate delegate;
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::AcceleratedWidget widget_1;
  auto window_1 = CreatePlatformWindow(&delegate, bounds, &widget_1, nullptr);
  EXPECT_EQ(window_1.get(), window_manager()->GetWindow(widget_1));

  gfx::AcceleratedWidget widget_2;
  auto window_2 = CreatePlatformWindow(&delegate, bounds, &widget_2, nullptr);
  EXPECT_EQ(window_2.get(), window_manager()->GetWindow(widget_2));
  EXPECT_EQ(window_1.get(), window_manager()->GetWindow(widget_1));

  window_1->Close();
  window_1.reset();
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_1));
  EXPECT_EQ(window_2.get(), window_manager()->GetWindow(widget_2));

  window_2.reset();
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_1));
  EXPECT_EQ(nullptr, window_manager()->GetWindow(widget_2));
}

// This test case ensures that OnCursorUpdate is called once when a mouse
// location moved to the window, and |window_mouse_currently_on_| is properly
// reset when the window is deleted.
TEST_F(X11WindowOzoneTest, CursorUpdateEnterAndDelete) {
  gfx::Rect bounds_1(0, 0, 100, 100);
  MockPlatformWindowDelegate delegate_1;
  gfx::AcceleratedWidget widget_1;
  auto window_1 =
      CreatePlatformWindow(&delegate_1, bounds_1, &widget_1, nullptr);

  MockPlatformWindowDelegate delegate_2;
  gfx::AcceleratedWidget widget_2;
  gfx::Rect bounds_2(0, 100, 100, 100);
  auto window_2 =
      CreatePlatformWindow(&delegate_2, bounds_2, &widget_2, nullptr);

  EXPECT_CALL(delegate_1, OnCursorUpdate()).Times(1);
  window_manager()->MouseOnWindow(static_cast<X11Window*>(window_1.get()));
  // The mouse is already on window_1, and this should not call OnCursorUpdate.
  window_manager()->MouseOnWindow(static_cast<X11Window*>(window_1.get()));

  EXPECT_CALL(delegate_2, OnCursorUpdate()).Times(1);
  window_manager()->MouseOnWindow(static_cast<X11Window*>(window_2.get()));

  EXPECT_EQ(window_2.get(),
            window_manager()->window_mouse_currently_on_for_test());

  // Dispatch Event on window 1 while event is captured on window 2.
  ::testing::Mock::VerifyAndClearExpectations(&delegate_1);
  EXPECT_CALL(delegate_1, OnCursorUpdate()).Times(1);
  window_2->SetCapture();
  ScopedXI2Event xi_event;
  xi_event.InitGenericButtonEvent(kPointerDeviceId, EventType::kMousePressed,
                                  gfx::Point(0, 0), EF_NONE);
  DispatchXEvent(xi_event, widget_1);
  EXPECT_EQ(window_1.get(),
            window_manager()->window_mouse_currently_on_for_test());

  // Removing the window should reset the |window_mouse_currently_on_|.
  window_1.reset();
  EXPECT_FALSE(window_manager()->window_mouse_currently_on_for_test());
}

class FakeX11ExtensionDelegateForSize : public X11ExtensionDelegate {
 public:
  explicit FakeX11ExtensionDelegateForSize(const gfx::Rect& guessed_size_px)
      : guessed_size_px_(guessed_size_px) {}
  ~FakeX11ExtensionDelegateForSize() override = default;

  void OnLostMouseGrab() override {}
#if BUILDFLAG(USE_ATK)
  bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event,
                     bool transient) override {
    return false;
  }
#endif
  bool IsOverrideRedirect(const X11Extension& x11_extension) const override {
    return false;
  }
  gfx::Rect GetGuessedFullScreenSizeInPx() const override {
    return guessed_size_px_;
  }

 private:
  gfx::Rect guessed_size_px_;
};

// Verifies X11Window sets fullscreen bounds in pixels when going to fullscreen.
TEST_F(X11WindowOzoneTest, SetFullscreen) {
  constexpr gfx::Rect screen_bounds_in_px(640, 480, 1280, 720);
  test_screen_.SetScaleAndBoundsForPrimaryDisplay(2, screen_bounds_in_px);

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  FakeX11ExtensionDelegateForSize x11_extension_delegate(screen_bounds_in_px);
  auto window =
      CreatePlatformWindow(&delegate, bounds, &widget, &x11_extension_delegate);
  EXPECT_CALL(
      delegate,
      OnBoundsChanged(testing::Eq(PlatformWindowDelegate::BoundsChange{true})));

  window->SetFullscreen(true, display::kInvalidDisplayId);
}


TEST_F(X11WindowOzoneTest, StartupIdProperty) {
  // Set a startup ID.
  const std::string kStartupId = "test-startup-id-123";
  base::nix::SetActivationToken(kStartupId);

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  // CreatePlatformWindow calls Initialize, which should consume the token
  // and set the _NET_STARTUP_ID property.
  auto window = CreatePlatformWindow(&delegate, gfx::Rect(0, 0, 100, 100),
                                     &widget, nullptr);

  // Verify that the token was consumed from global state.
  EXPECT_FALSE(base::nix::TakeXdgActivationToken().has_value());

  x11::Window xid = static_cast<x11::Window>(widget);

  // Read the _NET_STARTUP_ID property from the window.
  auto* connection = x11::Connection::Get();
  scoped_refptr<base::RefCountedMemory> data;
  x11::Atom type;
  ASSERT_TRUE(GetRawBytesOfProperty(xid, connection->GetAtom("_NET_STARTUP_ID"),
                                    &data, &type));

  EXPECT_EQ(type, connection->GetAtom("UTF8_STRING"));
  std::string value(reinterpret_cast<const char*>(data->data()), data->size());
  EXPECT_EQ(value, kStartupId);
}

TEST_F(X11WindowOzoneTest, StartupIdPropertyFromInitProperties) {
  const std::string kStartupId = "test-startup-id-properties";
  const std::string kGlobalToken = "global-token-should-not-be-used";
  base::nix::SetActivationToken(kGlobalToken);

  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(StoreWidget(&widget));

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 100, 100);
  properties.startup_id = kStartupId;

  auto window = std::make_unique<X11Window>(&delegate);
  window->Initialize(std::move(properties));

  // Verify that the global token was NOT consumed.
  auto global_token = base::nix::TakeXdgActivationToken();
  ASSERT_TRUE(global_token.has_value());
  EXPECT_EQ(global_token.value(), kGlobalToken);

  x11::Window xid = static_cast<x11::Window>(widget);

  // Read the _NET_STARTUP_ID property from the window.
  auto* connection = x11::Connection::Get();
  scoped_refptr<base::RefCountedMemory> data;
  x11::Atom type;
  ASSERT_TRUE(GetRawBytesOfProperty(xid, connection->GetAtom("_NET_STARTUP_ID"),
                                    &data, &type));

  EXPECT_EQ(type, connection->GetAtom("UTF8_STRING"));
  std::string value(reinterpret_cast<const char*>(data->data()), data->size());
  EXPECT_EQ(value, kStartupId);
}

TEST_F(X11WindowOzoneTest, NotifyStartupCompletePreservesGeometryCache) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect kInitialBounds(10, 20, 300, 400);
  auto window =
      CreatePlatformWindow(&delegate, kInitialBounds, &widget, nullptr);

  window->NotifyStartupComplete("test-startup-id");

  constexpr gfx::Rect kNewBounds(50, 60, 500, 600);
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(testing::AtLeast(1));
  window->SetBoundsInPixels(kNewBounds);
  x11::Connection::Get()->Sync();
  x11::Connection::Get()->DispatchAll();
  EXPECT_EQ(window->GetBoundsInPixels(), kNewBounds);
}

// Verifies that _GTK_FRAME_EXTENTS is not set when the window manager can't
// handle it (https://crbug.com/396808830).
TEST_F(X11WindowOzoneTest, NoFrameExtentsWhenDecorationInsetsUnsupported) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  ON_CALL(delegate, CalculateInsetsInDIP(_))
      .WillByDefault(testing::Return(gfx::Insets(4)));
  gfx::AcceleratedWidget widget;
  auto window = CreatePlatformWindow(&delegate, gfx::Rect(10, 20, 300, 400),
                                     &widget, nullptr);
  if (window->CanSetDecorationInsets()) {
    GTEST_SKIP() << "The window manager supports _GTK_FRAME_EXTENTS.";
  }

  window->Show(/*inactive=*/false);
  x11::Connection::Get()->Sync();

  scoped_refptr<base::RefCountedMemory> data;
  x11::Atom type;
  EXPECT_FALSE(GetRawBytesOfProperty(
      static_cast<x11::Window>(widget),
      x11::Connection::Get()->GetAtom("_GTK_FRAME_EXTENTS"), &data, &type));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(X11WindowOzoneTest, ActivateSetsRequestorCurrentlyActiveWindow) {
  auto* connection = x11::Connection::Get();
  ASSERT_TRUE(
      connection->WmSupportsHint(connection->GetAtom("_NET_ACTIVE_WINDOW")));
  ASSERT_NE(GuessWindowManager(), WM_WMII);

  x11::Connection observer;
  ASSERT_TRUE(observer.Ready());
  auto root_events = observer.ScopedSelectEvent(
      observer.default_root(), x11::EventMask::SubstructureNotify);
  observer.Sync();

  auto take_active_window_message =
      [&](x11::Window target_window) -> std::optional<x11::ClientMessageEvent> {
    connection->Sync();
    observer.Sync();
    observer.ReadResponses();
    const x11::Atom net_active_window = observer.GetAtom("_NET_ACTIVE_WINDOW");
    std::optional<x11::ClientMessageEvent> found;
    for (const auto& ev : observer.events()) {
      if (const auto* msg = ev.As<x11::ClientMessageEvent>();
          msg && msg->type == net_active_window &&
          msg->window == target_window) {
        found = *msg;
      }
    }
    observer.events().clear();
    return found;
  };

  auto dispatch_focus = [&](x11::Window target_window, bool focus_in) {
    x11::FocusEvent focus_event{
        .opcode = focus_in ? x11::FocusEvent::In : x11::FocusEvent::Out,
        .detail = x11::NotifyDetail::Nonlinear,
        .event = target_window,
        .mode = x11::NotifyMode::Normal,
    };
    connection->DispatchEvent(x11::Event(false, focus_event));
  };

  testing::NiceMock<MockPlatformWindowDelegate> delegate_1;
  gfx::AcceleratedWidget widget_1;
  auto window_1 = CreatePlatformWindow(&delegate_1, gfx::Rect(0, 0, 100, 100),
                                       &widget_1, nullptr);
  const auto xwindow_1 = static_cast<x11::Window>(widget_1);

  testing::NiceMock<MockPlatformWindowDelegate> delegate_2;
  gfx::AcceleratedWidget widget_2;
  auto window_2 = CreatePlatformWindow(&delegate_2, gfx::Rect(200, 0, 100, 100),
                                       &widget_2, nullptr);
  const auto xwindow_2 = static_cast<x11::Window>(widget_2);

  window_1->Show(/*inactive=*/true);
  window_2->Show(/*inactive=*/true);
  connection->DispatchAll();
  dispatch_focus(xwindow_1, false);
  dispatch_focus(xwindow_2, false);
  observer.events().clear();

  // With no active window, data[2] must be 0 (x11::Window::None).
  window_2->Activate();
  auto msg = take_active_window_message(xwindow_2);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->data.data32[0], 1u);
  EXPECT_EQ(msg->data.data32[2], 0u);

  // With `window_1` active, activating `window_2` must pass `window_1` in
  // data[2].
  connection->DispatchAll();
  dispatch_focus(xwindow_2, false);
  dispatch_focus(xwindow_1, true);
  ASSERT_TRUE(static_cast<X11Window*>(window_1.get())->IsActive());
  ASSERT_FALSE(static_cast<X11Window*>(window_2.get())->IsActive());
  observer.events().clear();

  window_2->Activate();
  msg = take_active_window_message(xwindow_2);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->data.data32[0], 1u);
  EXPECT_EQ(msg->data.data32[2], static_cast<uint32_t>(xwindow_1));

  // Deactivating `window_1` before re-activating it must not report `window_1`
  // as already active even while `has_window_focus_` has not yet cleared.
  connection->DispatchAll();
  dispatch_focus(xwindow_2, false);
  dispatch_focus(xwindow_1, true);
  window_1->Deactivate();
  ASSERT_FALSE(static_cast<X11Window*>(window_1.get())->IsActive());
  observer.events().clear();

  window_1->Activate();
  msg = take_active_window_message(xwindow_1);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->data.data32[0], 1u);
  EXPECT_EQ(msg->data.data32[2], 0u);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(X11WindowOzoneTest, ShowMapsWindowWithoutExplicitSync) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  gfx::AcceleratedWidget widget;
  auto window = CreatePlatformWindow(&delegate, gfx::Rect(0, 0, 100, 100),
                                     &widget, nullptr);
  auto* x11_window = static_cast<X11Window*>(window.get());
  auto* connection = x11::Connection::Get();
  connection->Sync();
  connection->DispatchAll();
  connection->Sync();
  connection->DispatchAll();

  window->Show(/*inactive=*/true);
  EXPECT_TRUE(x11_window->IsMapPending());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !x11_window->IsMapPending(); }));
}

// Verifies that once a window with a window manager frame is mapped, moves
// requested by other clients (e.g. `xdotool windowmove`) position the frame,
// while moves requested by Chrome position the client window
// (https://crbug.com/338741570).
TEST_F(X11WindowOzoneTest, MoveWithWindowManagerFrame) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  gfx::AcceleratedWidget widget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(StoreWidget(&widget));
  X11Window window(&delegate);
  window.Initialize(
      PlatformWindowInitProperties(gfx::Rect(100, 100, 300, 200)));
  window.SetUseNativeFrame(true);
  window.Show(/*inactive=*/false);
  ASSERT_TRUE(base::test::RunUntil([&] { return !window.IsMapPending(); }));
  auto* connection = x11::Connection::Get();
  connection->Sync();
  connection->DispatchAll();
  const gfx::Rect bounds = window.GetBoundsInPixels();
  const gfx::Vector2d frame_offset =
      bounds.origin() - window.GetOuterBounds().origin();
  if (frame_offset.IsZero()) {
    GTEST_SKIP() << "The window manager does not provide a frame.";
  }
  // StaticGravity places the client window, not the frame, at the requested
  // position. The window manager may ignore the position (Openbox does while
  // windows of parallel test processes are there), so don't expect it.
  EXPECT_NE(window.GetOuterBounds().origin(), gfx::Point(100, 100));

  // Move the window like another X client would.
  connection->ConfigureWindow(
      {.window = static_cast<x11::Window>(widget), .x = 0, .y = 0});
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return window.GetBoundsInPixels().origin() != bounds.origin(); }));
  EXPECT_EQ(window.GetBoundsInPixels().origin(), gfx::Point() + frame_offset);

  constexpr gfx::Point kOrigin(150, 120);
  window.SetBoundsInPixels(gfx::Rect(kOrigin, bounds.size()));
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !window.IsWmSyncActiveForTest(); }));
  EXPECT_EQ(window.GetBoundsInPixels().origin(), kOrigin);
}

namespace {

constexpr int kTouchDeviceId = 0;

// A touch event received by a MockPlatformWindowDelegate.
struct ReceivedTouch {
  EventType type;
  PointerId id;

  bool operator==(const ReceivedTouch&) const = default;

  friend void PrintTo(const ReceivedTouch& touch, std::ostream* os) {
    *os << "{type=" << static_cast<int>(touch.type) << ", id=" << touch.id
        << "}";
  }
};

// Dispatches an XI2 touch event of `evtype` for `tracking_id`, reported
// relative to `event_window`.
void DispatchTouch(int evtype, int tracking_id, x11::Window event_window) {
  ScopedXI2Event xi_event;
  xi_event.InitTouchEvent(kTouchDeviceId, evtype, tracking_id,
                          gfx::Point(100, 100), {});
  x11::Event* event = xi_event;
  event->As<x11::Input::DeviceEvent>()->event = event_window;
  x11::Connection::Get()->DispatchEvent(*event);
}

// Stores the button of a _NET_WM_MOVERESIZE message for `window`.
class MoveResizeObserver : public x11::EventObserver {
 public:
  MoveResizeObserver(x11::Connection& connection,
                     x11::Window window,
                     std::optional<uint32_t>* button)
      : connection_(connection), window_(window), button_(button) {
    connection_->AddEventObserver(this);
  }
  ~MoveResizeObserver() override { connection_->RemoveEventObserver(this); }

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override {
    auto* message = xev.As<x11::ClientMessageEvent>();
    if (message && message->window == window_ &&
        message->type == connection_->GetAtom("_NET_WM_MOVERESIZE")) {
      *button_ = message->data.data32[3];
    }
  }

 private:
  const raw_ref<x11::Connection> connection_;
  const x11::Window window_;
  const raw_ptr<std::optional<uint32_t>> button_;
};

}  // namespace

class X11WindowOzoneTouchTest : public X11WindowOzoneTest {
 public:
  void SetUp() override {
    X11WindowOzoneTest::SetUp();
    // Start each test with fresh touch id assignments.
    TouchFactory::GetInstance()->ResetForTest();
    SetUpTouchDevicesForTest({kTouchDeviceId});

    EXPECT_CALL(delegate_, OnClosed()).Times(1);
    window_ = CreatePlatformWindow(&delegate_, gfx::Rect(30, 80, 800, 600),
                                   &widget_, nullptr);
    ON_CALL(delegate_, DispatchEvent(_)).WillByDefault([this](Event* event) {
      if (event->IsTouchEvent()) {
        received_touches_.push_back(
            {event->type(), event->AsTouchEvent()->pointer_details().id});
      }
    });
  }

  void TearDown() override {
    window_.reset();
    X11WindowOzoneTest::TearDown();
  }

 protected:
  x11::Window xwindow() const { return static_cast<x11::Window>(widget_); }

  // Runs the window's move loop. `during_loop` runs once the loop has started
  // and must end it.
  void RunMoveLoop(base::OnceClosure during_loop) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(during_loop));
    GetWmMoveLoopHandler(*window_)->RunMoveLoop(gfx::Vector2d());
  }

  void EndMoveLoop() { GetWmMoveLoopHandler(*window_)->EndMoveLoop(); }

  testing::NiceMock<MockPlatformWindowDelegate> delegate_;
  gfx::AcceleratedWidget widget_;
  std::unique_ptr<PlatformWindow> window_;
  std::vector<ReceivedTouch> received_touches_;
};

// A move loop that grabs the pointer itself ends every active touch sequence
// when it releases the grab, and the X server reports their end relative to
// the loop's input window. The windows must still see those touches end, even
// if the loop ends while they are down (e.g. on Escape) and even if they did
// not start the move. (The grab itself is not taken here, since tests may share
// the X server with other test processes.)
TEST_F(X11WindowOzoneTouchTest, CancelsAllActiveTouchesAfterGrabbedMoveLoop) {
  DispatchTouch(x11::Input::DeviceEvent::TouchBegin, 10, xwindow());
  DispatchTouch(x11::Input::DeviceEvent::TouchBegin, 11, xwindow());
  ASSERT_EQ((std::vector<ReceivedTouch>{{EventType::kTouchPressed, 0},
                                        {EventType::kTouchPressed, 1}}),
            received_touches_);
  received_touches_.clear();

  X11Window::CancelTouchesEndedByMoveLoop(/*consumed_touches=*/{},
                                          /*grabbed_pointer=*/true);

  EXPECT_EQ((std::vector<ReceivedTouch>{{EventType::kTouchCancelled, 0},
                                        {EventType::kTouchCancelled, 1}}),
            received_touches_);

  // The touches are no longer active, so they are not cancelled again.
  received_touches_.clear();
  X11Window::CancelTouchesEndedByMoveLoop(/*consumed_touches=*/{},
                                          /*grabbed_pointer=*/true);
  EXPECT_TRUE(received_touches_.empty());
}

// A touch release consumed by the move loop must end the touch sequence on the
// window it was dispatched to, whichever window the X server reported the
// release relative to (e.g. a window that is not an X11Window).
TEST_F(X11WindowOzoneTouchTest, MoveLoopCancelsConsumedTouchRelease) {
  // With capture, the loop does not grab the pointer.
  window_->SetCapture();
  DispatchTouch(x11::Input::DeviceEvent::TouchBegin, 10, xwindow());
  ASSERT_EQ((std::vector<ReceivedTouch>{{EventType::kTouchPressed, 0}}),
            received_touches_);
  received_touches_.clear();

  const auto other_window = x11::Connection::Get()->GenerateId<x11::Window>();
  RunMoveLoop(base::BindLambdaForTesting([other_window]() {
    // The release ends the loop.
    DispatchTouch(x11::Input::DeviceEvent::TouchEnd, 10, other_window);
  }));

  EXPECT_EQ((std::vector<ReceivedTouch>{{EventType::kTouchCancelled, 0}}),
            received_touches_);
  window_->ReleaseCapture();
}

// A move loop that did not grab the pointer must leave touch sequences that
// are still down alone, e.g. so that a tab drag can re-attach to a tab strip
// with the finger still down.
TEST_F(X11WindowOzoneTouchTest, MoveLoopWithoutPointerGrabKeepsActiveTouches) {
  window_->SetCapture();
  DispatchTouch(x11::Input::DeviceEvent::TouchBegin, 10, xwindow());
  received_touches_.clear();

  RunMoveLoop(base::BindLambdaForTesting([this]() { EndMoveLoop(); }));

  EXPECT_TRUE(received_touches_.empty());
  window_->ReleaseCapture();
}

// The window manager follows the touch only through pointer emulation, so a
// move started by a touch must report the emulated button 1 as held.
// https://crbug.com/40928551
TEST_F(X11WindowOzoneTouchTest, TouchStartsWindowManagerMoveWithButton) {
  // Receive _NET_WM_MOVERESIZE as the window manager would.
  x11::Connection wm_connection;
  wm_connection.ChangeWindowAttributes(x11::ChangeWindowAttributesRequest{
      .window = wm_connection.default_root(),
      .event_mask = x11::EventMask::SubstructureNotify,
  });
  wm_connection.Sync();

  ON_CALL(delegate_, DispatchEvent(_)).WillByDefault([this](Event* event) {
    if (event->type() == EventType::kTouchPressed) {
      GetWmMoveResizeHandler(*window_)->DispatchHostWindowDragMovement(
          HTCAPTION, gfx::Point(100, 100));
    }
  });
  DispatchTouch(x11::Input::DeviceEvent::TouchBegin, 10, xwindow());
  x11::Connection::Get()->Sync();

  std::optional<uint32_t> button;
  MoveResizeObserver observer(wm_connection, xwindow(), &button);
  wm_connection.Sync();
  wm_connection.DispatchAll();
  EXPECT_EQ(1u, button);
}

// Setting and releasing capture on a window while a drag or move loop holds the
// pointer grab must leave the loop's grab in place.
TEST_F(X11WindowOzoneTest, CaptureKeepsMoveLoopPointerGrab) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  gfx::AcceleratedWidget widget;
  EXPECT_CALL(delegate, OnClosed()).Times(1);
  auto window = CreatePlatformWindow(&delegate, gfx::Rect(0, 0, 100, 100),
                                     &widget, nullptr);

  // Stand in for the input window of a move loop.
  auto* connection = x11::Connection::Get();
  const auto loop_window = connection->GenerateId<x11::Window>();
  connection->CreateWindow(x11::CreateWindowRequest{
      .wid = loop_window,
      .parent = connection->default_root(),
      .x = -100,
      .y = -100,
      .width = 10,
      .height = 10,
      .c_class = x11::WindowClass::InputOnly,
      .override_redirect = x11::Bool32(true),
  });
  connection->MapWindow(loop_window);
  connection->Sync();
  if (GrabPointer(loop_window, false, nullptr) != x11::GrabStatus::Success) {
    connection->DestroyWindow({loop_window});
    GTEST_SKIP() << "Another client holds the pointer grab.";
  }

  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(loop_window, GetActivePointerGrabWindow());

  window->ReleaseCapture();
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(loop_window, GetActivePointerGrabWindow());

  // The X server must still report the grab: another client can't grab the
  // pointer.
  connection->Sync();
  x11::Connection other_connection;
  const auto other_window = other_connection.GenerateId<x11::Window>();
  other_connection.CreateWindow(x11::CreateWindowRequest{
      .wid = other_window,
      .parent = other_connection.default_root(),
      .width = 10,
      .height = 10,
      .c_class = x11::WindowClass::InputOnly,
      .override_redirect = x11::Bool32(true),
  });
  other_connection.MapWindow(other_window);
  auto reply = other_connection
                   .GrabPointer(x11::GrabPointerRequest{
                       .grab_window = other_window,
                       .pointer_mode = x11::GrabMode::Async,
                       .keyboard_mode = x11::GrabMode::Async,
                       .time = x11::Time::CurrentTime,
                   })
                   .Sync();
  ASSERT_TRUE(reply);
  EXPECT_EQ(x11::GrabStatus::AlreadyGrabbed, reply->status);
  other_connection.UngrabPointer();
  other_connection.Sync();

  UngrabPointer();
  connection->DestroyWindow({loop_window});
}

}  // namespace ui
