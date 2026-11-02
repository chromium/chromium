// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/test/configurable_test_custom_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

namespace views {

// This tests the wayland and linux(x11) implementation of the
// DesktopWindowTreeHostPlatform.

namespace {

class MouseEventRecorder : public ui::EventHandler {
 public:
  MouseEventRecorder() = default;

  MouseEventRecorder(const MouseEventRecorder&) = delete;
  MouseEventRecorder& operator=(const MouseEventRecorder&) = delete;

  ~MouseEventRecorder() override = default;

  void Reset() { mouse_events_.clear(); }

  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    mouse_events_.push_back(*mouse);
  }

  std::vector<ui::MouseEvent> mouse_events_;
};

#if BUILDFLAG(IS_LINUX)
class TouchEventTypeRecorder : public ui::EventHandler {
 public:
  TouchEventTypeRecorder() = default;
  TouchEventTypeRecorder(const TouchEventTypeRecorder&) = delete;
  TouchEventTypeRecorder& operator=(const TouchEventTypeRecorder&) = delete;
  ~TouchEventTypeRecorder() override = default;

  void Reset() { event_types_.clear(); }

  const std::vector<ui::EventType>& event_types() const { return event_types_; }

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* touch) override {
    event_types_.push_back(touch->type());
  }

  std::vector<ui::EventType> event_types_;
};
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace

class ShapedWidgetDelegate : public WidgetDelegateView {
 public:
  ShapedWidgetDelegate() = default;

  ShapedWidgetDelegate(const ShapedWidgetDelegate&) = delete;
  ShapedWidgetDelegate& operator=(const ShapedWidgetDelegate&) = delete;

  ~ShapedWidgetDelegate() override = default;

  // WidgetDelegateView:
  std::unique_ptr<FrameView> CreateFrameView(Widget* widget) override {
    // Create a FrameView with a window mask with the bottom right
    // corner cut out.
    auto frame_view = std::make_unique<test::ConfigurableTestCustomFrameView>();
    frame_view->SetWindowMaskCallback(
        base::BindRepeating([](const gfx::Size& size, SkPath* window_mask) {
          int right = size.width();
          int bottom = size.height();

          *window_mask = SkPath::Polygon(
              {
                  SkPoint(0, 0),
                  SkPoint(0, bottom),
                  SkPoint(right, bottom),
                  SkPoint(right, 10),
                  SkPoint(right - 10, 10),
                  SkPoint(right - 10, 0),
              },
              /*isClosed=*/true);
        }));

    frame_view->SetHitTestCallback(
        base::BindRepeating([](const gfx::Point& point) -> int {
          if (point == gfx::Point(500, 500)) {
            return HTBOTTOM;
          }

          return HTNOWHERE;
        }));

    return std::move(frame_view);
  }
};

class DesktopWindowTreeHostPlatformImplTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostPlatformImplTest() = default;
  DesktopWindowTreeHostPlatformImplTest(
      const DesktopWindowTreeHostPlatformImplTest&) = delete;
  DesktopWindowTreeHostPlatformImplTest& operator=(
      const DesktopWindowTreeHostPlatformImplTest&) = delete;
  ~DesktopWindowTreeHostPlatformImplTest() override = default;

  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();
  }

 protected:
  // Creates a widget of size 100x100.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                              Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.remove_standard_frame = true;
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget->Init(std::move(params));
    return widget;
  }
};

