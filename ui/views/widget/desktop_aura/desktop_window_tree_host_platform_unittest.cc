// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_switches.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if !BUILDFLAG(IS_FUCHSIA)
#include "ui/aura/window_tree_host_platform.h"    // nogncheck
#include "ui/platform_window/stub/stub_window.h"  // nogncheck
#endif

namespace views {

namespace {

class TestWidgetObserver : public WidgetObserver {
 public:
  enum class Change {
    kVisibility,
    kDestroying,
  };

  explicit TestWidgetObserver(Widget* widget) {
    DCHECK(widget);
    widget_observation_.Observe(widget);
  }

  TestWidgetObserver(const TestWidgetObserver&) = delete;
  TestWidgetObserver& operator=(const TestWidgetObserver&) = delete;

  ~TestWidgetObserver() override = default;

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
    }
  }

  bool widget_destroying() const { return on_widget_destroying_; }
  bool visible() const { return visible_; }

 private:
  // views::WidgetObserver overrides:
  void OnWidgetDestroying(Widget* widget) override {
    DCHECK(widget_observation_.IsObservingSource(widget));
    widget_observation_.Reset();
    on_widget_destroying_ = true;
    StopWaiting();
  }
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    DCHECK(widget_observation_.IsObservingSource(widget));
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

  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
  bool on_widget_destroying_ = false;
  bool visible_ = false;
};

std::unique_ptr<Widget> CreateWidgetWithNativeWidgetWithParams(
    Widget::InitParams params) {
  auto widget = std::make_unique<Widget>();
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  widget->Init(std::move(params));
  return widget;
}

std::unique_ptr<Widget> CreateWidgetWithNativeWidget() {
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(100, 100, 100, 100);
  return CreateWidgetWithNativeWidgetWithParams(std::move(params));
}

}  // namespace

class DesktopWindowTreeHostPlatformTest : public ViewsTestBase {
 public:
  DesktopWindowTreeHostPlatformTest() = default;

  DesktopWindowTreeHostPlatformTest(const DesktopWindowTreeHostPlatformTest&) =
      delete;
  DesktopWindowTreeHostPlatformTest& operator=(
      const DesktopWindowTreeHostPlatformTest&) = delete;

  ~DesktopWindowTreeHostPlatformTest() override = default;
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
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kUnknown,
                                      ui::PlatformWindowState::kMinimized);

  EXPECT_FALSE(widget->GetNativeWindow()->IsVisible());

  // Pretend a PlatformWindow exits the minimized state.
  host_platform->OnWindowStateChanged(ui::PlatformWindowState::kMinimized,
                                      ui::PlatformWindowState::kNormal);
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
  if (!host_platform->platform_window()->ShouldUpdateWindowShape())
    return;

  auto* content_window =
      DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
          widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(content_window);
  EXPECT_FALSE(host_platform->GetWindowMaskForWindowShapeInPixels().isEmpty());
  // SetClipPath for the layer of the content window is updated from it.
  EXPECT_FALSE(host_platform->GetWindowMaskForClipping().isEmpty());
  EXPECT_FALSE(widget->GetLayer()->FillsBoundsCompletely());

  // When fullscreen mode, clip_path_ is set to empty since there is no
  // |NonClientView::GetWindowMask|.
  host_platform->SetFullscreen(true, display::kInvalidDisplayId);
  widget->SetBounds(gfx::Rect(800, 800));
  EXPECT_TRUE(host_platform->GetWindowMaskForWindowShapeInPixels().isEmpty());
  EXPECT_TRUE(host_platform->GetWindowMaskForClipping().isEmpty());
  EXPECT_TRUE(widget->GetLayer()->FillsBoundsCompletely());
}

// Calling show/hide/show triggers changing visibility of the native widget.
TEST_F(DesktopWindowTreeHostPlatformTest,
       OnAcceleratedWidgetMadeVisibleCalled) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  ASSERT_TRUE(host_platform);
  EXPECT_FALSE(aura::test::AcceleratedWidgetMadeVisible(host_platform));

  widget->Show();
  EXPECT_TRUE(aura::test::AcceleratedWidgetMadeVisible(host_platform));

  widget->Hide();
  EXPECT_FALSE(aura::test::AcceleratedWidgetMadeVisible(host_platform));

  widget->Show();
  EXPECT_TRUE(aura::test::AcceleratedWidgetMadeVisible(host_platform));
}

