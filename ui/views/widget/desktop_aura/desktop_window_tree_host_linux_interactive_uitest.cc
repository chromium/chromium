// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/hit_test.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"

#if defined(USE_X11)
#include "ui/aura/env.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/x/test/x11_property_change_waiter.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/xproto.h"
#include "ui/views/controls/textfield/textfield.h"
#endif  // defined(USE_X11)

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

#if defined(USE_X11)
// Creates a widget with the given bounds.
std::unique_ptr<Widget> CreateWidget(const gfx::Rect& bounds) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = bounds;
  widget->Init(std::move(params));
  return widget;
}

// Dispatches a XMotionEvent targeted at |host|'s X window with location
// |point_in_screen|.
void DispatchMouseMotionEvent(DesktopWindowTreeHostLinux* desktop_host,
                              const gfx::Point& point_in_screen) {
  gfx::Rect bounds_in_screen = desktop_host->window()->GetBoundsInScreen();

  auto* connection = x11::Connection::Get();
  x11::MotionNotifyEvent xev{
      .detail = x11::Motion::Normal,
      .root = connection->default_root(),
      .event = static_cast<x11::Window>(desktop_host->GetAcceleratedWidget()),
      .root_x = point_in_screen.x(),
      .root_y = point_in_screen.y(),
      .event_x = point_in_screen.x() - bounds_in_screen.x(),
      .event_y = point_in_screen.y() - bounds_in_screen.y(),
      .same_screen = true,
  };

  x11::Event x11_event(xev);
  ui::X11EventSource::GetInstance()->ProcessXEvent(&x11_event);
}

// Blocks till |window| gets activated.
class ActivationWaiter : public ui::X11PropertyChangeWaiter {
 public:
  explicit ActivationWaiter(x11::Window window)
      : ui::X11PropertyChangeWaiter(ui::GetX11RootWindow(),
                                    "_NET_ACTIVE_WINDOW"),
        window_(window) {}

  ~ActivationWaiter() override = default;

  ActivationWaiter(const ActivationWaiter&) = delete;
  ActivationWaiter& operator=(ActivationWaiter&) = delete;

 private:
  // ui::X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(x11::Event* event) override {
    x11::Window window = x11::Window::None;
    ui::GetProperty(ui::GetX11RootWindow(), gfx::GetAtom("_NET_ACTIVE_WINDOW"),
                    &window);
    return window != window_;
  }

  x11::Window window_;
};
#endif  // defined(USE_X11)

// An event handler which counts the number of mouse moves it has seen.
class MouseMoveCounterHandler : public ui::EventHandler {
 public:
  MouseMoveCounterHandler() = default;
  ~MouseMoveCounterHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_MOVED)
      ++count_;
  }

  int num_mouse_moves() const { return count_; }

 private:
  int count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MouseMoveCounterHandler);
};

// A fake handler, which just stores the hittest and pointer location values.
class FakeWmMoveResizeHandler : public ui::WmMoveResizeHandler {
 public:
  using SetBoundsCallback = base::RepeatingCallback<void(gfx::Rect)>;
  explicit FakeWmMoveResizeHandler(ui::PlatformWindow* window)
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
  ui::PlatformWindow* platform_window_;
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
class HitTestWidgetDelegate : public WidgetDelegate {
 public:
  HitTestWidgetDelegate() {
    SetCanResize(true);
    SetOwnedByWidget(true);
  }
  ~HitTestWidgetDelegate() override = default;

  HitTestNonClientFrameView* frame_view() { return frame_view_; }

  // WidgetDelegate:
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView(
      Widget* widget) override {
    DCHECK(!frame_view_);
    auto frame_view = std::make_unique<HitTestNonClientFrameView>(widget);
    frame_view_ = frame_view.get();
    return frame_view;
  }

 private:
  HitTestNonClientFrameView* frame_view_ = nullptr;

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

