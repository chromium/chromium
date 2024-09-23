// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/test/ui_controls.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "ui/views/widget/desktop_aura/window_event_filter_linux.h"
using DesktopWindowTreeHostPlatformImpl = views::DesktopWindowTreeHostLinux;
#else
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/views/widget/desktop_aura/window_event_filter_lacros.h"
using DesktopWindowTreeHostPlatformImpl = views::DesktopWindowTreeHostLacros;
#endif

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
}

std::unique_ptr<Widget> CreateWidget(const gfx::Rect& bounds) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = bounds;
  widget->Init(std::move(params));
  return widget;
}

// Dispatches a motion event targeted to |point_in_screen|.
void DispatchMouseMotionEventSync(const gfx::Point& point_in_screen) {
  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(
      point_in_screen.x(), point_in_screen.y(), run_loop.QuitClosure());
  run_loop.Run();
}

// An event handler which counts the number of mouse moves it has seen.
class MouseMoveCounterHandler : public ui::EventHandler {
 public:
  MouseMoveCounterHandler() = default;

  MouseMoveCounterHandler(const MouseMoveCounterHandler&) = delete;
  MouseMoveCounterHandler& operator=(const MouseMoveCounterHandler&) = delete;

  ~MouseMoveCounterHandler() override = default;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    // ui_controls::SendMouseMoveNotifyWhenDone calls
    // aura::Window::MoveCursorTo, which internally results in calling both
    // aura::WindowEventDispatcher::PostSynthesizeMouseMove and
    // aura::WindowTreeHostPlatform::MoveCursorToScreenLocationInPixels. Thus,
    // two events will come - one is synthetic and another one is our real one.
    // Ignore the synthetic events as we are not interested in them.
    if (event->type() == ui::EventType::kMouseMoved &&
        !event->IsSynthesized()) {
      ++count_;
    }
  }

  int num_mouse_moves() const { return count_; }

 private:
  int count_ = 0;
};

// A fake handler, which just stores the hittest and pointer location values.
class FakeWmMoveResizeHandler : public ui::WmMoveResizeHandler {
 public:
  using SetBoundsCallback = base::RepeatingCallback<void(gfx::Rect)>;
  explicit FakeWmMoveResizeHandler(ui::PlatformWindow* window)
      : platform_window_(window) {}

  FakeWmMoveResizeHandler(const FakeWmMoveResizeHandler&) = delete;
  FakeWmMoveResizeHandler& operator=(const FakeWmMoveResizeHandler&) = delete;

  ~FakeWmMoveResizeHandler() override = default;

  void Reset() {
    hittest_ = -1;
    pointer_location_in_px_ = gfx::Point();
  }

  int hittest() const { return hittest_; }
  gfx::Point pointer_location_in_px() const { return pointer_location_in_px_; }

  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

  void set_platform_window(ui::PlatformWindow* platform_window) {
    platform_window_ = platform_window;
  }

  // ui::WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override {
    hittest_ = hittest;
    pointer_location_in_px_ = pointer_location_in_px;

    platform_window_->SetBoundsInPixels(bounds_);
  }

 private:
  raw_ptr<ui::PlatformWindow> platform_window_;
  gfx::Rect bounds_;

  int hittest_ = -1;
  gfx::Point pointer_location_in_px_;
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

  HitTestNonClientFrameView(const HitTestNonClientFrameView&) = delete;
  HitTestNonClientFrameView& operator=(const HitTestNonClientFrameView&) =
      delete;

  ~HitTestNonClientFrameView() override = default;

  void set_hit_test_result(int component) { hit_test_result_ = component; }

  // NonClientFrameView overrides:
  int NonClientHitTest(const gfx::Point& point) override {
    return hit_test_result_;
  }

 private:
  int hit_test_result_ = HTNOWHERE;
};

// This is used to return HitTestNonClientFrameView on create call.
class HitTestWidgetDelegate : public WidgetDelegate {
 public:
  HitTestWidgetDelegate() { SetCanResize(true); }

  HitTestWidgetDelegate(const HitTestWidgetDelegate&) = delete;
  HitTestWidgetDelegate& operator=(const HitTestWidgetDelegate&) = delete;

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
  raw_ptr<HitTestNonClientFrameView, DanglingUntriaged> frame_view_ = nullptr;
};

