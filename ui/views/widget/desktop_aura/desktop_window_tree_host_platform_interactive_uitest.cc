// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/hit_test.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/views/test/views_interactive_ui_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter.h"
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
  explicit FakeWmMoveResizeHandler(ui::PlatformWindow* window)
      : platform_window_(window), hittest_(-1) {}
  ~FakeWmMoveResizeHandler() override = default;

  void Reset() {
    hittest_ = -1;
    pointer_location_ = gfx::Point();
  }

  int hittest() const { return hittest_; }
  gfx::Point pointer_location() const { return pointer_location_; }

  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  // ui::WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location) override {
    hittest_ = hittest;
    pointer_location_ = pointer_location;

    platform_window_->SetBounds(bounds_);
  }

 private:
  ui::PlatformWindow* platform_window_;
  gfx::Rect bounds_;

  int hittest_ = -1;
  gfx::Point pointer_location_;

  DISALLOW_COPY_AND_ASSIGN(FakeWmMoveResizeHandler);
};

void SetExpectationBasedOnHittestValue(int hittest,
                                       const FakeWmMoveResizeHandler& handler,
                                       const gfx::Point& pointer_location) {
  if (IsNonClientComponent(hittest)) {
    // Ensure both the pointer location and the hit test value are passed to the
    // fake move/resize handler.
    EXPECT_EQ(handler.pointer_location().ToString(),
              pointer_location.ToString());
    EXPECT_EQ(handler.hittest(), hittest);
    return;
  }

  // Ensure the handler does not receive the hittest value or the pointer
  // location.
  EXPECT_TRUE(handler.pointer_location().IsOrigin());
  EXPECT_NE(handler.hittest(), hittest);
}

// This is used to return a customized result to NonClientHitTest.
class HitTestNonClientFrameView : public NativeFrameView {
 public:
  explicit HitTestNonClientFrameView(Widget* widget)
      : NativeFrameView(widget), hit_test_result_(HTNOWHERE) {}
  ~HitTestNonClientFrameView() override {}

  void set_hit_test_result(int component) { hit_test_result_ = component; }

  // NonClientFrameView overrides:
  int NonClientHitTest(const gfx::Point& point) override {
    return hit_test_result_;
  }

 private:
  int hit_test_result_;

  DISALLOW_COPY_AND_ASSIGN(HitTestNonClientFrameView);
};

// This is used to return HitTestNonClientFrameView on create call.
class HitTestWidgetDelegate : public views::WidgetDelegate {
 public:
  HitTestWidgetDelegate(views::Widget* widget,
                        HitTestNonClientFrameView* frame_view)
      : widget_(widget), frame_view_(frame_view) {}
  ~HitTestWidgetDelegate() override {}

  void set_can_resize(bool can_resize) {
    can_resize_ = can_resize;
    widget_->OnSizeConstraintsChanged();
  }

  // views::WidgetDelegate:
  bool CanResize() const override { return can_resize_; }
  views::Widget* GetWidget() override { return widget_; }
  views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(Widget* widget) override {
    return frame_view_;
  }

 private:
  views::Widget* widget_;
  HitTestNonClientFrameView* frame_view_;
  bool can_resize_ = false;

  DISALLOW_COPY_AND_ASSIGN(HitTestWidgetDelegate);
};

}  // namespace

class DesktopWindowTreeHostPlatformTest : public ViewsInteractiveUITestBase {
 public:
  DesktopWindowTreeHostPlatformTest() = default;
  ~DesktopWindowTreeHostPlatformTest() override = default;

