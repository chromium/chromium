// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <utility>

#include "base/command_line.h"
#include "ui/aura/native_window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display_switches.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// This tests the wayland and linux(x11) implementation of the
// DesktopWindowTreeHostPlatform.

namespace {
// A NonClientFrameView with a window mask with the bottom right corner cut out.
class ShapedNonClientFrameView : public NonClientFrameView {
 public:
  ShapedNonClientFrameView() = default;

  ShapedNonClientFrameView(const ShapedNonClientFrameView&) = delete;
  ShapedNonClientFrameView& operator=(const ShapedNonClientFrameView&) = delete;

  ~ShapedNonClientFrameView() override = default;

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Fake bottom for non client event test.
    if (point == gfx::Point(500, 500))
      return HTBOTTOM;
    return HTNOWHERE;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    int right = size.width();
    int bottom = size.height();

    window_mask->moveTo(0, 0);
    window_mask->lineTo(0, bottom);
    window_mask->lineTo(right, bottom);
    window_mask->lineTo(right, 10);
    window_mask->lineTo(right - 10, 10);
    window_mask->lineTo(right - 10, 0);
    window_mask->close();
  }
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  bool GetAndResetLayoutRequest() {
    bool layout_requested = layout_requested_;
    layout_requested_ = false;
    return layout_requested;
  }

 private:
  void Layout(PassKey) override { layout_requested_ = true; }

  bool layout_requested_ = false;
};

class ShapedWidgetDelegate : public WidgetDelegateView {
 public:
  ShapedWidgetDelegate() = default;

  ShapedWidgetDelegate(const ShapedWidgetDelegate&) = delete;
  ShapedWidgetDelegate& operator=(const ShapedWidgetDelegate&) = delete;

  ~ShapedWidgetDelegate() override = default;

  // WidgetDelegateView:
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override {
    return std::make_unique<ShapedNonClientFrameView>();
  }
};

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

}  // namespace

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
  Widget::CloseAllSecondaryWidgets();
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

}  // namespace views
