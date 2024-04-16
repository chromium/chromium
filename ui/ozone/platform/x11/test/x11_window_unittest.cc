// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window.h"
#include "base/memory/raw_ptr.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/x/test/x11_property_change_waiter.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/extensions/x11_extension_delegate.h"

namespace ui {

namespace {

constexpr int kPointerDeviceId = 1;

using BoundsChange = PlatformWindowDelegate::BoundsChange;

class TestPlatformWindowDelegate : public PlatformWindowDelegate {
 public:
  TestPlatformWindowDelegate() = default;
  TestPlatformWindowDelegate(const TestPlatformWindowDelegate&) = delete;
  TestPlatformWindowDelegate& operator=(const TestPlatformWindowDelegate&) =
      delete;
  ~TestPlatformWindowDelegate() override = default;

  gfx::AcceleratedWidget widget() const { return widget_; }
  PlatformWindowState state() const { return state_; }

  void WaitForBoundsChange(
      const PlatformWindowDelegate::BoundsChange& expected_change) {
    if (expected_change_ == expected_change)
      return;
    expected_change_ = expected_change;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // PlatformWindowDelegate:
  void OnBoundsChanged(
      const PlatformWindowDelegate::BoundsChange& change) override {
    changed_ = change;
    size_px_ = window_->GetBoundsInPixels().size();
    if (!quit_closure_.is_null() && changed_ == expected_change_)
      std::move(quit_closure_).Run();
  }
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {
    state_ = new_state;
  }
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {
    widget_ = gfx::kNullAcceleratedWidget;
  }
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}
  SkPath GetWindowMaskForWindowShapeInPixels() override {
    SkPath window_mask;
    int right = size_px_.width();
    int bottom = size_px_.height();

    window_mask.moveTo(0, 0);
    window_mask.lineTo(0, bottom);
    window_mask.lineTo(right, bottom);
    window_mask.lineTo(right, 10);
    window_mask.lineTo(right - 10, 10);
    window_mask.lineTo(right - 10, 0);
    window_mask.close();
    return window_mask;
  }

  void set_window(X11Window* window) { window_ = window; }

 private:
  raw_ptr<X11Window> window_ = nullptr;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  PlatformWindowState state_ = PlatformWindowState::kUnknown;
  PlatformWindowDelegate::BoundsChange changed_{false};
  gfx::Size size_px_;
  PlatformWindowDelegate::BoundsChange expected_change_{false};

  // Ends the run loop.
  base::OnceClosure quit_closure_;
};

class ShapedX11ExtensionDelegate : public X11ExtensionDelegate {
 public:
  ShapedX11ExtensionDelegate() = default;
  ~ShapedX11ExtensionDelegate() override = default;

  void set_guessed_bounds(const gfx::Rect& guessed_bounds_px) {
    guessed_bounds_px_ = guessed_bounds_px;
  }

  void OnLostMouseGrab() override {}
#if BUILDFLAG(USE_ATK)
  bool OnAtkKeyEvent(AtkKeyEventStruct* atk_key_event,
                     bool transient) override {
    return false;
  }
#endif
  bool IsOverrideRedirect() const override { return false; }
  gfx::Rect GetGuessedFullScreenSizeInPx() const override {
    return guessed_bounds_px_;
  }

 private:
  gfx::Rect guessed_bounds_px_;
};

// Blocks till the window state hint, |hint|, is set or unset.
class WMStateWaiter : public X11PropertyChangeWaiter {
 public:
  WMStateWaiter(x11::Window window, const char* hint, bool wait_till_set)
      : X11PropertyChangeWaiter(window, "_NET_WM_STATE"),
        hint_(hint),
        wait_till_set_(wait_till_set) {}
  WMStateWaiter(const WMStateWaiter&) = delete;
  WMStateWaiter& operator=(const WMStateWaiter&) = delete;
  ~WMStateWaiter() override = default;