// A Widget that allows setting the min/max size for the widget.
class CustomSizeWidget : public Widget {
 public:
  CustomSizeWidget() = default;

  CustomSizeWidget(const CustomSizeWidget&) = delete;
  CustomSizeWidget& operator=(const CustomSizeWidget&) = delete;

  ~CustomSizeWidget() override = default;

  void set_min_size(const gfx::Size& size) { min_size_ = size; }
  void set_max_size(const gfx::Size& size) { max_size_ = size; }

  // Widget:
  gfx::Size GetMinimumSize() const override { return min_size_; }
  gfx::Size GetMaximumSize() const override { return max_size_; }

 private:
  gfx::Size min_size_;
  gfx::Size max_size_;
};

TEST_F(DesktopWindowTreeHostPlatformTest, SetBoundsWithMinMax) {
  CustomSizeWidget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  const raw_ptr<aura::WindowTreeHost> host_;
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

TEST_F(DesktopWindowTreeHostPlatformTest, MakesParentChildRelationship) {
  bool context_is_also_parent = false;
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformProperties()
          .set_parent_for_non_top_level_windows) {
    context_is_also_parent = true;
  }
#endif
  auto widget = CreateWidgetWithNativeWidget();
  widget->Show();

  Widget::InitParams widget_2_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                     Widget::InitParams::TYPE_MENU);
  widget_2_params.bounds = gfx::Rect(110, 110, 100, 100);
  widget_2_params.parent = widget->GetNativeWindow();
  auto widget2 =
      CreateWidgetWithNativeWidgetWithParams(std::move(widget_2_params));
  widget2->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  EXPECT_EQ(host_platform->window_parent_, nullptr);
  EXPECT_EQ(host_platform->window_children_.size(), 1u);

  auto* host_platform2 = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget2->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  EXPECT_EQ(host_platform2->window_parent_, host_platform);
  EXPECT_EQ(*host_platform->window_children_.begin(), host_platform2);

  Widget::InitParams widget_3_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                     Widget::InitParams::TYPE_MENU);
  widget_3_params.bounds = gfx::Rect(120, 120, 50, 80);
  widget_3_params.parent = widget->GetNativeWindow();
  auto widget3 =
      CreateWidgetWithNativeWidgetWithParams(std::move(widget_3_params));
  widget3->Show();

  EXPECT_EQ(host_platform->window_parent_, nullptr);
  EXPECT_EQ(host_platform->window_children_.size(), 2u);

  auto* host_platform3 = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget3->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  EXPECT_EQ(host_platform3->window_parent_, host_platform);
  EXPECT_NE(host_platform->window_children_.find(host_platform3),
            host_platform->window_children_.end());

  Widget::InitParams widget_4_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                     Widget::InitParams::TYPE_TOOLTIP);
  widget_4_params.bounds = gfx::Rect(105, 105, 10, 10);
  widget_4_params.context = widget->GetNativeWindow();
  auto widget4 =
      CreateWidgetWithNativeWidgetWithParams(std::move(widget_4_params));
  widget4->Show();

  EXPECT_EQ(host_platform->window_parent_, nullptr);
  auto* host_platform4 = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget4->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  if (context_is_also_parent) {
    EXPECT_EQ(host_platform->window_children_.size(), 3u);
    EXPECT_EQ(host_platform4->window_parent_, host_platform);
    EXPECT_NE(host_platform->window_children_.find(host_platform4),
              host_platform->window_children_.end());
  } else {
    EXPECT_EQ(host_platform4->window_parent_, nullptr);
    EXPECT_EQ(host_platform->window_children_.size(), 2u);
    EXPECT_NE(host_platform->window_children_.find(host_platform3),
              host_platform->window_children_.end());
  }
}

class TestWidgetDelegate : public WidgetDelegate {
 public:
  TestWidgetDelegate() = default;
  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate operator=(const TestWidgetDelegate&) = delete;
  ~TestWidgetDelegate() override = default;

  void GetAccessiblePanes(std::vector<View*>* panes) override {
    base::ranges::copy(accessible_panes_, std::back_inserter(*panes));
  }