TEST_F(DesktopWindowTreeHostPlatformImplTest,
       ChildWindowDestructionDuringTearDown) {
  Widget parent_widget;
  Widget::InitParams parent_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  parent_widget.Init(std::move(parent_params));
  parent_widget.Show();

  Widget child_widget;
  Widget::InitParams child_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  child_params.parent = parent_widget.GetNativeWindow();
  child_widget.Init(std::move(child_params));
  child_widget.Show();

  // Sanity check that the two widgets each have their own XID.
  ASSERT_NE(parent_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
            child_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  Widget::CloseAllWidgets();
  EXPECT_TRUE(DesktopWindowTreeHostPlatform::GetAllOpenWindows().empty());
}

TEST_F(DesktopWindowTreeHostPlatformImplTest, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  base::RunLoop().RunUntilIdle();

  widget->SetBounds(gfx::Rect(100, 100, 501, 501));

  base::RunLoop().RunUntilIdle();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  auto* host_platform = static_cast<DesktopWindowTreeHostPlatform*>(
      widget->GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_platform);

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::PointF(500, 500),
                       gfx::PointF(500, 500), base::TimeTicks::Now(), 0, 0, {});
  host_platform->DispatchEvent(&event);

  ASSERT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(ui::EventType::kMousePressed, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

// Checks that the visibility of the content window of
// `DesktopWindowTreeHostPlatform` matches the visibility of the ui compositor.
TEST_F(DesktopWindowTreeHostPlatformImplTest,
       ContentWindowVisibilityMatchesCompositorVisibility) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  // Disable native occlusion tracking so it doesn't interfere with visibility
  // for this test.
  aura::NativeWindowOcclusionTracker::DisableNativeWindowOcclusionTracking(
      widget->GetNativeWindow()->GetHost());
  widget->Show();
  base::RunLoop().RunUntilIdle();

  auto* host_platform = static_cast<DesktopWindowTreeHostPlatform*>(
      widget->GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_platform);
  auto* compositor = host_platform->compositor();
  ASSERT_TRUE(compositor);

  EXPECT_TRUE(compositor->IsVisible());
  EXPECT_TRUE(host_platform->GetContentWindow()->IsVisible());

  // Mark as not visible via `WindowTreeHost`.
  host_platform->Hide();
  EXPECT_FALSE(compositor->IsVisible());
  EXPECT_FALSE(host_platform->GetContentWindow()->IsVisible());

  // Mark as visible via `WindowTreeHost`.
  static_cast<aura::WindowTreeHost*>(host_platform)->Show();
  EXPECT_TRUE(compositor->IsVisible());
  EXPECT_TRUE(host_platform->GetContentWindow()->IsVisible());

  // Mark compositor directly as not visible.
  compositor->SetVisible(false);
  EXPECT_FALSE(compositor->IsVisible());
  EXPECT_FALSE(host_platform->GetContentWindow()->IsVisible());

  // Mark compositor directly as visible.
  compositor->SetVisible(true);
  EXPECT_TRUE(compositor->IsVisible());
  EXPECT_TRUE(host_platform->GetContentWindow()->IsVisible());
}

// Checks that a call to `SetZOrderLevel` on a `PlatformWindow` sets the z order
// on the associated `Widget`.
TEST_F(DesktopWindowTreeHostPlatformImplTest,
       SetZOrderCorrectlySetsZOrderOnPlatformWindows) {
  // We want the widget to be initialized with a non-default z order to check
  // that it gets initialized with the correct z order.
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  widget.Init(std::move(params));
  widget.Show();

  auto* host_platform = static_cast<DesktopWindowTreeHostPlatform*>(
      widget.GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_platform);

  auto* platform_window = host_platform->platform_window();
  ASSERT_EQ(ui::ZOrderLevel::kFloatingWindow,
            platform_window->GetZOrderLevel());
  ASSERT_EQ(ui::ZOrderLevel::kFloatingWindow, widget.GetZOrderLevel());

  platform_window->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_EQ(ui::ZOrderLevel::kNormal, platform_window->GetZOrderLevel());
  EXPECT_EQ(ui::ZOrderLevel::kNormal, widget.GetZOrderLevel());
}

class DesktopWindowTreeHostPlatformImplHighDPITest
    : public DesktopWindowTreeHostPlatformImplTest {
 public:
  DesktopWindowTreeHostPlatformImplHighDPITest() = default;
  ~DesktopWindowTreeHostPlatformImplHighDPITest() override = default;

 private:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

    DesktopWindowTreeHostPlatformImplTest::SetUp();
  }
};

