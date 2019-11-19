// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/hit_test.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/views/test/views_interactive_ui_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"

namespace views {

namespace {

bool IsNonClientComponent(int hittest) {
  switch (hittest) {
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTCAPTION:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT:
      return true;
    default:
      return false;
  }
  return true;
}

// A fake handler, which just stores the hittest and pointer location values.
class FakeWmMoveResizeHandler : public ui::WmMoveResizeHandler {
 public:
  using SetBoundsCallback = base::RepeatingCallback<void(gfx::Rect)>;
  explicit FakeWmMoveResizeHandler(ui::PlatformWindowBase* window)
      : platform_window_(window), hittest_(-1) {}
  ~FakeWmMoveResizeHandler() override = default;

  void Reset() {
    hittest_ = -1;
    pointer_location_in_px_ = gfx::Point();
  }

  int hittest() const { return hittest_; }
  gfx::Point pointer_location_in_px() const { return pointer_location_in_px_; }

  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // ui::WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override {
    hittest_ = hittest;
    pointer_location_in_px_ = pointer_location_in_px;

    platform_window_->SetBounds(bounds_);
  }

 private:
  ui::PlatformWindowBase* platform_window_;
  gfx::Rect bounds_;

  int hittest_ = -1;
  gfx::Point pointer_location_in_px_;

  DISALLOW_COPY_AND_ASSIGN(FakeWmMoveResizeHandler);
};

void SetExpectationBasedOnHittestValue(
    int hittest,
    const FakeWmMoveResizeHandler& handler,
    const gfx::Point& pointer_location_in_px) {
  if (IsNonClientComponent(hittest)) {
    // Ensure both the pointer location and the hit test value are passed to the
    // fake move/resize handler.
    EXPECT_EQ(handler.pointer_location_in_px().ToString(),
              pointer_location_in_px.ToString());
    EXPECT_EQ(handler.hittest(), hittest);
    return;
  }

  // Ensure the handler does not receive the hittest value or the pointer
  // location.
  EXPECT_TRUE(handler.pointer_location_in_px().IsOrigin());
  EXPECT_NE(handler.hittest(), hittest);
}

// This is used to return a customized result to NonClientHitTest.
class HitTestNonClientFrameView : public NativeFrameView {
 public:
  explicit HitTestNonClientFrameView(Widget* widget)
      : NativeFrameView(widget) {}
  ~HitTestNonClientFrameView() override = default;

  void set_hit_test_result(int component) { hit_test_result_ = component; }

  // NonClientFrameView overrides:
  int NonClientHitTest(const gfx::Point& point) override {
    return hit_test_result_;
  }

 private:
  int hit_test_result_ = HTNOWHERE;

  DISALLOW_COPY_AND_ASSIGN(HitTestNonClientFrameView);
};

// This is used to return HitTestNonClientFrameView on create call.
class HitTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit HitTestWidgetDelegate(views::Widget* widget) : widget_(widget) {}
  ~HitTestWidgetDelegate() override = default;

  void set_can_resize(bool can_resize) {
    can_resize_ = can_resize;
    widget_->OnSizeConstraintsChanged();
  }

  HitTestNonClientFrameView* frame_view() { return frame_view_; }

  // views::WidgetDelegate:
  bool CanResize() const override { return can_resize_; }
  views::Widget* GetWidget() override { return widget_; }
  views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(Widget* widget) override {
    DCHECK(widget_ == widget);
    if (!frame_view_)
      frame_view_ = new HitTestNonClientFrameView(widget);
    return frame_view_;
  }
  void DeleteDelegate() override { delete this; }

 private:
  views::Widget* const widget_;
  HitTestNonClientFrameView* frame_view_ = nullptr;
  bool can_resize_ = false;

  DISALLOW_COPY_AND_ASSIGN(HitTestWidgetDelegate);
};

// Test host that can intercept calls to the real host.
class TestDesktopWindowTreeHostLinux : public DesktopWindowTreeHostLinux {
 public:
  TestDesktopWindowTreeHostLinux(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura)
      : DesktopWindowTreeHostLinux(native_widget_delegate,
                                   desktop_native_widget_aura) {}
  ~TestDesktopWindowTreeHostLinux() override = default;

  // PlatformWindowDelegateBase:
  // Instead of making these tests friends of the host, override the dispatch
  // method to make it public and nothing else.
  void DispatchEvent(ui::Event* event) override {
    DesktopWindowTreeHostLinux::DispatchEvent(event);
  }