  void AddAccessiblePane(View* pane) { accessible_panes_.push_back(pane); }

 private:
  std::vector<raw_ptr<View, VectorExperimental>> accessible_panes_;
};

TEST_F(DesktopWindowTreeHostPlatformTest, OnRotateFocus) {
  using Direction = ui::PlatformWindowDelegate::RotateDirection;

  auto delegate = std::make_unique<TestWidgetDelegate>();
  Widget::InitParams widget_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget_params.bounds = gfx::Rect(110, 110, 100, 100);
  widget_params.delegate = delegate.get();
  auto widget = std::make_unique<Widget>();
  widget->Init(std::move(widget_params));

  View* views[2];
  for (auto*& view : views) {
    auto child_view = std::make_unique<View>();
    child_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);

    auto pane = std::make_unique<AccessiblePaneView>();
    delegate->AddAccessiblePane(pane.get());
    view = pane->AddChildView(std::move(child_view));
    widget->GetContentsView()->AddChildView(std::move(pane));
  }
  widget->Show();
  ASSERT_TRUE(widget->IsActive());

  auto* focus_manager = widget->GetFocusManager();

  // Start rotating from start.
  EXPECT_TRUE(DesktopWindowTreeHostPlatform::RotateFocusForWidget(
      *widget, Direction::kForward, true));
  EXPECT_EQ(views[0], focus_manager->GetFocusedView());

  EXPECT_TRUE(DesktopWindowTreeHostPlatform::RotateFocusForWidget(
      *widget, Direction::kForward, false));
  EXPECT_EQ(views[1], focus_manager->GetFocusedView());

  EXPECT_TRUE(DesktopWindowTreeHostPlatform::RotateFocusForWidget(
      *widget, Direction::kBackward, false));
  EXPECT_EQ(views[0], focus_manager->GetFocusedView());

  // Attempting to rotate again without resetting should notify that we've
  // reached the end.
  EXPECT_FALSE(DesktopWindowTreeHostPlatform::RotateFocusForWidget(
      *widget, Direction::kBackward, false));
  EXPECT_EQ(views[0], focus_manager->GetFocusedView());

  // Restart rotating from back.
  EXPECT_TRUE(DesktopWindowTreeHostPlatform::RotateFocusForWidget(
      *widget, Direction::kBackward, true));
  EXPECT_EQ(views[1], focus_manager->GetFocusedView());
}

TEST_F(DesktopWindowTreeHostPlatformTest, CanMaximize) {
  auto widget = CreateWidgetWithNativeWidget();
  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  widget->widget_delegate()->SetCanMaximize(true);
  EXPECT_TRUE(host_platform->CanMaximize());

  widget->widget_delegate()->SetCanMaximize(false);
  EXPECT_FALSE(host_platform->CanMaximize());
}

TEST_F(DesktopWindowTreeHostPlatformTest, CanFullscreen) {
  auto widget = CreateWidgetWithNativeWidget();
  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  widget->widget_delegate()->SetCanFullscreen(true);
  EXPECT_TRUE(host_platform->CanFullscreen());

  widget->widget_delegate()->SetCanFullscreen(false);
  EXPECT_FALSE(host_platform->CanFullscreen());
}

#if !BUILDFLAG(IS_FUCHSIA)
class ScopedPlatformWindowFactoryDelegate
    : public aura::WindowTreeHostPlatform::
          PlatformWindowFactoryDelegateForTesting {
 public:
  ScopedPlatformWindowFactoryDelegate() {
    aura::WindowTreeHostPlatform::SetPlatformWindowFactoryDelegateForTesting(
        this);
  }
  ScopedPlatformWindowFactoryDelegate(
      const ScopedPlatformWindowFactoryDelegate&) = delete;
  ScopedPlatformWindowFactoryDelegate& operator=(
      const ScopedPlatformWindowFactoryDelegate&) = delete;
  ~ScopedPlatformWindowFactoryDelegate() override {
    aura::WindowTreeHostPlatform::SetPlatformWindowFactoryDelegateForTesting(
        nullptr);
  }

  std::unique_ptr<ui::PlatformWindow> Create(
      aura::WindowTreeHostPlatform* host) override {
    auto stub_ptr = std::make_unique<ui::StubWindow>(gfx::Rect());
    stub_ptr->InitDelegateWithWidget(/*delegate=*/host,
                                     /*widget=*/++last_accelerated_widget_);
    return std::move(stub_ptr);
  }
  gfx::AcceleratedWidget last_accelerated_widget_ = gfx::kNullAcceleratedWidget;
};