 private:
  // X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting() override {
    std::vector<x11::Atom> hints;
    if (x11::Connection::Get()->GetArrayProperty(
            xwindow(), x11::GetAtom("_NET_WM_STATE"), &hints)) {
      return base::Contains(hints, x11::GetAtom(hint_)) != wait_till_set_;
    }
    return true;
  }

  // The name of the hint to wait to get set or unset.
  const char* hint_;

  // Whether we are waiting for |hint| to be set or unset.
  bool wait_till_set_;
};

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

// Returns the list of rectangles which describe |window|'s bounding region via
// the X shape extension.
std::vector<gfx::Rect> GetShapeRects(x11::Window window) {
  std::vector<gfx::Rect> shape_vector;
  if (auto shape = x11::Connection::Get()
                       ->shape()
                       .GetRectangles({window, x11::Shape::Sk::Bounding})
                       .Sync()) {
    for (const auto& rect : shape->rectangles)
      shape_vector.emplace_back(rect.x, rect.y, rect.width, rect.height);
  }
  return shape_vector;
}

// Returns true if one of |rects| contains point (x,y).
bool ShapeRectContainsPoint(const std::vector<gfx::Rect>& shape_rects,
                            int x,
                            int y) {
  gfx::Point point(x, y);
  return base::ranges::any_of(
      shape_rects, [&point](const auto& rect) { return rect.Contains(point); });
}

}  // namespace

class X11WindowTest : public testing::Test {
 public:
  X11WindowTest()
      : task_env_(std::make_unique<base::test::TaskEnvironment>(
            base::test::TaskEnvironment::MainThreadType::UI)) {}
  X11WindowTest(const X11WindowTest&) = delete;
  X11WindowTest& operator=(const X11WindowTest&) = delete;
  ~X11WindowTest() override = default;

  void SetUp() override {
    auto* connection = x11::Connection::Get();
    event_source_ = std::make_unique<X11EventSource>(connection);

    std::vector<int> pointer_devices;
    pointer_devices.push_back(kPointerDeviceId);
    ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(pointer_devices);

    // X11 requires display::Screen instance.
    test_screen_.emplace();
    display::Screen::SetScreenInstance(&test_screen_.value());

    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    connection->SynchronizeForTest(true);
  }

 protected:
  void TearDown() override {
    x11::Connection::Get()->SynchronizeForTest(false);
    display::Screen::SetScreenInstance(nullptr);
    test_screen_.reset();
  }

  std::unique_ptr<X11Window> CreateX11Window(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds,
      X11ExtensionDelegate* x11_extension_delegate) {
    PlatformWindowInitProperties init_params(bounds);
    init_params.x11_extension_delegate = x11_extension_delegate;
    auto window = std::make_unique<X11Window>(delegate);
    window->Initialize(std::move(init_params));
    // Remove native frame so that X server doesn't change our bounds.
    window->SetUseNativeFrame(false);
    return window;
  }