  void ResetCalledMaximize() { called_maximize_ = false; }
  bool called_maximize() const { return called_maximize_; }
  // DesktopWindowTreeHost
  void Maximize() override {
    called_maximize_ = true;
    DesktopWindowTreeHostLinux::Maximize();
  }

 private:
  bool called_maximize_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestDesktopWindowTreeHostLinux);
};

}  // namespace

class DesktopWindowTreeHostLinuxTest : public ViewsInteractiveUITestBase {
 public:
  DesktopWindowTreeHostLinuxTest() = default;
  ~DesktopWindowTreeHostLinuxTest() override = default;

 protected:
  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds) {
    Widget* toplevel = new Widget;
    delegate_ = new HitTestWidgetDelegate(toplevel);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    auto* native_widget = new views::DesktopNativeWidgetAura(toplevel);
    toplevel_params.native_widget = native_widget;
    host_ = new TestDesktopWindowTreeHostLinux(toplevel, native_widget);
    toplevel_params.desktop_window_tree_host = host_;
    toplevel_params.delegate = delegate_;
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_params.bounds = bounds;
    toplevel_params.remove_standard_frame = true;
    toplevel->Init(std::move(toplevel_params));
    return toplevel;
  }

  void GenerateAndDispatchMouseEvent(ui::EventType event_type,
                                     const gfx::Point& click_location,
                                     int flags) {
    DCHECK(host_);
    std::unique_ptr<ui::MouseEvent> mouse_event(
        GenerateMouseEvent(event_type, click_location, flags));
    host_->DispatchEvent(mouse_event.get());
  }

  void GenerateAndDispatchClickMouseEvent(const gfx::Point& click_location,
                                          int flags) {
    DCHECK(host_);
    GenerateAndDispatchMouseEvent(ui::ET_MOUSE_PRESSED, click_location, flags);
    GenerateAndDispatchMouseEvent(ui::ET_MOUSE_RELEASED, click_location, flags);
  }

  ui::MouseEvent* GenerateMouseEvent(ui::EventType event_type,
                                     const gfx::Point& click_location,
                                     int flags) {
    int flag = 0;
    if (flags & ui::EF_LEFT_MOUSE_BUTTON)
      flag = ui::EF_LEFT_MOUSE_BUTTON;
    else if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
      flag = ui::EF_RIGHT_MOUSE_BUTTON;

    if (!flag) {
      NOTREACHED()
          << "Other mouse clicks are not supported yet. Add the new one.";
    }
    return new ui::MouseEvent(event_type, click_location, click_location,
                              base::TimeTicks::Now(), flags, flag);
  }

  HitTestWidgetDelegate* delegate_ = nullptr;
  TestDesktopWindowTreeHostLinux* host_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostLinuxTest);
};