TEST_F(DesktopWindowTreeHostPlatformImplHighDPITest, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  widget->SetBounds(gfx::Rect(100, 100, 1000, 1000));
  base::RunLoop().RunUntilIdle();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  auto* host_platform = static_cast<DesktopWindowTreeHostPlatform*>(
      widget->GetNativeWindow()->GetHost());
  ASSERT_TRUE(host_platform);

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::PointF(1001, 1001),
                       gfx::PointF(1001, 1001), base::TimeTicks::Now(), 0, 0,
                       {});
  host_platform->DispatchEvent(&event);

  EXPECT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(gfx::Point(500, 500), recorder.mouse_events()[0].location());
  EXPECT_EQ(ui::EventType::kMousePressed, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

#if BUILDFLAG(IS_LINUX)
// Verifies that disabling event listening (e.g. for a modal dialog) cancels
// active touches and clears held mouse button and drag-press state even if no
// release event reaches the host window, and that normal touch and mouse input
// resumes once event listening is re-enabled.
TEST_F(DesktopWindowTreeHostPlatformImplTest,
       DisableEventListeningClearsInputState) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  auto* native_widget = new DesktopNativeWidgetAura(widget.get());
  params.native_widget = native_widget;
  auto* host_linux =
      new DesktopWindowTreeHostLinux(widget.get(), native_widget);
  params.desktop_window_tree_host = host_linux;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  widget->Show();

  aura::Window* content_window = widget->GetNativeWindow();
  ASSERT_TRUE(content_window);
  ui::PlatformWindowDelegate* platform_delegate = host_linux;
  aura::WindowEventDispatcher* dispatcher =
      host_linux->AsWindowTreeHost()->dispatcher();

  TouchEventTypeRecorder touch_recorder;
  content_window->AddPreTargetHandler(&touch_recorder);

  auto* env = aura::Env::GetInstance();
  base::TimeTicks time = ui::EventTimeForNow();
  auto dispatch_touch = [&](ui::EventType type, const gfx::Point& location) {
    time += base::Milliseconds(10);
    ui::TouchEvent event(
        type, location, time,
        ui::PointerDetails(ui::EventPointerType::kTouch, /*pointer_id=*/0));
    platform_delegate->DispatchEvent(&event);
  };

  // Press a touch on the window, then disable event listening without
  // delivering a touch release (simulating a modal dialog consuming the
  // release).
  constexpr gfx::Point kPoint(50, 50);
  dispatch_touch(ui::EventType::kTouchPressed, kPoint);
  ASSERT_TRUE(env->is_touch_down());
  ASSERT_EQ(std::vector<ui::EventType>({ui::EventType::kTouchPressed}),
            touch_recorder.event_types());

  base::OnceClosure enable_events = host_linux->DisableEventListening();
  EXPECT_EQ(std::vector<ui::EventType>(
                {ui::EventType::kTouchPressed, ui::EventType::kTouchCancelled}),
            touch_recorder.event_types());
  EXPECT_FALSE(env->is_touch_down());
  EXPECT_FALSE(env->ConsumeTouchForDrag(content_window));

  std::move(enable_events).Run();
  EXPECT_FALSE(env->is_touch_down());
  EXPECT_FALSE(env->ConsumeTouchForDrag(content_window));

  // Likewise, a left mouse press held when event listening is disabled is
  // cleared even when the release is consumed outside the host window.
  time += base::Milliseconds(10);
  ui::MouseEvent mouse_press(ui::EventType::kMousePressed, kPoint, kPoint, time,
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  platform_delegate->DispatchEvent(&mouse_press);
  ASSERT_TRUE(env->IsLeftMouseButtonDown());
  ASSERT_EQ(content_window, dispatcher->mouse_pressed_handler());

  enable_events = host_linux->DisableEventListening();
  EXPECT_FALSE(env->IsLeftMouseButtonDown());
  EXPECT_FALSE(env->ConsumeLeftMousePressForDrag(content_window));
  EXPECT_EQ(nullptr, dispatcher->mouse_pressed_handler());

  std::move(enable_events).Run();
  EXPECT_FALSE(env->IsLeftMouseButtonDown());
  EXPECT_FALSE(env->ConsumeLeftMousePressForDrag(content_window));
  // Verify the recorded press target itself was cleared, not only the button
  // flags.
  env->set_mouse_button_flags(ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_FALSE(env->ConsumeLeftMousePressForDrag(content_window));
  env->set_mouse_button_flags(ui::EF_NONE);

  // Once event listening is re-enabled, new touch and mouse presses are
  // dispatched and tracked normally.
  touch_recorder.Reset();
  constexpr gfx::Point kNextPoint(60, 60);
  dispatch_touch(ui::EventType::kTouchPressed, kNextPoint);
  EXPECT_EQ(std::vector<ui::EventType>({ui::EventType::kTouchPressed}),
            touch_recorder.event_types());
  EXPECT_TRUE(env->is_touch_down());
  EXPECT_EQ(content_window->GetBoundsInScreen().origin() +
                kNextPoint.OffsetFromOrigin(),
            env->ConsumeTouchForDrag(content_window));
  dispatch_touch(ui::EventType::kTouchReleased, kNextPoint);
  EXPECT_FALSE(env->is_touch_down());

  time += base::Milliseconds(10);
  ui::MouseEvent next_mouse_press(ui::EventType::kMousePressed, kNextPoint,
                                  kNextPoint, time, ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON);
  platform_delegate->DispatchEvent(&next_mouse_press);
  EXPECT_TRUE(env->ConsumeLeftMousePressForDrag(content_window));
  time += base::Milliseconds(10);
  ui::MouseEvent next_mouse_release(ui::EventType::kMouseReleased, kNextPoint,
                                    kNextPoint, time, ui::EF_LEFT_MOUSE_BUTTON,
                                    ui::EF_LEFT_MOUSE_BUTTON);
  platform_delegate->DispatchEvent(&next_mouse_release);

  content_window->RemovePreTargetHandler(&touch_recorder);

  // Closing the widget synchronously during touch cancellation inside
  // DisableEventListening() must be handled safely.
  class CloseOnTouchCancelHandler : public ui::EventHandler {
   public:
    explicit CloseOnTouchCancelHandler(Widget* widget) : widget_(widget) {}
    void OnTouchEvent(ui::TouchEvent* touch) override {
      if (touch->type() == ui::EventType::kTouchCancelled) {
        widget_->GetNativeWindow()->RemovePreTargetHandler(this);
        widget_->CloseNow();
      }
    }

   private:
    raw_ptr<Widget> widget_;
  } close_handler(widget.get());

  content_window->AddPreTargetHandler(&close_handler);
  dispatch_touch(ui::EventType::kTouchPressed, kPoint);
  enable_events = host_linux->DisableEventListening();
  std::move(enable_events).Run();
  if (widget->GetNativeWindow()) {
    widget->GetNativeWindow()->RemovePreTargetHandler(&close_handler);
  }
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace views