  void DispatchSingleEventToWidget(x11::Event* x11_event, x11::Window window) {
    auto* device_event = x11_event->As<x11::Input::DeviceEvent>();
    DCHECK(device_event);
    device_event->event = window;
    x11::Connection::Get()->DispatchEvent(*x11_event);
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_env_;
  std::unique_ptr<X11EventSource> event_source_;

  std::optional<TestScreen> test_screen_;
};

// https://crbug.com/898742: Test is flaky.
TEST_F(X11WindowTest, DISABLED_Shape) {
  if (!IsShapeExtensionAvailable())
    return;

  // 1) Test setting the window shape via the ShapedX11ExtensionDelegate. This
  // technique is used to get rounded corners on Chrome windows when not using
  // the native window frame.
  TestPlatformWindowDelegate delegate;
  ShapedX11ExtensionDelegate x11_extension_delegate;
  constexpr gfx::Rect bounds(100, 100, 100, 100);
  auto window = CreateX11Window(&delegate, bounds, &x11_extension_delegate);
  delegate.set_window(window.get());
  window->Show(false);

  const x11::Window x11_window = window->window();
  ASSERT_TRUE(x11_window != x11::Window::None);

  // Force PlatformWindowDelegate::OnBoundsChanged.
  window->SetBoundsInPixels(bounds);
  // Force update the window region.
  window->ResetWindowRegion();

  auto* connection = x11::Connection::Get();
  connection->DispatchAll();

  std::vector<gfx::Rect> shape_rects = GetShapeRects(x11_window);
  ASSERT_FALSE(shape_rects.empty());

  // The widget was supposed to be 100x100, but the WM might have ignored this
  // suggestion.
  int window_width = window->GetBoundsInPixels().width();
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, window_width - 15, 5));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, window_width - 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, window_width - 5, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, window_width + 5, 15));

  // Changing window's size should update the shape.
  window->SetBoundsInPixels(gfx::Rect(100, 100, 200, 200));
  // Force update the window region.
  window->ResetWindowRegion();
  connection->DispatchAll();

  if (window->GetBoundsInPixels().width() == 200) {
    shape_rects = GetShapeRects(x11_window);
    ASSERT_FALSE(shape_rects.empty());
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 85, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 185, 5));
    EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 195, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 195, 15));
    EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 205, 15));
  }

  if (connection->WmSupportsHint(
          x11::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"))) {
    // The shape should be changed to a rectangle which fills the entire screen
    // when |widget1| is maximized.
    {
      WMStateWaiter waiter(x11_window, "_NET_WM_STATE_MAXIMIZED_VERT", true);
      window->Maximize();
      waiter.Wait();
    }

    // Ensure that the task which is posted when a window is resized is run.
    base::RunLoop().RunUntilIdle();

    // xvfb does not support Xrandr so we cannot check the maximized window's
    // bounds.
    auto geometry = connection->GetGeometry(x11_window).Sync();
    auto maximized_width = geometry ? geometry->width : 0;

    shape_rects = GetShapeRects(x11_window);
    ASSERT_FALSE(shape_rects.empty());
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, maximized_width - 1, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, maximized_width - 1, 15));
  }

  // 2) Test setting the window shape via PlatformWindow::SetShape().
  auto shape_region = std::make_unique<std::vector<gfx::Rect>>();
  shape_region->emplace_back(10, 0, 90, 10);
  shape_region->emplace_back(0, 10, 10, 90);
  shape_region->emplace_back(10, 10, 90, 90);

  TestPlatformWindowDelegate delegate2;
  constexpr gfx::Rect bounds2(30, 80, 800, 600);
  auto window2 = CreateX11Window(&delegate2, bounds2, nullptr);
  window2->Show(false);

  const x11::Window x11_window2 = window2->window();
  ASSERT_TRUE(x11_window2 != x11::Window::None);

  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  window2->SetShape(std::move(shape_region), transform);

  connection->DispatchAll();

  shape_rects = GetShapeRects(x11_window2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Changing the windows's size should not affect the shape.
  window2->SetBoundsInPixels(gfx::Rect(100, 100, 200, 200));
  shape_rects = GetShapeRects(x11_window2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Setting the shape to nullptr resets the shape back to the entire
  // window bounds.
  window2->SetShape(nullptr, transform);
  shape_rects = GetShapeRects(x11_window2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 105, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 500, 500));
}

// Flaky on Linux ASAN and ChromeOS. https://crbug.com/1291868
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_WindowManagerTogglesFullscreen \
  DISABLED_WindowManagerTogglesFullscreen
