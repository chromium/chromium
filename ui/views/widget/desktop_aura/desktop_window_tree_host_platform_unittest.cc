// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/display/display_switches.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

class TestWidgetObserver : public WidgetObserver {
 public:
  enum class Change {
    kVisibility,
    kDestroying,
  };

  explicit TestWidgetObserver(Widget* widget) : widget_(widget) {
    DCHECK(widget_);
    widget_->AddObserver(this);
  }
  ~TestWidgetObserver() override {
    // This might have been destroyed by the widget destroying delegate call.
    if (widget_)
      widget_->RemoveObserver(this);
  }

  // Waits for notification changes for the |change|. |old_value| must be
  // provided to be sure that this is not called after the change has already
  // happened - e.g. synchronous change.
  void WaitForChange(Change change, bool old_value) {
    switch (change) {
      case Change::kVisibility:
        if (old_value == visible_)
          Wait();
        break;
      case Change::kDestroying:
        if (old_value == on_widget_destroying_)
          Wait();
        break;
      default:
        NOTREACHED() << "unknown value";
        break;
    }
  }

  bool widget_destroying() const { return on_widget_destroying_; }
  bool visible() const { return visible_; }

 private:
  // views::WidgetObserver overrides:
  void OnWidgetDestroying(Widget* widget) override {
    DCHECK_EQ(widget_, widget);
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    on_widget_destroying_ = true;
    StopWaiting();
  }
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    DCHECK_EQ(widget_, widget);
    visible_ = visible;
    StopWaiting();
  }

  void Wait() {
    ASSERT_FALSE(run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void StopWaiting() {
    if (!run_loop_)
      return;
    ASSERT_TRUE(run_loop_->running());
    run_loop_->Quit();
  }

  Widget* widget_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool on_widget_destroying_ = false;
  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWidgetObserver);
};

std::unique_ptr<Widget> CreateWidgetWithNativeWidget() {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

class DesktopWindowTreeHostPlatformTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostPlatformTest() {}
  ~DesktopWindowTreeHostPlatformTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostPlatformTest);
};

TEST_F(DesktopWindowTreeHostPlatformTest, CallOnNativeWidgetDestroying) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  TestWidgetObserver observer(widget->native_widget_private()->GetWidget());
  widget->CloseNow();

  observer.WaitForChange(TestWidgetObserver::Change::kDestroying,
                         false /* old_value */);
  EXPECT_TRUE(observer.widget_destroying());
}

// Calling show/hide/show triggers changing visibility of the native widget.
TEST_F(DesktopWindowTreeHostPlatformTest, CallOnNativeWidgetVisibilityChanged) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  TestWidgetObserver observer(widget->native_widget_private()->GetWidget());
  EXPECT_FALSE(observer.visible());

  widget->Show();
  EXPECT_TRUE(observer.visible());

  widget->Hide();
  EXPECT_FALSE(observer.visible());

  widget->Show();
  EXPECT_TRUE(observer.visible());
}

// Tests that the minimization information is propagated to the content window.
TEST_F(DesktopWindowTreeHostPlatformTest,
       ToggleMinimizePropogateToContentWindow) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();
  widget->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(host_platform);

  EXPECT_TRUE(widget->GetNativeWindow()->IsVisible());

  // Pretend a PlatformWindow enters the minimized state.
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kMinimized);

  EXPECT_FALSE(widget->GetNativeWindow()->IsVisible());

  // Pretend a PlatformWindow exits the minimized state.
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kNormal);
  EXPECT_TRUE(widget->GetNativeWindow()->IsVisible());
}

// Tests that the window shape is updated from the
// |NonClientView::GetWindowMask|.
TEST_F(DesktopWindowTreeHostPlatformTest, UpdateWindowShapeFromWindowMask) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();
  widget->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(host_platform);
  if (!host_platform->platform_window()->ShouldUseLayerForShapedWindow())
    return;

  auto* content_window =
      DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
          widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(content_window);
  // alpha_shape for the layer of content window is updated from the
  // |NonClientView::GetWindowMask|.
  EXPECT_FALSE(host_platform->GetWindowMaskForWindowShapeInPixels().isEmpty());
  EXPECT_TRUE(content_window->layer()->alpha_shape());

  // When fullscreen mode, alpha_shape is set to empty since there is no
  // |NonClientView::GetWindowMask|.
  host_platform->SetFullscreen(true);
  widget->SetBounds(gfx::Rect(800, 800));
  EXPECT_TRUE(host_platform->GetWindowMaskForWindowShapeInPixels().isEmpty());
  EXPECT_FALSE(content_window->layer()->alpha_shape());
}