  // PlatformWindowDelegate:
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

class DesktopWindowTreeHostLinuxTest
    : public test::DesktopWidgetTestInteractive {
 public:
  DesktopWindowTreeHostLinuxTest() = default;
  ~DesktopWindowTreeHostLinuxTest() override = default;

 protected:
  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds) {
    Widget* toplevel = new Widget;
    delegate_ = new HitTestWidgetDelegate();
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    auto* native_widget = new DesktopNativeWidgetAura(toplevel);
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

#if defined(USE_X11)
// Test that calling Widget::Deactivate() sets the widget as inactive wrt to
// Chrome even if it not possible to deactivate the window wrt to the x server.
// This behavior is required by several interactive_ui_tests.
TEST_F(DesktopWindowTreeHostLinuxTest, Deactivate) {
  // TODO(1109112): enable this test.
  if (features::IsUsingOzonePlatform())
    GTEST_SKIP();
  std::unique_ptr<Widget> widget(CreateWidget(gfx::Rect(100, 100, 100, 100)));

  ActivationWaiter waiter(static_cast<x11::Window>(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget()));
  widget->Show();
  widget->Activate();
  waiter.Wait();

  widget->Deactivate();
  // Regardless of whether |widget|'s X11 window eventually gets deactivated,
  // |widget|'s "active" state should change.
  EXPECT_FALSE(widget->IsActive());

  // |widget|'s X11 window should still be active. Reactivating |widget| should
  // update the widget's "active" state.
  // Note: Activating a widget whose X11 window is not active does not
  // synchronously update the widget's "active" state.
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
}

// Chrome attempts to make mouse capture look synchronous on Linux. Test that
// Chrome synchronously switches the window that mouse events are forwarded to
// when capture is changed.
TEST_F(DesktopWindowTreeHostLinuxTest, CaptureEventForwarding) {
  // TODO(1109112): enable this test.
  if (features::IsUsingOzonePlatform())
    GTEST_SKIP();
  std::unique_ptr<Widget> widget1(CreateWidget(gfx::Rect(100, 100, 100, 100)));
  aura::Window* window1 = widget1->GetNativeWindow();
  DesktopWindowTreeHostLinux* host1 =
      static_cast<DesktopWindowTreeHostLinux*>(window1->GetHost());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget(gfx::Rect(200, 100, 100, 100)));
  aura::Window* window2 = widget2->GetNativeWindow();
  DesktopWindowTreeHostLinux* host2 =
      static_cast<DesktopWindowTreeHostLinux*>(window2->GetHost());
  widget2->Show();

  MouseMoveCounterHandler recorder1;
  window1->AddPreTargetHandler(&recorder1);
  MouseMoveCounterHandler recorder2;
  window2->AddPreTargetHandler(&recorder2);

  // Move the mouse to the center of |widget2|.
  gfx::Point point_in_screen = widget2->GetWindowBoundsInScreen().CenterPoint();
  DispatchMouseMotionEvent(host2, point_in_screen);
  EXPECT_EQ(0, recorder1.num_mouse_moves());
  EXPECT_EQ(1, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Set capture to |widget1|. Because DesktopWindowTreeHostX11 calls
  // XGrabPointer() with owner == False, the X server sends events to |widget2|
  // as long as the mouse is hovered over |widget2|. Verify that Chrome
  // redirects mouse events to |widget1|.
  widget1->SetCapture(nullptr);
  point_in_screen += gfx::Vector2d(1, 0);
  DispatchMouseMotionEvent(host2, point_in_screen);
  EXPECT_EQ(1, recorder1.num_mouse_moves());
  EXPECT_EQ(1, recorder2.num_mouse_moves());
  // If the event's location was correctly changed to be relative to |widget1|,
  // Env's last mouse position will be correct.
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Set capture to |widget2|. Subsequent events sent to |widget2| should not be
  // forwarded.
  widget2->SetCapture(nullptr);
  point_in_screen += gfx::Vector2d(1, 0);
  DispatchMouseMotionEvent(host2, point_in_screen);
  EXPECT_EQ(1, recorder1.num_mouse_moves());
  EXPECT_EQ(2, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // If the mouse is not hovered over |widget1| or |widget2|, the X server will
  // send events to the window which has capture. Test the mouse events sent to
  // |widget2| are not forwarded.
  DispatchMouseMotionEvent(host2, point_in_screen);
  EXPECT_EQ(1, recorder1.num_mouse_moves());
  EXPECT_EQ(3, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Release capture. Test that when capture is released, mouse events are no
  // longer forwarded to other widgets.
  widget2->ReleaseCapture();
  point_in_screen = widget1->GetWindowBoundsInScreen().CenterPoint();
  DispatchMouseMotionEvent(host1, point_in_screen);
  EXPECT_EQ(2, recorder1.num_mouse_moves());
  EXPECT_EQ(3, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Cleanup
  window1->RemovePreTargetHandler(&recorder1);
  window2->RemovePreTargetHandler(&recorder2);
}

TEST_F(DesktopWindowTreeHostLinuxTest, InputMethodFocus) {
  // TODO(1109112): enable this test.
  if (features::IsUsingOzonePlatform())
    GTEST_SKIP();
  std::unique_ptr<Widget> widget(CreateWidget(gfx::Rect(100, 100, 100, 100)));

  // Waiter should be created as early as possible so that PropertyNotify has
  // time to be set before widget is activated.
  ActivationWaiter waiter(static_cast<x11::Window>(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget()));

  std::unique_ptr<Textfield> textfield(new Textfield);
  textfield->SetBounds(0, 0, 200, 20);
  widget->GetRootView()->AddChildView(textfield.get());
  widget->ShowInactive();
  textfield->RequestFocus();

  EXPECT_FALSE(widget->IsActive());
  // TODO(shuchen): uncomment the below check once the
  // "default-focused-input-method" logic is removed in aura::WindowTreeHost.
  // EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
  //           widget->GetInputMethod()->GetTextInputType());

  widget->Activate();
  waiter.Wait();

  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  widget->Deactivate();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
}

#endif  // defined(USE_X11)

}  // namespace views