TEST_F(DesktopWindowTreeHostLinuxTest, HitTest) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(bounds));
  widget->Show();

  // Install a fake move/resize handler to intercept the move/resize call.
  auto handler =
      std::make_unique<FakeWmMoveResizeHandler>(host_->platform_window());
  host_->DestroyNonClientEventFilter();
  host_->non_client_window_event_filter_ =
      std::make_unique<WindowEventFilterLinux>(host_, handler.get());
  delegate_->set_can_resize(true);

  // It is not important to use pointer locations corresponding to the hittests
  // values used in the browser itself, because we fake the hit test results,
  // which non client frame view sends back. Thus, just make sure the content
  // window is able to receive these events.
  gfx::Point pointer_location_in_px(10, 10);

  constexpr int hittest_values[] = {
      HTBOTTOM, HTBOTTOMLEFT, HTBOTTOMRIGHT, HTCAPTION,     HTLEFT,
      HTRIGHT,  HTTOP,        HTTOPLEFT,     HTTOPRIGHT,    HTNOWHERE,
      HTBORDER, HTCLIENT,     HTCLOSE,       HTERROR,       HTGROWBOX,
      HTHELP,   HTHSCROLL,    HTMENU,        HTMAXBUTTON,   HTMINBUTTON,
      HTREDUCE, HTSIZE,       HTSYSMENU,     HTTRANSPARENT, HTVSCROLL,
      HTZOOM,
  };

  aura::Window* window = widget->GetNativeWindow();
  auto* frame_view = delegate_->frame_view();
  for (int hittest : hittest_values) {
    handler->Reset();

    // Set the desired hit test result value, which will be returned, when
    // WindowEventFilter starts to perform hit testing.
    frame_view->set_hit_test_result(hittest);

    gfx::Rect bounds = window->GetBoundsInScreen();

    // The wm move/resize handler receives pointer location in the global screen
    // coordinate, whereas event dispatcher receives event locations on a local
    // system coordinate. Thus, add an offset of a new possible origin value of
    // a window to the expected pointer location.
    gfx::Point expected_pointer_location_in_px(pointer_location_in_px);
    expected_pointer_location_in_px.Offset(bounds.x(), bounds.y());

    if (hittest == HTCAPTION) {
      // Move the window on HTCAPTION hit test value.
      bounds =
          gfx::Rect(gfx::Point(bounds.x() + 2, bounds.y() + 4), bounds.size());
      handler->set_bounds(bounds);
    } else if (IsNonClientComponent(hittest)) {
      // Resize the window on other than HTCAPTION non client hit test values.
      bounds = gfx::Rect(
          gfx::Point(bounds.origin()),
          gfx::Size(bounds.size().width() + 5, bounds.size().height() + 10));
      handler->set_bounds(bounds);
    }

    // Send mouse down event and make sure the WindowEventFilter calls the
    // move/resize handler to start interactive move/resize with the |hittest|
    // value we specified.
    GenerateAndDispatchMouseEvent(ui::ET_MOUSE_PRESSED, pointer_location_in_px,
                                  ui::EF_LEFT_MOUSE_BUTTON);

    // The test expectation is based on the hit test component. If it is a
    // non-client component, which results in a call to move/resize, the
    // handler must receive the hittest value and the pointer location in
    // global screen coordinate system. In other cases, it must not.
    SetExpectationBasedOnHittestValue(hittest, *handler.get(),
                                      expected_pointer_location_in_px);
    // Make sure the bounds of the content window are correct.
    EXPECT_EQ(window->GetBoundsInScreen().ToString(), bounds.ToString());

    // Dispatch mouse release event to release a mouse pressed handler and be
    // able to consume future events.
    GenerateAndDispatchMouseEvent(ui::ET_MOUSE_RELEASED, pointer_location_in_px,
                                  ui::EF_LEFT_MOUSE_BUTTON);
  }
}

// Tests that the window is maximized in response to a double click event.
TEST_F(DesktopWindowTreeHostLinuxTest, DoubleClickHeaderMaximizes) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(bounds));
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  RunPendingMessages();

  host_->ResetCalledMaximize();

  auto* frame_view = delegate_->frame_view();
  // Set the desired hit test result value, which will be returned, when
  // WindowEventFilter starts to perform hit testing.
  frame_view->set_hit_test_result(HTCAPTION);

  host_->ResetCalledMaximize();

  int flags = ui::EF_LEFT_MOUSE_BUTTON;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);
  flags |= ui::EF_IS_DOUBLE_CLICK;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);

  EXPECT_TRUE(host_->called_maximize());

  widget->CloseNow();
}

// Tests that the window does not maximize in response to a double click event,
// if the first click was to a different target component than that of the
// second click.
TEST_F(DesktopWindowTreeHostLinuxTest,
       DoubleClickTwoDifferentTargetsDoesntMaximizes) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(bounds));
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  RunPendingMessages();

  host_->ResetCalledMaximize();

  auto* frame_view = delegate_->frame_view();

  frame_view->set_hit_test_result(HTCLIENT);
  int flags = ui::EF_LEFT_MOUSE_BUTTON;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);

  frame_view->set_hit_test_result(HTCLIENT);
  flags |= ui::EF_IS_DOUBLE_CLICK;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);

  EXPECT_FALSE(host_->called_maximize());

  widget->CloseNow();
}

// Tests that the window does not maximize in response to a double click event,
// if the double click was interrupted by a right click.
TEST_F(DesktopWindowTreeHostLinuxTest,
       RightClickDuringDoubleClickDoesntMaximize) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(bounds));
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  RunPendingMessages();

  host_->ResetCalledMaximize();

  auto* frame_view = delegate_->frame_view();

  frame_view->set_hit_test_result(HTCLIENT);
  int flags_left_button = ui::EF_LEFT_MOUSE_BUTTON;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags_left_button);

  frame_view->set_hit_test_result(HTCAPTION);
  GenerateAndDispatchClickMouseEvent(gfx::Point(), ui::EF_RIGHT_MOUSE_BUTTON);
  EXPECT_FALSE(host_->called_maximize());

  flags_left_button |= ui::EF_IS_DOUBLE_CLICK;
  GenerateAndDispatchClickMouseEvent(gfx::Point(), flags_left_button);
  EXPECT_FALSE(host_->called_maximize());

  widget->CloseNow();
}

}  // namespace views
