// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/x/event.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

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

// Verifies that SetBoundsInPixels() is safe against the X11Window being
// synchronously destroyed during the GetBoundsInPixels() call. This can happen
// if GeometryCache::GetBoundsPx() processes synchronous X server replies and
// dispatches an OnBoundsChanged event that closes the widget (e.g., as in
// crbug.com/1068755).
TEST_F(X11WindowOzoneTest, SetBoundsInPixelsUseAfterFreeViaGeometryCache) {
  testing::NiceMock<MockPlatformWindowDelegate> delegate;
  gfx::AcceleratedWidget widget;
  constexpr gfx::Rect bounds(30, 80, 800, 600);
  std::unique_ptr<PlatformWindow> window =
      CreatePlatformWindow(&delegate, bounds, &widget, nullptr);

  auto* connection = x11::Connection::Get();
  auto xwindow = static_cast<x11::Window>(widget);

  // Step 1: Force the X11Window's GeometryCache (and its parent chain) to
  // become Ready and record last_notified_geometry_. This goes through the
  // synchronous DispatchNow() path. The resulting X11Window::OnBoundsChanged
  // sees a size change and only posts a delayed-resize task, so the delegate
  // is not called synchronously here.
  window->GetBoundsInPixels();

  // Step 2: Create a real parent window at a non-zero offset so that after
  // reparenting, only the absolute origin of |xwindow| changes (size stays
  // 800x600). override_redirect avoids any WM interference.
  x11::Window new_parent = connection->GenerateId<x11::Window>();
  connection->CreateWindow({
      .wid = new_parent,
      .parent = connection->default_root(),
      .x = 200,
      .y = 200,
      .width = 1000,
      .height = 1000,
      .c_class = x11::WindowClass::InputOnly,
      .override_redirect = x11::Bool32(true),
  });

  // Step 3: Synthesize the ReparentNotify the X server would send for a WM
  // reparent. GeometryCache::OnEvent replaces parent_ with a fresh un-Ready
  // GeometryCache for |new_parent|, leaving the leaf cache's chain not Ready.
  x11::ReparentNotifyEvent reparent{};
  reparent.event = xwindow;
  reparent.window = xwindow;
  reparent.parent = new_parent;
  reparent.x = 0;
  reparent.y = 0;
  x11::Event reparent_event(/*send_event=*/false, std::move(reparent));
  connection->DispatchEvent(reparent_event);

  // Step 4: Arm the delegate so that the *next* synchronous OnBoundsChanged
  // (which will fire from inside SetBoundsInPixels → GetBoundsInPixels →
  // GetBoundsPx → DispatchNow → OnBoundsChanged → NotifyBoundsChanged) frees
  // the X11Window — modelling Widget::CloseNow → SetPlatformWindow(nullptr).
  bool armed = true;
  bool freed = false;
  EXPECT_CALL(delegate, OnBoundsChanged(_))
      .WillRepeatedly([&](const PlatformWindowDelegate::BoundsChange&) {
        if (armed) {
          armed = false;
          freed = true;
          // ~X11Window → PrepareForShutdown → Close → CloseXWindow →
          // geometry_cache_.reset(). All GeometryCache weak_ptrs are
          // invalidated, so every GetBoundsPx() frame on the stack will
          // return {}, and SetBoundsInPixels() should guard against this
          // deletion.
          window.reset();
        }
      });

  // Step 5: Call SetBoundsInPixels(). Inside, GetBoundsInPixels() recurses
  // into the un-Ready parent cache, which processes replies synchronously via
  // DispatchNow(). The resulting OnBoundsChanged event triggers the observer,
  // which destroys the window. X11Window must safely return early instead
  // of accessing freed members or the destroyed delegate.
  PlatformWindow* raw = window.get();
  raw->SetBoundsInPixels(gfx::Rect(40, 90, 800, 600));

  // If we got here without ASAN tripping, the synchronous-free path was not
  // exercised; surface that as a test failure rather than a silent pass.
  EXPECT_TRUE(freed) << "delegate was never invoked synchronously";

  connection->DestroyWindow({new_parent});
}

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

}  // namespace ui