// Test host that can intercept calls to the real host.
class TestDesktopWindowTreeHostPlatformImpl
    : public DesktopWindowTreeHostPlatformImpl {
 public:
  TestDesktopWindowTreeHostPlatformImpl(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura)
      : DesktopWindowTreeHostPlatformImpl(native_widget_delegate,
                                          desktop_native_widget_aura) {}

  TestDesktopWindowTreeHostPlatformImpl(
      const TestDesktopWindowTreeHostPlatformImpl&) = delete;
  TestDesktopWindowTreeHostPlatformImpl& operator=(
      const TestDesktopWindowTreeHostPlatformImpl&) = delete;

  ~TestDesktopWindowTreeHostPlatformImpl() override = default;

  // PlatformWindowDelegate:
  // Instead of making these tests friends of the host, override the dispatch
  // method to make it public and nothing else.
  void DispatchEvent(ui::Event* event) override {
    DesktopWindowTreeHostPlatformImpl::DispatchEvent(event);
  }

  void ResetCalledMaximize() { called_maximize_ = false; }
  bool called_maximize() const { return called_maximize_; }
  // DesktopWindowTreeHost
  void Maximize() override {
    called_maximize_ = true;
    DesktopWindowTreeHostPlatformImpl::Maximize();
  }

 private:
  bool called_maximize_ = false;
};

}  // namespace

class DesktopWindowTreeHostPlatformImplTest
    : public test::DesktopWidgetTestInteractive,
      public views::WidgetObserver {
 public:
  DesktopWindowTreeHostPlatformImplTest() = default;

  DesktopWindowTreeHostPlatformImplTest(
      const DesktopWindowTreeHostPlatformImplTest&) = delete;
  DesktopWindowTreeHostPlatformImplTest& operator=(
      const DesktopWindowTreeHostPlatformImplTest&) = delete;

  ~DesktopWindowTreeHostPlatformImplTest() override = default;

 protected:
  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds) {
    Widget* toplevel = new Widget;
    delegate_ = std::make_unique<HitTestWidgetDelegate>();
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    auto* native_widget = new DesktopNativeWidgetAura(toplevel);
    toplevel_params.native_widget = native_widget;
    host_ = new TestDesktopWindowTreeHostPlatformImpl(toplevel, native_widget);
    toplevel_params.desktop_window_tree_host = host_.get();
    toplevel_params.delegate = delegate_.get();
    toplevel_params.bounds = bounds;
    toplevel_params.remove_standard_frame = true;
    toplevel->Init(std::move(toplevel_params));
    widget_observation_.Observe(toplevel);
    return toplevel;
  }

  void DispatchEvent(ui::Event* event) {
    DCHECK(host_);
    std::unique_ptr<ui::Event> owned_event(event);
    host_->DispatchEvent(owned_event.get());
  }

  void GenerateAndDispatchClickMouseEvent(const gfx::Point& click_location,
                                          int flags) {
    DCHECK(host_);
    DispatchEvent(GenerateMouseEvent(ui::EventType::kMousePressed,
                                     click_location, flags));
    DispatchEvent(GenerateMouseEvent(ui::EventType::kMouseReleased,
                                     click_location, flags));
  }

  ui::MouseEvent* GenerateMouseEvent(ui::EventType event_type,
                                     const gfx::Point& click_location,
                                     int flags) {
    int flag = 0;
    if (flags & ui::EF_LEFT_MOUSE_BUTTON) {
      flag = ui::EF_LEFT_MOUSE_BUTTON;
    } else {
      CHECK(flags & ui::EF_RIGHT_MOUSE_BUTTON)
          << "Other mouse clicks are not supported yet. Add the new one.";
      flag = ui::EF_RIGHT_MOUSE_BUTTON;
    }
    return new ui::MouseEvent(event_type, click_location, click_location,
                              base::TimeTicks::Now(), flags, flag);
  }

  ui::GestureEvent* GenerateGestureEvent(
      const gfx::Point& gesture_location,
      const ui::GestureEventDetails& gesture_details) {
    return new ui::GestureEvent(gesture_location.x(), gesture_location.y(), 0,
                                base::TimeTicks::Now(), gesture_details);
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override {
    CHECK(widget_observation_.IsObservingSource(widget));
    widget_observation_.Reset();
    host_ = nullptr;
  }

  std::unique_ptr<HitTestWidgetDelegate> delegate_ = nullptr;
  raw_ptr<TestDesktopWindowTreeHostPlatformImpl> host_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

// These tests are run using either click or touch events.
class DesktopWindowTreeHostPlatformImplTestWithTouch
    : public DesktopWindowTreeHostPlatformImplTest,
      public testing::WithParamInterface<bool> {
 public:
  bool use_touch_event() const { return GetParam(); }
};

// On Lacros, the resize and drag operations are handled by compositor,
// so this test does not make much sense.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_HitTest DISABLED_HitTest
#else
#define MAYBE_HitTest HitTest
#endif
TEST_P(DesktopWindowTreeHostPlatformImplTestWithTouch, MAYBE_HitTest) {
  gfx::Rect widget_bounds(0, 0, 100, 100);
  std::unique_ptr<Widget> widget(BuildTopLevelDesktopWidget(widget_bounds));
  widget->Show();

  // Install a fake move/resize handler to intercept the move/resize call.
  auto handler =
      std::make_unique<FakeWmMoveResizeHandler>(host_->platform_window());
  host_->DestroyNonClientEventFilter();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  host_->non_client_window_event_filter_ =
      std::make_unique<WindowEventFilterLacros>(host_, handler.get());
#else
  host_->non_client_window_event_filter_ =
      std::make_unique<WindowEventFilterLinux>(host_, handler.get());
#endif

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

    // The wm move/resize handler receives pointer location in the global
    // screen coordinate, whereas event dispatcher receives event locations on
    // a local system coordinate. Thus, add an offset of a new possible origin
    // value of a window to the expected pointer location.
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

    // Send mouse/touch down event and make sure the WindowEventFilter calls
    // the move/resize handler to start interactive move/resize with the
    // |hittest| value we specified.

    if (use_touch_event()) {
      ui::GestureEventDetails gesture_details(
          ui::EventType::kGestureScrollBegin);
      DispatchEvent(
          GenerateGestureEvent(pointer_location_in_px, gesture_details));
    } else {
      DispatchEvent(GenerateMouseEvent(ui::EventType::kMousePressed,
                                       pointer_location_in_px,
                                       ui::EF_LEFT_MOUSE_BUTTON));
    }

    // The test expectation is based on the hit test component. If it is a
    // non-client component, which results in a call to move/resize, the
    // handler must receive the hittest value and the pointer location in
    // global screen coordinate system. In other cases, it must not.
    SetExpectationBasedOnHittestValue(hittest, *handler.get(),
                                      expected_pointer_location_in_px);
    // Make sure the bounds of the content window are correct.
    EXPECT_EQ(window->GetBoundsInScreen().ToString(), bounds.ToString());

    // Dispatch mouse/touch release event to release a mouse/touch pressed
    // handler and be able to consume future events.
    if (use_touch_event()) {
      ui::GestureEventDetails gesture_details(ui::EventType::kGestureScrollEnd);
      DispatchEvent(
          GenerateGestureEvent(pointer_location_in_px, gesture_details));
    } else {
      DispatchEvent(GenerateMouseEvent(ui::EventType::kMouseReleased,
                                       pointer_location_in_px,
                                       ui::EF_LEFT_MOUSE_BUTTON));
    }
  }
  handler->set_platform_window(nullptr);
  widget->CloseNow();
}