// A Widget that allows setting the min/max size for the widget.
class CustomSizeWidget : public Widget {
 public:
  CustomSizeWidget() = default;
  ~CustomSizeWidget() override = default;

  void set_min_size(const gfx::Size& size) { min_size_ = size; }
  void set_max_size(const gfx::Size& size) { max_size_ = size; }

  // Widget:
  gfx::Size GetMinimumSize() const override { return min_size_; }
  gfx::Size GetMaximumSize() const override { return max_size_; }

 private:
  gfx::Size min_size_;
  gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomSizeWidget);
};

TEST_F(DesktopWindowTreeHostPlatformTest, SetBoundsWithMinMax) {
  CustomSizeWidget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(200, 100);
  widget.Init(std::move(params));
  widget.Show();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(gfx::Size(200, 100).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
  widget.SetBounds(gfx::Rect(300, 200));
  EXPECT_EQ(gfx::Size(300, 200).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());

  widget.set_min_size(gfx::Size(100, 100));
  widget.SetBounds(gfx::Rect(50, 500));
  EXPECT_EQ(gfx::Size(100, 500).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
}

class ResizeObserver : public aura::WindowTreeHostObserver {
 public:
  explicit ResizeObserver(aura::WindowTreeHost* host) : host_(host) {
    host_->AddObserver(this);
  }
  ResizeObserver(const ResizeObserver&) = delete;
  ResizeObserver& operator=(const ResizeObserver&) = delete;
  ~ResizeObserver() override { host_->RemoveObserver(this); }

  int bounds_change_count() const { return bounds_change_count_; }
  int resize_count() const { return resize_count_; }

  // aura::WindowTreeHostObserver:
  void OnHostResized(aura::WindowTreeHost* host) override { resize_count_++; }
  void OnHostWillProcessBoundsChange(aura::WindowTreeHost* host) override {
    bounds_change_count_++;
  }

 private:
  aura::WindowTreeHost* const host_;
  int resize_count_ = 0;
  int bounds_change_count_ = 0;
};

// Verifies that setting widget bounds, just after creating it, with the same
// size passed in InitParams does not lead to a "bounds change" event. Prevents
// regressions, such as https://crbug.com/1151092.
TEST_F(DesktopWindowTreeHostPlatformTest, SetBoundsWithUnchangedSize) {
  auto widget = CreateWidgetWithNativeWidget();
  widget->Show();

  EXPECT_EQ(gfx::Size(100, 100), widget->GetWindowBoundsInScreen().size());
  auto* host = widget->GetNativeWindow()->GetHost();
  ResizeObserver observer(host);

  auto* dwth_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(dwth_platform);

  // Check with different origin.
  dwth_platform->SetBoundsInPixels(gfx::Rect(2, 2, 100, 100));
  EXPECT_EQ(1, observer.bounds_change_count());
  EXPECT_EQ(0, observer.resize_count());
}

class DesktopWindowTreeHostPlatformHighDPITest
    : public DesktopWindowTreeHostPlatformTest {
 public:
  DesktopWindowTreeHostPlatformHighDPITest() = default;
  ~DesktopWindowTreeHostPlatformHighDPITest() override = default;

 private:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

    DesktopWindowTreeHostPlatformTest::SetUp();
  }
};

// Tests that the window shape is updated properly from the
// |NonClientView::GetWindowMask| in HighDPI.
TEST_F(DesktopWindowTreeHostPlatformHighDPITest, VerifyWindowShapeInHighDPI) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();
  widget->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(host_platform);
  if (!host_platform->platform_window()->ShouldUseLayerForShapedWindow())
    return;

  auto* content_window =
      DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
          widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(content_window);
  // Check device scale factor.
  EXPECT_EQ(host_platform->device_scale_factor(), 2.0);

  // alpha_shape for the layer of content window is updated from the
  // |NonClientView::GetWindowMask|.
  SkPath path_in_pixels = host_platform->GetWindowMaskForWindowShapeInPixels();
  EXPECT_FALSE(path_in_pixels.isEmpty());

  // Converts path to DIPs and calculates expected region from it.
  SkPath path_in_dips;
  path_in_pixels.transform(
      SkMatrix(host_platform->GetInverseRootTransform().matrix()),
      &path_in_dips);
  SkRegion region;
  region.setRect(path_in_dips.getBounds().round());
  SkRegion expected_region;
  expected_region.setPath(path_in_dips, region);

  SkRegion shape_region;
  for (auto& bound : *(content_window->layer()->alpha_shape()))
    shape_region.op(gfx::RectToSkIRect(bound), SkRegion::Op::kUnion_Op);

  // Test that region from alpha_shape is same as the expected region from path
  // in DIPs.
  EXPECT_EQ(shape_region, expected_region);
}

}  // namespace views