#else
#define MAYBE_WindowManagerTogglesFullscreen WindowManagerTogglesFullscreen
#endif
// Test that the widget reacts on changes in fullscreen state initiated by the
// window manager (e.g. via a window manager accelerator key).
TEST_F(X11WindowTest, MAYBE_WindowManagerTogglesFullscreen) {
  auto* connection = x11::Connection::Get();
  if (!connection->WmSupportsHint(x11::GetAtom("_NET_WM_STATE_FULLSCREEN"))) {
    return;
  }

  TestPlatformWindowDelegate delegate;
  ShapedX11ExtensionDelegate x11_extension_delegate;
  constexpr gfx::Rect bounds(100, 100, 100, 100);
  x11_extension_delegate.set_guessed_bounds(bounds);
  auto window = CreateX11Window(&delegate, bounds, &x11_extension_delegate);
  delegate.set_window(window.get());
  x11::Window x11_window = window->window();
  window->Show(false);

  connection->DispatchAll();

  EXPECT_NE(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);

  gfx::Rect initial_bounds = window->GetBoundsInPixels();
  {
    WMStateWaiter waiter(x11_window, "_NET_WM_STATE_FULLSCREEN", true);
    window->SetFullscreen(true, display::kInvalidDisplayId);
    waiter.Wait();
  }
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);

  // Emulate the window manager exiting fullscreen via a window manager
  // accelerator key.
  {
    ui::SendClientMessage(
        x11_window, ui::GetX11RootWindow(), x11::GetAtom("_NET_WM_STATE"),
        {0, static_cast<uint32_t>(x11::GetAtom("_NET_WM_STATE_FULLSCREEN")), 0,
         1, 0});

    WMStateWaiter waiter(x11_window, "_NET_WM_STATE_FULLSCREEN", false);
    waiter.Wait();
  }

  // Ensure it continues in browser fullscreen mode and bounds are restored to
  // |initial_bounds|.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);
  delegate.WaitForBoundsChange({false});
  EXPECT_EQ(initial_bounds, window->GetBoundsInPixels());

  // Emulate window resize (through X11 configure events) while in browser
  // fullscreen mode and ensure bounds are tracked correctly.
  initial_bounds.set_size({400, 400});
  {
    connection->ConfigureWindow({
        .window = x11_window,
        .width = initial_bounds.width(),
        .height = initial_bounds.height(),
    });
    // Ensure that the task which is posted when a window is resized is run.
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);
  delegate.WaitForBoundsChange({false});
  EXPECT_EQ(initial_bounds, window->GetBoundsInPixels());

  // Calling Widget::SetFullscreen(false) should clear the widget's fullscreen
  // state and clean things up.
  window->SetFullscreen(false, display::kInvalidDisplayId);
  EXPECT_NE(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);
  delegate.WaitForBoundsChange({false});
  EXPECT_EQ(initial_bounds, window->GetBoundsInPixels());
}

// TODO(crbug.com/40820331): Flaky on both Linux and ChromeOS.
// Tests that the minimization information is propagated to the
// PlatformWindowDelegate.
TEST_F(X11WindowTest,
       DISABLED_ToggleMinimizePropogateToPlatformWindowDelegate) {
  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect bounds(10, 10, 100, 100);
  auto window = CreateX11Window(&delegate, bounds, nullptr);
  delegate.set_window(window.get());
  window->Show(false);
  window->Activate();

  x11::Connection::Get()->DispatchAll();

  x11::Window x11_window = window->window();

  // Minimize by iconifying.
  {
    EXPECT_FALSE(window->IsMinimized());

    SendClientMessage(x11_window, GetX11RootWindow(),
                      x11::GetAtom("WM_CHANGE_STATE"),
                      {x11::WM_STATE_ICONIC, 0, 0, 0, 0});
    // Wait till set.
    WMStateWaiter waiter(x11_window, "_NET_WM_STATE_HIDDEN", true);
    waiter.Wait();
  }
  EXPECT_TRUE(window->IsMinimized());
  EXPECT_EQ(delegate.state(), PlatformWindowState::kMinimized);

  // Show from minimized by sending WM_STATE_NORMAL.
  {
    SendClientMessage(x11_window, GetX11RootWindow(),
                      x11::GetAtom("WM_CHANGE_STATE"),
                      {x11::WM_STATE_NORMAL, 0, 0, 0, 0});
    // Wait till unset.
    WMStateWaiter waiter(x11_window, "_NET_WM_STATE_HIDDEN", false);
    waiter.Wait();
  }
  EXPECT_FALSE(window->IsMinimized());
  EXPECT_EQ(delegate.state(), PlatformWindowState::kNormal);
}

}  // namespace ui