TEST_F(DesktopWindowTreeHostPlatformTest, ShowInitiallyMinimizedWidget) {
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(100, 100, 100, 100);
  params.show_state = ui::mojom::WindowShowState::kMinimized;
  std::unique_ptr<ScopedPlatformWindowFactoryDelegate>
      scoped_platform_window_factory_delegate(
          new ScopedPlatformWindowFactoryDelegate);
  std::unique_ptr<Widget> widget =
      CreateWidgetWithNativeWidgetWithParams(std::move(params));
  scoped_platform_window_factory_delegate.reset();

  // Calling `Widget::Show()` for a widget initially created as minimized does
  // not cause the widget to get activated yet. (i.e. stays minimized). This
  // happens specifically when the widget is created as part of a session
  // restore and `Widget::Show()` is called for initialization and not to
  // actually show the widget. The widget will use `Widget::saved_show_state_`
  // to pass to the native widget and window tree host. Essentially this is
  // testing that `DesktopWindowTreeHostPlatform` does not get activated if
  // `DesktopWindowTreeHostPlatform::Show()` is called with
  // `ui::mojom::WindowShowState::kMinimized`.
  widget->Show();
  EXPECT_FALSE(widget->IsActive());

  widget->Show();
  EXPECT_TRUE(widget->IsActive());
}

TEST_F(DesktopWindowTreeHostPlatformTest, FocusParentWindowWillActivate) {
#if BUILDFLAG(IS_OZONE)
  if (!base::FeatureList::IsEnabled(
          features::kOzoneBubblesUsePlatformWidgets)) {
    GTEST_SKIP();
  }
#endif

  std::unique_ptr<ScopedPlatformWindowFactoryDelegate>
      scoped_platform_window_factory_delegate(
          new ScopedPlatformWindowFactoryDelegate);

  auto widget = CreateWidgetWithNativeWidget();
  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  widget->Show();

  Widget::InitParams widget_2_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                     Widget::InitParams::TYPE_BUBBLE);
  widget_2_params.bounds = gfx::Rect(110, 110, 100, 100);
  widget_2_params.parent = widget->GetNativeWindow();
  auto widget2 =
      CreateWidgetWithNativeWidgetWithParams(std::move(widget_2_params));
  widget2->Show();

  // Deactivate toplevel.
  host_platform->Deactivate();
  EXPECT_FALSE(host_platform->IsActive());

  // Set focus in the toplevel content_window.
  std::unique_ptr<aura::Window> child_window =
      std::make_unique<aura::Window>(nullptr);
  child_window->Init(ui::LAYER_NOT_DRAWN);
  host_platform->GetContentWindow()->AddChild(child_window.get());
  auto* focus_client = aura::client::GetFocusClient(host_platform->window());
  focus_client->FocusWindow(child_window.get());

  // Toplevel should be active.
  EXPECT_TRUE(host_platform->IsActive());
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

class VisibilityObserver : public aura::WindowObserver {
 public:
  VisibilityObserver() = default;
  int shown() const { return shown_; }
  void OnWindowVisibilityChanging(aura::Window*, bool visible) override {
    if (visible) {
      shown_++;
    }
  }

 private:
  int shown_ = 0;
};

TEST_F(DesktopWindowTreeHostPlatformTest, ContentWindowShownOnce) {
  std::unique_ptr<Widget> widget = CreateWidgetWithNativeWidget();
  widget->Show();

  auto* host_platform = DesktopWindowTreeHostPlatform::GetHostForWidget(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  VisibilityObserver observer;
  host_platform->GetContentWindow()->AddObserver(&observer);

  widget->Hide();
  widget->SetOpacity(0.f);
  widget->Show();

  // Show is only called once.
  EXPECT_EQ(observer.shown(), 1);

  host_platform->GetContentWindow()->RemoveObserver(&observer);
}

}  // namespace views