// Tests that the window is maximized in response to a double click event.
TEST_P(DesktopWindowTreeHostPlatformImplTestWithTouch,
       DoubleClickHeaderMaximizes) {
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

  if (use_touch_event()) {
    ui::GestureEventDetails details(ui::EventType::kGestureTap);
    details.set_tap_count(1);
    DispatchEvent(GenerateGestureEvent(gfx::Point(), details));
    details.set_tap_count(2);
    DispatchEvent(GenerateGestureEvent(gfx::Point(), details));
  } else {
    int flags = ui::EF_LEFT_MOUSE_BUTTON;
    GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);
    flags |= ui::EF_IS_DOUBLE_CLICK;
    GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);
  }

  EXPECT_TRUE(host_->called_maximize());

  widget->CloseNow();
}

// Tests that the window does not maximize in response to a double click event,
// if the first click was to a different target component than that of the
// second click.
TEST_P(DesktopWindowTreeHostPlatformImplTestWithTouch,
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

  if (use_touch_event()) {
    frame_view->set_hit_test_result(HTCLIENT);
    ui::GestureEventDetails details(ui::EventType::kGestureTap);
    details.set_tap_count(1);
    DispatchEvent(GenerateGestureEvent(gfx::Point(), details));

    frame_view->set_hit_test_result(HTCLIENT);
    details.set_tap_count(2);
    DispatchEvent(GenerateGestureEvent(gfx::Point(), details));
  } else {
    frame_view->set_hit_test_result(HTCLIENT);
    int flags = ui::EF_LEFT_MOUSE_BUTTON;
    GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);

    frame_view->set_hit_test_result(HTCLIENT);
    flags |= ui::EF_IS_DOUBLE_CLICK;
    GenerateAndDispatchClickMouseEvent(gfx::Point(), flags);
  }

  EXPECT_FALSE(host_->called_maximize());

  widget->CloseNow();
}