 protected:
  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds) {
    Widget* toplevel = new Widget;
    frame_view_ = new HitTestNonClientFrameView(toplevel);
    delegate_ = new HitTestWidgetDelegate(toplevel, frame_view_);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.native_widget =
        new views::DesktopNativeWidgetAura(toplevel);
    toplevel_params.delegate = delegate_;
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_params.bounds = bounds;
    toplevel_params.remove_standard_frame = true;
    toplevel->Init(toplevel_params);
    return toplevel;
  }

  std::unique_ptr<ui::MouseEvent> CreateMouseEvent(
      const gfx::Point& pointer_location,
      ui::EventType event_type,
      int flags) {
    std::unique_ptr<ui::MouseEvent> mouse_event =
        std::make_unique<ui::MouseEvent>(event_type, pointer_location,
                                         pointer_location,
                                         base::TimeTicks::Now(), flags, flags);
    return mouse_event;
  }

  HitTestNonClientFrameView* frame_view_ = nullptr;
  HitTestWidgetDelegate* delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostPlatformTest);
};

TEST_F(DesktopWindowTreeHostPlatformTest, HitTest) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(bounds));
  widget->Show();

  aura::Window* window = widget->GetNativeWindow();
  DesktopWindowTreeHostPlatform* host =
      static_cast<DesktopWindowTreeHostPlatform*>(window->GetHost());

  // Install a fake move/resize handler to intercept the move/resize call.
  WindowEventFilter* non_client_filter =
      host->non_client_window_event_filter_.get();
  std::unique_ptr<FakeWmMoveResizeHandler> handler =
      std::make_unique<FakeWmMoveResizeHandler>(host->platform_window());
  non_client_filter->SetWmMoveResizeHandler(handler.get());

  delegate_->set_can_resize(true);

  // It is not important to use pointer locations corresponding to the hittests
  // values used in the browser itself, because we fake the hit test results,
  // which non client frame view sends back. Thus, just make sure the content
  // window is able to receive these events.
  gfx::Point pointer_location(10, 10);

  constexpr int hittest_values[] = {
      HTBOTTOM, HTBOTTOMLEFT, HTBOTTOMRIGHT, HTCAPTION,     HTLEFT,
      HTRIGHT,  HTTOP,        HTTOPLEFT,     HTTOPRIGHT,    HTNOWHERE,
      HTBORDER, HTCLIENT,     HTCLOSE,       HTERROR,       HTGROWBOX,
      HTHELP,   HTHSCROLL,    HTMENU,        HTMAXBUTTON,   HTMINBUTTON,
      HTREDUCE, HTSIZE,       HTSYSMENU,     HTTRANSPARENT, HTVSCROLL,
      HTZOOM,
  };

  for (int hittest : hittest_values) {
    handler->Reset();

    // Set the desired hit test result value, which will be returned, when
    // WindowEventFilter starts to perform hit testing.
    frame_view_->set_hit_test_result(hittest);

    gfx::Rect bounds = window->GetBoundsInScreen();

    // The wm move/resize handler receives pointer location in the global screen
    // coordinate, whereas event dispatcher receives event locations on a local
    // system coordinate. Thus, add an offset of a new possible origin value of
    // a window to the expected pointer location.
    gfx::Point expected_pointer_location(pointer_location);
    expected_pointer_location.Offset(bounds.x(), bounds.y());

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
    auto mouse_down_event = CreateMouseEvent(
        pointer_location, ui::ET_MOUSE_PRESSED, ui::EF_LEFT_MOUSE_BUTTON);
    host->DispatchEvent(mouse_down_event.get());

    // The test expectation is based on the hit test component. If it is a
    // non-client component, which results in a call to move/resize, the handler
    // must receive the hittest value and the pointer location in global screen
    // coordinate system. In other cases, it must not.
    SetExpectationBasedOnHittestValue(hittest, *handler.get(),
                                      expected_pointer_location);
    // Make sure the bounds of the content window are correct.
    EXPECT_EQ(window->GetBoundsInScreen().ToString(), bounds.ToString());

    // Dispatch mouse up event to release mouse pressed handler and be able to
    // consume future events.
    auto mouse_up_event = CreateMouseEvent(
        pointer_location, ui::ET_MOUSE_RELEASED, ui::EF_LEFT_MOUSE_BUTTON);
    host->DispatchEvent(mouse_up_event.get());
  }
}

}  // namespace views