// Tests that the window does not maximize in response to a double click event,
// if the double click was interrupted by a right click.
TEST_F(DesktopWindowTreeHostPlatformImplTest,
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

// Test that calling Widget::Deactivate() sets the widget as inactive wrt to
// Chrome even if it not possible to deactivate the window wrt to the x server.
// This behavior is required by several interactive_ui_tests.
TEST_F(DesktopWindowTreeHostPlatformImplTest, Deactivate) {
  std::unique_ptr<Widget> widget(CreateWidget(gfx::Rect(100, 100, 100, 100)));

  {
    widget->Show();
    widget->Activate();
    views::test::WaitForWidgetActive(widget.get(), true);
  }

  {
    // Regardless of whether |widget|'s X11 window eventually gets deactivated,
    // |widget|'s "active" state should change.
    widget->Deactivate();
    views::test::WaitForWidgetActive(widget.get(), false);
    EXPECT_FALSE(widget->IsActive());
  }

  {
    // |widget|'s X11 window should still be active. Reactivating |widget|
    // should update the widget's "active" state. Note: Activating a widget
    // whose X11 window is not active does not synchronously update the widget's
    // "active" state.
    widget->Activate();
    views::test::WaitForWidgetActive(widget.get(), true);
    EXPECT_TRUE(widget->IsActive());
  }
}

// Chrome attempts to make mouse capture look synchronous on Linux. Test that
// Chrome synchronously switches the window that mouse events are forwarded to
// when capture is changed.
TEST_F(DesktopWindowTreeHostPlatformImplTest, CaptureEventForwarding) {
  ui_controls::EnableUIControls();

  std::unique_ptr<Widget> widget1(CreateWidget(gfx::Rect(100, 100, 100, 100)));
  aura::Window* window1 = widget1->GetNativeWindow();
  widget1->Show();
  views::test::WaitForWidgetActive(widget1.get(), true);

  std::unique_ptr<Widget> widget2(CreateWidget(gfx::Rect(200, 100, 100, 100)));
  aura::Window* window2 = widget2->GetNativeWindow();
  widget2->Show();
  views::test::WaitForWidgetActive(widget2.get(), true);

  MouseMoveCounterHandler recorder1;
  window1->AddPreTargetHandler(&recorder1);
  MouseMoveCounterHandler recorder2;
  window2->AddPreTargetHandler(&recorder2);

  // Move the mouse to the center of |widget2|.
  gfx::Point point_in_screen = widget2->GetWindowBoundsInScreen().CenterPoint();
  DispatchMouseMotionEventSync(point_in_screen);
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
  DispatchMouseMotionEventSync(point_in_screen);
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
  DispatchMouseMotionEventSync(point_in_screen);
  EXPECT_EQ(1, recorder1.num_mouse_moves());
  EXPECT_EQ(2, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // If the mouse is not hovered over |widget1| or |widget2|, the X server will
  // send events to the window which has capture. Test the mouse events sent to
  // |widget2| are not forwarded.
  DispatchMouseMotionEventSync(point_in_screen);
  EXPECT_EQ(1, recorder1.num_mouse_moves());
  EXPECT_EQ(3, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Release capture. Test that when capture is released, mouse events are no
  // longer forwarded to other widgets.
  widget2->ReleaseCapture();
  point_in_screen = widget1->GetWindowBoundsInScreen().CenterPoint();
  DispatchMouseMotionEventSync(point_in_screen);
  EXPECT_EQ(2, recorder1.num_mouse_moves());
  EXPECT_EQ(3, recorder2.num_mouse_moves());
  EXPECT_EQ(point_in_screen.ToString(),
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Cleanup
  window1->RemovePreTargetHandler(&recorder1);
  window2->RemovePreTargetHandler(&recorder2);
}

TEST_F(DesktopWindowTreeHostPlatformImplTest, InputMethodFocus) {
  std::unique_ptr<Widget> widget(CreateWidget(gfx::Rect(100, 100, 100, 100)));

  std::unique_ptr<Textfield> textfield(new Textfield);
  // Provide an accessible name so that accessibility paint checks pass.
  textfield->GetViewAccessibility().SetName(u"test");
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
  views::test::WaitForWidgetActive(widget.get(), true);

  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  widget->Deactivate();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
}

INSTANTIATE_TEST_SUITE_P(,
                         DesktopWindowTreeHostPlatformImplTestWithTouch,
                         testing::Bool());

}  // namespace views
