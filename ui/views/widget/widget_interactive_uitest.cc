// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/views_interactive_ui_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/public/activation_client.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {
namespace test {

namespace {

// A View that closes the Widget and exits the current message-loop when it
// receives a mouse-release event.
class ExitLoopOnRelease : public View {
 public:
  explicit ExitLoopOnRelease(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    DCHECK(quit_closure_);
  }

  ~ExitLoopOnRelease() override = default;

 private:
  // View:
  void OnMouseReleased(const ui::MouseEvent& event) override {
    GetWidget()->Close();
    std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ExitLoopOnRelease);
};

// A view that does a capture on ui::ET_GESTURE_TAP_DOWN events.
class GestureCaptureView : public View {
 public:
  GestureCaptureView() = default;
  ~GestureCaptureView() override = default;

 private:
  // View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      GetWidget()->SetCapture(this);
      event->StopPropagation();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(GestureCaptureView);
};

// A view that always processes all mouse events.
class MouseView : public View {
 public:
  MouseView() = default;
  ~MouseView() override = default;

  bool OnMousePressed(const ui::MouseEvent& event) override {
    pressed_++;
    return true;
  }

  void OnMouseEntered(const ui::MouseEvent& event) override { entered_++; }

  void OnMouseExited(const ui::MouseEvent& event) override { exited_++; }

  // Return the number of OnMouseEntered calls and reset the counter.
  int EnteredCalls() {
    int i = entered_;
    entered_ = 0;
    return i;
  }

  // Return the number of OnMouseExited calls and reset the counter.
  int ExitedCalls() {
    int i = exited_;
    exited_ = 0;
    return i;
  }

  int pressed() const { return pressed_; }

 private:
  int entered_ = 0;
  int exited_ = 0;

  int pressed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MouseView);
};

// A View that shows a different widget, sets capture on that widget, and
// initiates a nested message-loop when it receives a mouse-press event.
class NestedLoopCaptureView : public View {
 public:
  explicit NestedLoopCaptureView(Widget* widget)
      : run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
        widget_(widget) {}
  ~NestedLoopCaptureView() override = default;

  base::OnceClosure GetQuitClosure() { return run_loop_.QuitClosure(); }

 private:
  // View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Start a nested loop.
    widget_->Show();
    widget_->SetCapture(widget_->GetContentsView());
    EXPECT_TRUE(widget_->HasCapture());

    run_loop_.Run();
    return true;
  }

  base::RunLoop run_loop_;

  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(NestedLoopCaptureView);
};

ui::WindowShowState GetWidgetShowState(const Widget* widget) {
  // Use IsMaximized/IsMinimized/IsFullScreen instead of GetWindowPlacement
  // because the former is implemented on all platforms but the latter is not.
  return widget->IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN :
      widget->IsMaximized() ? ui::SHOW_STATE_MAXIMIZED :
      widget->IsMinimized() ? ui::SHOW_STATE_MINIMIZED :
      widget->IsActive() ? ui::SHOW_STATE_NORMAL :
                           ui::SHOW_STATE_INACTIVE;
}

// Give the OS an opportunity to process messages for an activation change, when
// there is actually no change expected (e.g. ShowInactive()).
void RunPendingMessagesForActiveStatusChange() {
#if defined(OS_MACOSX)
  // On Mac, a single spin is *usually* enough. It isn't when a widget is shown
  // and made active in two steps, so tests should follow up with a ShowSync()
  // or ActivateSync to ensure a consistent state.
  base::RunLoop().RunUntilIdle();
#endif
  // TODO(tapted): Check for desktop aura widgets.
}

// Activate a widget, and wait for it to become active. On non-desktop Aura
// this is just an activation. For other widgets, it means activating and then
// spinning the run loop until the OS has activated the window.
void ActivateSync(Widget* widget) {
  views::test::WidgetActivationWaiter waiter(widget, true);
  widget->Activate();
  waiter.Wait();
}

// Like for ActivateSync(), wait for a widget to become active, but Show() the
// widget rather than calling Activate().
void ShowSync(Widget* widget) {
  views::test::WidgetActivationWaiter waiter(widget, true);
  widget->Show();
  waiter.Wait();
}

void DeactivateSync(Widget* widget) {
#if defined(OS_MACOSX)
  // Deactivation of a window isn't a concept on Mac: If an application is
  // active and it has any activatable windows, then one of them is always
  // active. But we can simulate deactivation (e.g. as if another application
  // became active) by temporarily making |widget| non-activatable, then
  // activating (and closing) a temporary widget.
  widget->widget_delegate()->SetCanActivate(false);
  Widget* stealer = new Widget;
  stealer->Init(Widget::InitParams(Widget::InitParams::TYPE_WINDOW));
  ShowSync(stealer);
  stealer->CloseNow();
  widget->widget_delegate()->SetCanActivate(true);
#else
  views::test::WidgetActivationWaiter waiter(widget, false);
  widget->Deactivate();
  waiter.Wait();
#endif
}

#if defined(OS_WIN)
void ActivatePlatformWindow(Widget* widget) {
  ::SetActiveWindow(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
}
#endif

// Calls ShowInactive() on a Widget, and spins a run loop. The goal is to give
// the OS a chance to activate a widget. However, for this case, the test
// doesn't expect that to happen, so there is nothing to wait for.
void ShowInactiveSync(Widget* widget) {
  widget->ShowInactive();
  RunPendingMessagesForActiveStatusChange();
}

// Wait until |callback| returns |expected_value|, but no longer than 1 second.
class PropertyWaiter {
 public:
  PropertyWaiter(base::RepeatingCallback<bool(void)> callback,
                 bool expected_value)
      : callback_(std::move(callback)), expected_value_(expected_value) {}

  bool Wait() {
    if (callback_.Run() == expected_value_) {
      success_ = true;
      return success_;
    }
    start_time_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, base::TimeDelta(), this, &PropertyWaiter::Check);
    run_loop_.Run();
    return success_;
  }

 private:
  void Check() {
    DCHECK(!success_);
    success_ = callback_.Run() == expected_value_;
    if (success_ || base::TimeTicks::Now() - start_time_ > kTimeout) {
      timer_.Stop();
      run_loop_.Quit();
    }
  }

  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1);
  base::RepeatingCallback<bool(void)> callback_;
  const bool expected_value_;
  bool success_ = false;
  base::TimeTicks start_time_;
  base::RunLoop run_loop_;
  base::RepeatingTimer timer_;
};

}  // namespace

class WidgetTestInteractive : public WidgetTest {
 public:
  WidgetTestInteractive() = default;
  ~WidgetTestInteractive() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    WidgetTest::SetUp();
  }

 protected:
#if defined(USE_AURA)
  static void ShowQuickMenuImmediately(
      TouchSelectionControllerImpl* controller) {
    DCHECK(controller);
    if (controller->quick_menu_timer_.IsRunning()) {
      controller->quick_menu_timer_.Stop();
      controller->QuickMenuTimerFired();
    }
  }
#endif  // defined (USE_AURA)
};

class DesktopWidgetTestInteractive : public WidgetTestInteractive {
 public:
  DesktopWidgetTestInteractive() = default;
  ~DesktopWidgetTestInteractive() override = default;

  // WidgetTestInteractive:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    WidgetTestInteractive::SetUp();
  }

 protected:
  Widget* CreateWidget() {
    Widget* widget = CreateTopLevelNativeWidget();
    widget->SetBounds(gfx::Rect(0, 0, 200, 200));
    return widget;
  }

  DISALLOW_COPY_AND_ASSIGN(DesktopWidgetTestInteractive);
};

#if defined(OS_WIN)
// Tests whether activation and focus change works correctly in Windows.
// We test the following:-
// 1. If the active aura window is correctly set when a top level widget is
//    created.
// 2. If the active aura window in widget 1 created above, is set to NULL when
//    another top level widget is created and focused.
// 3. On focusing the native platform window for widget 1, the active aura
//    window for widget 1 should be set and that for widget 2 should reset.
// TODO(ananta): Discuss with erg on how to write this test for linux x11 aura.
TEST_F(DesktopWidgetTestInteractive,
       DesktopNativeWidgetAuraActivationAndFocusTest) {
  // Create widget 1 and expect the active window to be its window.
  View* focusable_view1 = new View;
  focusable_view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  Widget* widget1 = CreateWidget();
  widget1->GetContentsView()->AddChildView(focusable_view1);
  widget1->Show();
  aura::Window* root_window1 = GetRootWindow(widget1);
  focusable_view1->RequestFocus();

  EXPECT_TRUE(root_window1 != nullptr);
  wm::ActivationClient* activation_client1 =
      wm::GetActivationClient(root_window1);
  EXPECT_TRUE(activation_client1 != nullptr);
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());

  // Create widget 2 and expect the active window to be its window.
  View* focusable_view2 = new View;
  Widget* widget2 = CreateWidget();
  widget1->GetContentsView()->AddChildView(focusable_view2);
  widget2->Show();
  aura::Window* root_window2 = GetRootWindow(widget2);
  focusable_view2->RequestFocus();
  ActivatePlatformWindow(widget2);

  wm::ActivationClient* activation_client2 =
      wm::GetActivationClient(root_window2);
  EXPECT_TRUE(activation_client2 != nullptr);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2->GetNativeView());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));

  // Now set focus back to widget 1 and expect the active window to be its
  // window.
  focusable_view1->RequestFocus();
  ActivatePlatformWindow(widget1);
  EXPECT_EQ(activation_client2->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());

  widget2->CloseNow();
  widget1->CloseNow();
}

class TouchEventHandler : public ui::EventHandler {
 public:
  explicit TouchEventHandler(Widget* widget) : widget_(widget) {
    widget_->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(this);
  }

  ~TouchEventHandler() override {
    widget_->GetNativeWindow()->GetHost()->window()->RemovePreTargetHandler(
        this);
  }

  void WaitForEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  static void __stdcall AsyncActivateMouse(HWND hwnd,
                                           UINT msg,
                                           ULONG_PTR data,
                                           LRESULT result) {
    EXPECT_EQ(MA_NOACTIVATE, result);
    std::move(reinterpret_cast<TouchEventHandler*>(data)->quit_closure_).Run();
  }

  void ActivateViaMouse() {
    SendMessageCallback(
        widget_->GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
        WM_MOUSEACTIVATE, 0, 0, AsyncActivateMouse,
        reinterpret_cast<ULONG_PTR>(this));
  }

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::ET_TOUCH_PRESSED)
      ActivateViaMouse();
  }

  Widget* widget_;
  base::OnceClosure quit_closure_;
  DISALLOW_COPY_AND_ASSIGN(TouchEventHandler);
};

// TODO(dtapuska): Disabled due to it being flaky crbug.com/817531
TEST_F(DesktopWidgetTestInteractive, DISABLED_TouchNoActivateWindow) {
  // ui_controls::SendTouchEvents which uses InjectTouchInput API only works
  // on Windows 8 and up.
  if (base::win::GetVersion() <= base::win::Version::WIN7)
    return;

  View* focusable_view = new View;
  focusable_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  Widget* widget = CreateWidget();
  widget->GetContentsView()->AddChildView(focusable_view);
  widget->Show();

  {
    TouchEventHandler touch_event_handler(widget);
    ASSERT_TRUE(ui_controls::SendTouchEvents(ui_controls::PRESS, 1, 100, 100));
    touch_event_handler.WaitForEvents();
  }

  widget->CloseNow();
}

#endif  // defined(OS_WIN)

TEST_F(WidgetTestInteractive, CaptureAutoReset) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  EXPECT_FALSE(toplevel->HasCapture());
  toplevel->SetCapture(nullptr);
  EXPECT_TRUE(toplevel->HasCapture());

  // By default, mouse release removes capture.
  gfx::Point click_location(45, 15);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&release);
  EXPECT_FALSE(toplevel->HasCapture());

  // Now a mouse release shouldn't remove capture.
  toplevel->set_auto_release_capture(false);
  toplevel->SetCapture(nullptr);
  EXPECT_TRUE(toplevel->HasCapture());
  toplevel->OnMouseEvent(&release);
  EXPECT_TRUE(toplevel->HasCapture());
  toplevel->ReleaseCapture();
  EXPECT_FALSE(toplevel->HasCapture());

  toplevel->Close();
  RunPendingMessages();
}

TEST_F(WidgetTestInteractive, ResetCaptureOnGestureEnd) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  View* gesture = new GestureCaptureView;
  gesture->SetBounds(0, 0, 30, 30);
  container->AddChildView(gesture);

  MouseView* mouse = new MouseView;
  mouse->SetBounds(30, 0, 30, 30);
  container->AddChildView(mouse);

  toplevel->SetSize(gfx::Size(100, 100));
  toplevel->Show();

  // Start a gesture on |gesture|.
  ui::GestureEvent tap_down(15, 15, 0, base::TimeTicks(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  ui::GestureEvent end(15, 15, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  toplevel->OnGestureEvent(&tap_down);

  // Now try to click on |mouse|. Since |gesture| will have capture, |mouse|
  // will not receive the event.
  gfx::Point click_location(45, 15);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, click_location, click_location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(toplevel->HasCapture());

  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(0, mouse->pressed());

  EXPECT_FALSE(toplevel->HasCapture());

  // The end of the gesture should release the capture, and pressing on |mouse|
  // should now reach |mouse|.
  toplevel->OnGestureEvent(&end);
  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(1, mouse->pressed());

  toplevel->Close();
  RunPendingMessages();
}

// Checks that if a mouse-press triggers a capture on a different widget (which
// consumes the mouse-release event), then the target of the press does not have
// capture.
TEST_F(WidgetTestInteractive, DisableCaptureWidgetFromMousePress) {
  // The test creates two widgets: |first| and |second|.
  // The View in |first| makes |second| visible, sets capture on it, and starts
  // a nested loop (like a menu does). The View in |second| terminates the
  // nested loop and closes the widget.
  // The test sends a mouse-press event to |first|, and posts a task to send a
  // release event to |second|, to make sure that the release event is
  // dispatched after the nested loop starts.

  Widget* first = CreateTopLevelFramelessPlatformWidget();
  Widget* second = CreateTopLevelFramelessPlatformWidget();

  NestedLoopCaptureView* container = new NestedLoopCaptureView(second);
  first->SetContentsView(container);

  second->SetContentsView(new ExitLoopOnRelease(container->GetQuitClosure()));

  first->SetSize(gfx::Size(100, 100));
  first->Show();

  gfx::Point location(20, 20);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &Widget::OnMouseEvent, base::Unretained(second),
          base::Owned(new ui::MouseEvent(
              ui::ET_MOUSE_RELEASED, location, location, ui::EventTimeForNow(),
              ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON))));
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  first->OnMouseEvent(&press);
  EXPECT_FALSE(first->HasCapture());
  first->Close();
  RunPendingMessages();
}

// Tests some grab/ungrab events.
// TODO(estade): can this be enabled now that this is an interactive ui test?
TEST_F(WidgetTestInteractive, DISABLED_GrabUngrab) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* child1 = CreateChildNativeWidgetWithParent(toplevel);
  Widget* child2 = CreateChildNativeWidgetWithParent(toplevel);

  toplevel->SetBounds(gfx::Rect(0, 0, 500, 500));

  child1->SetBounds(gfx::Rect(10, 10, 300, 300));
  View* view = new MouseView();
  view->SetBounds(0, 0, 300, 300);
  child1->GetRootView()->AddChildView(view);

  child2->SetBounds(gfx::Rect(200, 10, 200, 200));
  view = new MouseView();
  view->SetBounds(0, 0, 200, 200);
  child2->GetRootView()->AddChildView(view);

  toplevel->Show();
  RunPendingMessages();

  // Click on child1
  gfx::Point p1(45, 45);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, p1, p1, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed);

  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, p1, p1, ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released);

  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  RunPendingMessages();

  // Click on child2
  gfx::Point p2(315, 45);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED, p2, p2, ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed2);
  EXPECT_TRUE(pressed2.handled());
  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child2->HasCapture());
  EXPECT_FALSE(child1->HasCapture());

  ui::MouseEvent released2(ui::ET_MOUSE_RELEASED, p2, p2, ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released2);
  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  toplevel->CloseNow();
}

// Tests mouse move outside of the window into the "resize controller" and back
// will still generate an OnMouseEntered and OnMouseExited event..
TEST_F(WidgetTestInteractive, CheckResizeControllerEvents) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();

  toplevel->SetBounds(gfx::Rect(0, 0, 100, 100));

  MouseView* view = new MouseView();
  view->SetBounds(90, 90, 10, 10);
  // |view| needs to be a particular size. Reset the LayoutManager so that
  // it doesn't get resized.
  toplevel->GetRootView()->SetLayoutManager(nullptr);
  toplevel->GetRootView()->AddChildView(view);

  toplevel->Show();
  RunPendingMessages();

  // Move to an outside position.
  gfx::Point p1(200, 200);
  ui::MouseEvent moved_out(ui::ET_MOUSE_MOVED, p1, p1, ui::EventTimeForNow(),
                           ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_out);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the active view.
  gfx::Point p2(95, 95);
  ui::MouseEvent moved_over(ui::ET_MOUSE_MOVED, p2, p2, ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the outer resizing border.
  gfx::Point p3(102, 95);
  ui::MouseEvent moved_resizer(ui::ET_MOUSE_MOVED, p3, p3,
                               ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_resizer);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(1, view->ExitedCalls());

  // Move onto the view again.
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  RunPendingMessages();

  toplevel->CloseNow();
}

// Test view focus restoration when a widget is deactivated and re-activated.
TEST_F(WidgetTestInteractive, ViewFocusOnWidgetActivationChanges) {
  Widget* widget1 = CreateTopLevelPlatformWidget();
  View* view1 = new View;
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget1->GetContentsView()->AddChildView(view1);

  Widget* widget2 = CreateTopLevelPlatformWidget();
  View* view2a = new View;
  View* view2b = new View;
  view2a->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view2b->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget2->GetContentsView()->AddChildView(view2a);
  widget2->GetContentsView()->AddChildView(view2b);

  ShowSync(widget1);
  EXPECT_TRUE(widget1->IsActive());
  view1->RequestFocus();
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());

  ShowSync(widget2);
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(nullptr, widget1->GetFocusManager()->GetFocusedView());
  view2a->RequestFocus();
  EXPECT_EQ(view2a, widget2->GetFocusManager()->GetFocusedView());
  view2b->RequestFocus();
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget1);
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(nullptr, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget2);
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(nullptr, widget1->GetFocusManager()->GetFocusedView());

  widget1->CloseNow();
  widget2->CloseNow();
}

// Test z-order of child widgets relative to their parent.
TEST_F(WidgetTestInteractive, ChildStackedRelativeToParent) {
  WidgetAutoclosePtr parent(CreateTopLevelPlatformWidget());
  Widget* child = CreateChildPlatformWidget(parent->GetNativeView());

  parent->SetBounds(gfx::Rect(160, 100, 320, 200));
  child->SetBounds(gfx::Rect(50, 50, 30, 20));

  // Child shown first. Initially not visible, but on top of parent when shown.
  // Use ShowInactive whenever showing the child, otherwise the usual activation
  // logic will just put it on top anyway. Here, we want to ensure it is on top
  // of its parent regardless.
  child->ShowInactive();
  EXPECT_FALSE(child->IsVisible());

  ShowSync(parent.get());
  EXPECT_TRUE(child->IsVisible());
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));
  EXPECT_FALSE(IsWindowStackedAbove(parent.get(), child));  // Sanity check.

  WidgetAutoclosePtr popover(CreateTopLevelPlatformWidget());
  popover->SetBounds(gfx::Rect(150, 90, 340, 240));
  ShowSync(popover.get());

  // NOTE: for aura-mus-client stacking of top-levels is not maintained in the
  // client, so z-order of top-levels can't be determined.
  EXPECT_TRUE(IsWindowStackedAbove(popover.get(), child));
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));

  // Showing the parent again should raise it and its child above the popover.
  ShowSync(parent.get());
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));
    EXPECT_TRUE(IsWindowStackedAbove(parent.get(), popover.get()));

  // Test grandchildren.
  Widget* grandchild = CreateChildPlatformWidget(child->GetNativeView());
  grandchild->SetBounds(gfx::Rect(5, 5, 15, 10));
  grandchild->ShowInactive();
  EXPECT_TRUE(IsWindowStackedAbove(grandchild, child));
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));
    EXPECT_TRUE(IsWindowStackedAbove(parent.get(), popover.get()));

  ShowSync(popover.get());
    EXPECT_TRUE(IsWindowStackedAbove(popover.get(), grandchild));
  EXPECT_TRUE(IsWindowStackedAbove(grandchild, child));

  ShowSync(parent.get());
  EXPECT_TRUE(IsWindowStackedAbove(grandchild, child));
    EXPECT_TRUE(IsWindowStackedAbove(child, popover.get()));

  // Test hiding and reshowing.
  parent->Hide();
  EXPECT_FALSE(grandchild->IsVisible());
  ShowSync(parent.get());

  EXPECT_TRUE(IsWindowStackedAbove(grandchild, child));
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));
    EXPECT_TRUE(IsWindowStackedAbove(parent.get(), popover.get()));

  grandchild->Hide();
  EXPECT_FALSE(grandchild->IsVisible());
  grandchild->ShowInactive();

  EXPECT_TRUE(IsWindowStackedAbove(grandchild, child));
  EXPECT_TRUE(IsWindowStackedAbove(child, parent.get()));
    EXPECT_TRUE(IsWindowStackedAbove(parent.get(), popover.get()));
}

#if defined(OS_WIN)

// Test view focus retention when a widget's HWND is disabled and re-enabled.
TEST_F(WidgetTestInteractive, ViewFocusOnHWNDEnabledChanges) {
  Widget* widget = CreateTopLevelFramelessPlatformWidget();
  widget->SetContentsView(new View);
  for (size_t i = 0; i < 2; ++i) {
    auto child = std::make_unique<View>();
    child->SetFocusBehavior(View::FocusBehavior::ALWAYS);
    widget->GetContentsView()->AddChildView(std::move(child));
  }

  widget->Show();
  widget->GetNativeWindow()->GetHost()->Show();
  const HWND hwnd = HWNDForWidget(widget);
  EXPECT_TRUE(::IsWindow(hwnd));
  EXPECT_TRUE(::IsWindowEnabled(hwnd));
  EXPECT_EQ(hwnd, ::GetActiveWindow());

  for (View* view : widget->GetContentsView()->children()) {
    SCOPED_TRACE(base::StringPrintf(
        "Child view %d", widget->GetContentsView()->GetIndexOf(view)));

    view->RequestFocus();
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());
    EXPECT_FALSE(::EnableWindow(hwnd, FALSE));
    EXPECT_FALSE(::IsWindowEnabled(hwnd));

    // Oddly, disabling the HWND leaves it active with the focus unchanged.
    EXPECT_EQ(hwnd, ::GetActiveWindow());
    EXPECT_TRUE(widget->IsActive());
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());

    EXPECT_TRUE(::EnableWindow(hwnd, TRUE));
    EXPECT_TRUE(::IsWindowEnabled(hwnd));
    EXPECT_EQ(hwnd, ::GetActiveWindow());
    EXPECT_TRUE(widget->IsActive());
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());
  }

  widget->CloseNow();
}

// This class subclasses the Widget class to listen for activation change
// notifications and provides accessors to return information as to whether
// the widget is active. We need this to ensure that users of the widget
// class activate the widget only when the underlying window becomes really
// active. Previously we would activate the widget in the WM_NCACTIVATE
// message which is incorrect because APIs like FlashWindowEx flash the
// window caption by sending fake WM_NCACTIVATE messages.
class WidgetActivationTest : public Widget {
 public:
  WidgetActivationTest() = default;

  ~WidgetActivationTest() override = default;

  bool OnNativeWidgetActivationChanged(bool active) override {
    active_ = active;
    return true;
  }

  bool active() const { return active_; }

 private:
  bool active_ = false;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationTest);
};

// Tests whether the widget only becomes active when the underlying window
// is really active.
TEST_F(WidgetTestInteractive, WidgetNotActivatedOnFakeActivationMessages) {
  WidgetActivationTest widget1;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget1);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget1.Init(std::move(init_params));
  widget1.Show();
  EXPECT_EQ(true, widget1.active());

  WidgetActivationTest widget2;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(std::move(init_params));
  widget2.Show();
  EXPECT_EQ(true, widget2.active());
  EXPECT_EQ(false, widget1.active());

  HWND win32_native_window1 = HWNDForWidget(&widget1);
  EXPECT_TRUE(::IsWindow(win32_native_window1));

  ::SendMessage(win32_native_window1, WM_NCACTIVATE, 1, 0);
  EXPECT_EQ(false, widget1.active());
  EXPECT_EQ(true, widget2.active());

  ::SetActiveWindow(win32_native_window1);
  EXPECT_EQ(true, widget1.active());
  EXPECT_EQ(false, widget2.active());
}

// On Windows if we create a fullscreen window on a thread, then it affects the
// way other windows on the thread interact with the taskbar. To workaround
// this we reduce the bounds of a fullscreen window by 1px when it loses
// activation. This test verifies the same.
TEST_F(WidgetTestInteractive, FullscreenBoundsReducedOnActivationLoss) {
  Widget widget1;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new DesktopNativeWidgetAura(&widget1);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(std::move(params));
  widget1.SetBounds(gfx::Rect(0, 0, 200, 200));
  widget1.Show();

  widget1.Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1.GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  widget1.SetFullscreen(true);
  EXPECT_TRUE(widget1.IsFullscreen());
  // Ensure that the StopIgnoringPosChanges task in HWNDMessageHandler runs.
  // This task is queued when a widget becomes fullscreen.
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1.GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  gfx::Rect fullscreen_bounds = widget1.GetWindowBoundsInScreen();

  Widget widget2;
  params.native_widget = new DesktopNativeWidgetAura(&widget2);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(std::move(params));
  widget2.SetBounds(gfx::Rect(0, 0, 200, 200));
  widget2.Show();

  widget2.Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget2.GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  gfx::Rect fullscreen_bounds_after_activation_loss =
      widget1.GetWindowBoundsInScreen();

  // After deactivation loss the bounds of the fullscreen widget should be
  // reduced by 1px.
  EXPECT_EQ(fullscreen_bounds.height() -
            fullscreen_bounds_after_activation_loss.height(), 1);

  widget1.Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1.GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  gfx::Rect fullscreen_bounds_after_activate =
      widget1.GetWindowBoundsInScreen();

  // After activation the bounds of the fullscreen widget should be restored.
  EXPECT_EQ(fullscreen_bounds, fullscreen_bounds_after_activate);

  widget1.CloseNow();
  widget2.CloseNow();
}

// Ensure the window rect and client rects are correct with a window that was
// maximized.
TEST_F(WidgetTestInteractive, FullscreenMaximizedWindowBounds) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new DesktopNativeWidgetAura(&widget);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.set_frame_type(Widget::FrameType::kForceCustom);
  widget.Init(std::move(params));
  widget.SetBounds(gfx::Rect(0, 0, 200, 200));
  widget.Show();

  widget.Maximize();
  EXPECT_TRUE(widget.IsMaximized());

  widget.SetFullscreen(true);
  EXPECT_TRUE(widget.IsFullscreen());
  EXPECT_FALSE(widget.IsMaximized());
  // Ensure that the StopIgnoringPosChanges task in HWNDMessageHandler runs.
  // This task is queued when a widget becomes fullscreen.
  RunPendingMessages();

  aura::WindowTreeHost* host = widget.GetNativeWindow()->GetHost();
  HWND hwnd = host->GetAcceleratedWidget();

  HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  ASSERT_TRUE(!!monitor);
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  ASSERT_TRUE(::GetMonitorInfo(monitor, &monitor_info));

  gfx::Rect monitor_bounds(monitor_info.rcMonitor);
  gfx::Rect window_bounds = widget.GetWindowBoundsInScreen();
  gfx::Rect client_area_bounds = host->GetBoundsInPixels();

  EXPECT_EQ(window_bounds, monitor_bounds);
  EXPECT_EQ(monitor_bounds, client_area_bounds);

  // Setting not fullscreen should return it to maximized.
  widget.SetFullscreen(false);
  EXPECT_FALSE(widget.IsFullscreen());
  EXPECT_TRUE(widget.IsMaximized());

  client_area_bounds = host->GetBoundsInPixels();
  EXPECT_TRUE(monitor_bounds.Contains(client_area_bounds));
  EXPECT_NE(monitor_bounds, client_area_bounds);

  widget.CloseNow();
}
#endif  // defined(OS_WIN)

// Provides functionality to create a window modal dialog.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  explicit ModalDialogDelegate(ui::ModalType type) : type_(type) {}
  ~ModalDialogDelegate() override = default;

  // WidgetDelegate overrides.
  ui::ModalType GetModalType() const override { return type_; }

 private:
  ui::ModalType type_;

  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
// Tests whether the focused window is set correctly when a modal window is
// created and destroyed. When it is destroyed it should focus the owner window.
TEST_F(DesktopWidgetTestInteractive, WindowModalWindowDestroyedActivationTest) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);
  const std::vector<gfx::NativeView>& focus_changes =
      focus_listener.focus_changes();

  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  top_level_widget.Init(std::move(init_params));
  ShowSync(&top_level_widget);

  gfx::NativeView top_level_native_view = top_level_widget.GetNativeView();
  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(1u, focus_changes.size());
  EXPECT_EQ(top_level_native_view, focus_changes[0]);

  // Create a modal dialog.
  ui::ModalType modal_type = ui::MODAL_TYPE_WINDOW;
  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate(modal_type);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, nullptr, top_level_widget.GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));

  // Note the dialog widget doesn't need a ShowSync. Since it is modal, it gains
  // active status synchronously, even on Mac.
  modal_dialog_widget->Show();

  gfx::NativeView modal_native_view = modal_dialog_widget->GetNativeView();
  ASSERT_EQ(3u, focus_changes.size());
  EXPECT_EQ(gfx::kNullNativeView, focus_changes[1]);
  EXPECT_EQ(modal_native_view, focus_changes[2]);

#if defined(OS_MACOSX)
  // Window modal dialogs on Mac are "sheets", which animate to close before
  // activating their parent widget.
  views::test::WidgetActivationWaiter waiter(&top_level_widget, true);
  modal_dialog_widget->Close();
  waiter.Wait();
#else
  modal_dialog_widget->CloseNow();
#endif

  ASSERT_EQ(5u, focus_changes.size());
  EXPECT_EQ(gfx::kNullNativeView, focus_changes[3]);
  EXPECT_EQ(top_level_native_view, focus_changes[4]);

  top_level_widget.CloseNow();
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}
#endif

// Disabled on Mac. Desktop Mac doesn't have system modal windows since Carbon
// was deprecated. It does have application modal windows, but only Ash requests
// those.
#if defined(OS_MACOSX)
#define MAYBE_SystemModalWindowReleasesCapture \
  DISABLED_SystemModalWindowReleasesCapture
#elif defined(OS_CHROMEOS)
// Investigate enabling for Chrome OS. It probably requires help from the window
// service.
#define MAYBE_SystemModalWindowReleasesCapture \
    DISABLED_SystemModalWindowReleasesCapture
#else
#define MAYBE_SystemModalWindowReleasesCapture SystemModalWindowReleasesCapture
#endif

// Test that when opening a system-modal window, capture is released.
TEST_F(DesktopWidgetTestInteractive, MAYBE_SystemModalWindowReleasesCapture) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);

  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  top_level_widget.Init(std::move(init_params));
  ShowSync(&top_level_widget);

  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(top_level_widget.GetNativeView(),
            focus_listener.focus_changes().back());

  EXPECT_FALSE(top_level_widget.HasCapture());
  top_level_widget.SetCapture(nullptr);
  EXPECT_TRUE(top_level_widget.HasCapture());

  // Create a modal dialog.
  ModalDialogDelegate* dialog_delegate =
      new ModalDialogDelegate(ui::MODAL_TYPE_SYSTEM);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, nullptr, top_level_widget.GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  ShowSync(modal_dialog_widget);

  EXPECT_FALSE(top_level_widget.HasCapture());

  modal_dialog_widget->CloseNow();
  top_level_widget.CloseNow();
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

TEST_F(DesktopWidgetTestInteractive, CanActivateFlagIsHonored) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.activatable = Widget::InitParams::ACTIVATABLE_NO;
  widget.Init(std::move(init_params));

  widget.Show();
  EXPECT_FALSE(widget.IsActive());
}

#if defined(USE_AURA)

#if defined(OS_CHROMEOS)
// TODO(crbug.com/916272): investigate fixing and enabling on Chrome OS.
#define MAYBE_TouchSelectionQuickMenuIsNotActivated \
  DISABLED_TouchSelectionQuickMenuIsNotActivated
#else
#define MAYBE_TouchSelectionQuickMenuIsNotActivated \
  TouchSelectionQuickMenuIsNotActivated
#endif

// Test that touch selection quick menu is not activated when opened.
TEST_F(DesktopWidgetTestInteractive,
       MAYBE_TouchSelectionQuickMenuIsNotActivated) {
  Widget* widget = CreateWidget();

  Textfield* textfield = new Textfield;
  textfield->SetBounds(0, 0, 200, 20);
  textfield->SetText(base::ASCIIToUTF16("some text"));
  widget->GetRootView()->AddChildView(textfield);

  widget->Show();
  textfield->RequestFocus();
  textfield->SelectAll(true);
  TextfieldTestApi textfield_test_api(textfield);

  RunPendingMessages();

  ui::test::EventGenerator generator(GetRootWindow(widget));
  generator.GestureTapAt(textfield->GetBoundsInScreen().origin() +
                         gfx::Vector2d(10, 10));
  ShowQuickMenuImmediately(static_cast<TouchSelectionControllerImpl*>(
      textfield_test_api.touch_selection_controller()));

  EXPECT_TRUE(textfield->HasFocus());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  widget->CloseNow();
}
#endif  // defined(USE_AURA)

#if defined(OS_WIN)
TEST_F(DesktopWidgetTestInteractive, DisableViewDoesNotActivateWidget) {
#else
TEST_F(WidgetTestInteractive, DisableViewDoesNotActivateWidget) {
#endif  // !defined(OS_WIN)

  // Create first widget and view, activate the widget, and focus the view.
  Widget widget1;
  Widget::InitParams params1 = CreateParams(Widget::InitParams::TYPE_POPUP);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.activatable = Widget::InitParams::ACTIVATABLE_YES;
  widget1.Init(std::move(params1));

  View* view1 = new View();
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget1.GetRootView()->AddChildView(view1);

  widget1.Show();
  ActivateSync(&widget1);

  FocusManager* focus_manager1 = widget1.GetFocusManager();
  ASSERT_TRUE(focus_manager1);
  focus_manager1->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager1->GetFocusedView());

  // Create second widget and view, activate the widget, and focus the view.
  Widget widget2;
  Widget::InitParams params2 = CreateParams(Widget::InitParams::TYPE_POPUP);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.activatable = Widget::InitParams::ACTIVATABLE_YES;
  widget2.Init(std::move(params2));

  View* view2 = new View();
  view2->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget2.GetRootView()->AddChildView(view2);

  widget2.Show();
  ActivateSync(&widget2);
  EXPECT_TRUE(widget2.IsActive());
  EXPECT_FALSE(widget1.IsActive());

  FocusManager* focus_manager2 = widget2.GetFocusManager();
  ASSERT_TRUE(focus_manager2);
  focus_manager2->SetFocusedView(view2);
  EXPECT_EQ(view2, focus_manager2->GetFocusedView());

  // Disable the first view and make sure it loses focus, but its widget is not
  // activated.
  view1->SetEnabled(false);
  EXPECT_NE(view1, focus_manager1->GetFocusedView());
  EXPECT_FALSE(widget1.IsActive());
  EXPECT_TRUE(widget2.IsActive());
}

TEST_F(WidgetTestInteractive, ShowCreatesActiveWindow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowInactive) {
  WidgetTest::WaitForSystemAppActivation();
  Widget* widget = CreateTopLevelPlatformWidget();

  ShowInactiveSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_INACTIVE);

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, InactiveBeforeShow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->IsVisible());

  ShowSync(widget);

  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->IsVisible());

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowInactiveAfterShow) {
  // Create 2 widgets to ensure window layering does not change.
  Widget* widget = CreateTopLevelPlatformWidget();
  Widget* widget2 = CreateTopLevelPlatformWidget();

  ShowSync(widget2);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget2->IsVisible());
  EXPECT_TRUE(widget2->IsActive());

  ShowSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  ShowInactiveSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget2->CloseNow();
  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowAfterShowInactive) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(100, 100, 100, 100));

  ShowInactiveSync(widget);
  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
TEST_F(WidgetTestInteractive, InactiveWidgetDoesNotGrabActivation) {
  Widget* widget = CreateTopLevelPlatformWidget();
  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  Widget widget2;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(std::move(params));
  widget2.Show();
  RunPendingMessagesForActiveStatusChange();

  EXPECT_EQ(GetWidgetShowState(&widget2), ui::SHOW_STATE_INACTIVE);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
  widget2.CloseNow();
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)

// ExitFullscreenRestoreState doesn't use DesktopAura widgets. On Mac, there are
// currently only Desktop widgets and fullscreen changes have to coordinate with
// the OS. See BridgedNativeWidgetUITest for native Mac fullscreen tests.
// Maximize on mac is also (intentionally) a no-op.
#if defined(OS_MACOSX)
#define MAYBE_ExitFullscreenRestoreState DISABLED_ExitFullscreenRestoreState
#else
#define MAYBE_ExitFullscreenRestoreState ExitFullscreenRestoreState
#endif

// Test that window state is not changed after getting out of full screen.
TEST_F(WidgetTestInteractive, MAYBE_ExitFullscreenRestoreState) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  toplevel->Show();
  RunPendingMessages();

  // This should be a normal state window.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));

  // And it should still be in normal state after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  // Now, make it maximized.
  toplevel->Maximize();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetWidgetShowState(toplevel));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));

  // And it stays maximized after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetWidgetShowState(toplevel));

  // Clean up.
  toplevel->Close();
  RunPendingMessages();
}

// Testing initial focus is assigned properly for normal top-level widgets,
// and subclasses that specify a initially focused child view.
TEST_F(WidgetTestInteractive, InitialFocus) {
  // By default, there is no initially focused view (even if there is a
  // focusable subview).
  Widget* toplevel(CreateTopLevelPlatformWidget());
  View* view = new View;
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  toplevel->GetContentsView()->AddChildView(view);

  ShowSync(toplevel);
  toplevel->Show();
  EXPECT_FALSE(view->HasFocus());
  EXPECT_FALSE(toplevel->GetFocusManager()->GetStoredFocusView());
  toplevel->CloseNow();

  // Testing a widget which specifies a initially focused view.
  TestInitialFocusWidgetDelegate delegate(GetContext());

  Widget* widget = delegate.GetWidget();
  ShowSync(widget);
  widget->Show();
  EXPECT_TRUE(delegate.view()->HasFocus());
  EXPECT_EQ(delegate.view(), widget->GetFocusManager()->GetStoredFocusView());
}

TEST_F(DesktopWidgetTestInteractive, RestoreAfterMinimize) {
  Widget* widget = CreateWidget();
  ShowSync(widget);
  ASSERT_FALSE(widget->IsMinimized());

  PropertyWaiter minimize_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget)),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_waiter.Wait());

  PropertyWaiter restore_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget)),
      false);
  widget->Restore();
  EXPECT_TRUE(restore_waiter.Wait());

  widget->CloseNow();
}

#if defined(OS_WIN)
// TODO(davidbienvenu): Get this test to pass on Linux and ChromeOS by hiding
// the root window when desktop widget is minimized.
// Tests that root window visibility toggles correctly when the desktop widget
// is minimized and maximized on Windows, and the Widget remains visible.
TEST_F(DesktopWidgetTestInteractive, RestoreAndMinimizeVisibility) {
  Widget* widget = CreateWidget();
  aura::Window* root_window = GetRootWindow(widget);
  ShowSync(widget);
  ASSERT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(root_window->IsVisible());

  PropertyWaiter minimize_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget)),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(root_window->IsVisible());

  PropertyWaiter restore_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget)),
      false);
  widget->Restore();
  EXPECT_TRUE(restore_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(root_window->IsVisible());
  widget->CloseNow();
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
// Tests that minimizing a widget causes the gesture_handler
// to be cleared when the widget is minimized.
TEST_F(DesktopWidgetTestInteractive, EventHandlersClearedOnWidgetMinimize) {
  Widget* widget = CreateWidget();
  ShowSync(widget);
  ASSERT_FALSE(widget->IsMinimized());
  View mouse_handler_view;
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  // This also sets the gesture_handler, and we'll verify that it
  // gets cleared when the widget is minimized.
  root_view->SetMouseHandler(&mouse_handler_view);
  EXPECT_TRUE(GetGestureHandler(root_view));

  widget->Minimize();
  EXPECT_FALSE(GetGestureHandler(root_view));

  widget->CloseNow();
}
#endif

#if defined(OS_LINUX) && BUILDFLAG(ENABLE_DESKTOP_AURA)
// Tests that when a desktop native widget has modal transient child, it should
// avoid restore focused view itself as the modal transient child window will do
// that, thus avoids having multiple focused view visually (crbug.com/727641).
TEST_F(DesktopWidgetTestInteractive,
       DesktopNativeWidgetWithModalTransientChild) {
  // Create a desktop native Widget for Widget::Deactivate().
  Widget* deactivate_widget = CreateWidget();
  ShowSync(deactivate_widget);

  // Create a top level desktop native widget.
  Widget* top_level = CreateWidget();

  Textfield* textfield = new Textfield;
  textfield->SetBounds(0, 0, 200, 20);
  top_level->GetRootView()->AddChildView(textfield);
  ShowSync(top_level);
  textfield->RequestFocus();
  EXPECT_TRUE(textfield->HasFocus());

  // Create a modal dialog.
  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate =
      new ModalDialogDelegate(ui::MODAL_TYPE_WINDOW);
  Widget* modal_dialog_widget = DialogDelegate::CreateDialogWidget(
      dialog_delegate, nullptr, top_level->GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(0, 0, 100, 10));
  Textfield* dialog_textfield = new Textfield;
  dialog_textfield->SetBounds(0, 0, 50, 5);
  modal_dialog_widget->GetRootView()->AddChildView(dialog_textfield);
  // Dialog widget doesn't need a ShowSync as it gains active status
  // synchronously.
  modal_dialog_widget->Show();
  dialog_textfield->RequestFocus();
  EXPECT_TRUE(dialog_textfield->HasFocus());
  EXPECT_FALSE(textfield->HasFocus());

  DeactivateSync(top_level);
  EXPECT_FALSE(dialog_textfield->HasFocus());
  EXPECT_FALSE(textfield->HasFocus());

  // After deactivation and activation of top level widget, only modal dialog
  // should restore focused view.
  ActivateSync(top_level);
  EXPECT_TRUE(dialog_textfield->HasFocus());
  EXPECT_FALSE(textfield->HasFocus());

  top_level->CloseNow();
  deactivate_widget->CloseNow();
}
#endif  // defined(OS_LINUX) && BUILDFLAG(ENABLE_DESKTOP_AURA)

namespace {

// Helper class for CaptureLostTrackingWidget to store whether
// OnMouseCaptureLost has been invoked for a widget.
class CaptureLostState {
 public:
  CaptureLostState() = default;

  bool GetAndClearGotCaptureLost() {
    bool value = got_capture_lost_;
    got_capture_lost_ = false;
    return value;
  }

  void OnMouseCaptureLost() { got_capture_lost_ = true; }

 private:
  bool got_capture_lost_ = false;

  DISALLOW_COPY_AND_ASSIGN(CaptureLostState);
};

// Used to verify OnMouseCaptureLost() has been invoked.
class CaptureLostTrackingWidget : public Widget {
 public:
  explicit CaptureLostTrackingWidget(CaptureLostState* capture_lost_state)
      : capture_lost_state_(capture_lost_state) {}

  // Widget:
  void OnMouseCaptureLost() override {
    capture_lost_state_->OnMouseCaptureLost();
    Widget::OnMouseCaptureLost();
  }

 private:
  // Weak. Stores whether OnMouseCaptureLost has been invoked for this widget.
  CaptureLostState* capture_lost_state_;

  DISALLOW_COPY_AND_ASSIGN(CaptureLostTrackingWidget);
};

}  // namespace

class WidgetCaptureTest : public ViewsInteractiveUITestBase {
 public:
  WidgetCaptureTest() = default;
  ~WidgetCaptureTest() override = default;

  // Verifies Widget::SetCapture() results in updating native capture along with
  // invoking the right Widget function.
  void TestCapture(bool use_desktop_native_widget) {
    CaptureLostState capture_state1;
    CaptureLostTrackingWidget widget1(&capture_state1);
    Widget::InitParams params1 =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params1.native_widget =
        CreateNativeWidget(params1, use_desktop_native_widget, &widget1);
    params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget1.Init(std::move(params1));
    widget1.Show();

    CaptureLostState capture_state2;
    CaptureLostTrackingWidget widget2(&capture_state2);
    Widget::InitParams params2 =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params2.native_widget =
        CreateNativeWidget(params2, use_desktop_native_widget, &widget2);
    widget2.Init(std::move(params2));
    widget2.Show();

    // Set capture to widget2 and verity it gets it.
    widget2.SetCapture(widget2.GetRootView());
    EXPECT_FALSE(widget1.HasCapture());
    EXPECT_TRUE(widget2.HasCapture());
    EXPECT_FALSE(capture_state1.GetAndClearGotCaptureLost());
    EXPECT_FALSE(capture_state2.GetAndClearGotCaptureLost());

    // Set capture to widget1 and verify it gets it.
    widget1.SetCapture(widget1.GetRootView());
    EXPECT_TRUE(widget1.HasCapture());
    EXPECT_FALSE(widget2.HasCapture());
    EXPECT_FALSE(capture_state1.GetAndClearGotCaptureLost());
    EXPECT_TRUE(capture_state2.GetAndClearGotCaptureLost());

    // Release and verify no one has it.
    widget1.ReleaseCapture();
    EXPECT_FALSE(widget1.HasCapture());
    EXPECT_FALSE(widget2.HasCapture());
    EXPECT_TRUE(capture_state1.GetAndClearGotCaptureLost());
    EXPECT_FALSE(capture_state2.GetAndClearGotCaptureLost());
  }

  NativeWidget* CreateNativeWidget(const Widget::InitParams& params,
                                   bool create_desktop_native_widget,
                                   Widget* widget) {
    // The test base class by default returns DesktopNativeWidgetAura.
    if (create_desktop_native_widget)
      return nullptr;
    return CreatePlatformNativeWidgetImpl(params, widget, kDefault, nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetCaptureTest);
};

// See description in TestCapture().
TEST_F(WidgetCaptureTest, Capture) {
  TestCapture(false);
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
// See description in TestCapture(). Creates DesktopNativeWidget.
TEST_F(WidgetCaptureTest, CaptureDesktopNativeWidget) {
  TestCapture(true);
}
#endif

// Tests to ensure capture is correctly released from a Widget with capture when
// it is destroyed. Test for crbug.com/622201.
TEST_F(WidgetCaptureTest, DestroyWithCapture_CloseNow) {
  CaptureLostState capture_state;
  CaptureLostTrackingWidget* widget =
      new CaptureLostTrackingWidget(&capture_state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->Show();

  widget->SetCapture(widget->GetRootView());
  EXPECT_TRUE(widget->HasCapture());
  EXPECT_FALSE(capture_state.GetAndClearGotCaptureLost());
  widget->CloseNow();

  EXPECT_TRUE(capture_state.GetAndClearGotCaptureLost());
}

TEST_F(WidgetCaptureTest, DestroyWithCapture_Close) {
  CaptureLostState capture_state;
  CaptureLostTrackingWidget* widget =
      new CaptureLostTrackingWidget(&capture_state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->Show();

  widget->SetCapture(widget->GetRootView());
  EXPECT_TRUE(widget->HasCapture());
  EXPECT_FALSE(capture_state.GetAndClearGotCaptureLost());
  widget->Close();
  EXPECT_TRUE(capture_state.GetAndClearGotCaptureLost());
}

TEST_F(WidgetCaptureTest, DestroyWithCapture_WidgetOwnsNativeWidget) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(params));
  widget.Show();

  widget.SetCapture(widget.GetRootView());
  EXPECT_TRUE(widget.HasCapture());
}

// Test that no state is set if capture fails.
TEST_F(WidgetCaptureTest, FailedCaptureRequestIsNoop) {
  Widget widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(400, 400);
  widget.Init(std::move(params));

  MouseView* mouse_view1 = new MouseView;
  MouseView* mouse_view2 = new MouseView;
  View* contents_view = new View;
  contents_view->AddChildView(mouse_view1);
  contents_view->AddChildView(mouse_view2);
  widget.SetContentsView(contents_view);

  mouse_view1->SetBounds(0, 0, 200, 400);
  mouse_view2->SetBounds(200, 0, 200, 400);

  // Setting capture should fail because |widget| is not visible.
  widget.SetCapture(mouse_view1);
  EXPECT_FALSE(widget.HasCapture());

  widget.Show();
  ui::test::EventGenerator generator(GetRootWindow(&widget),
                                     widget.GetNativeWindow());
  generator.set_current_screen_location(gfx::Point(300, 10));
  generator.PressLeftButton();

  EXPECT_FALSE(mouse_view1->pressed());
  EXPECT_TRUE(mouse_view2->pressed());
}

// Regression test for http://crbug.com/382421 (Linux-Aura issue).
// TODO(pkotwicz): Make test pass on CrOS and Windows.
// TODO(tapted): Investigate for toolkit-views on Mac http;//crbug.com/441064.
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_MouseExitOnCaptureGrab DISABLED_MouseExitOnCaptureGrab
#else
#define MAYBE_MouseExitOnCaptureGrab MouseExitOnCaptureGrab
#endif

// Test that a synthetic mouse exit is sent to the widget which was handling
// mouse events when a different widget grabs capture.
TEST_F(WidgetCaptureTest, MAYBE_MouseExitOnCaptureGrab) {
  Widget widget1;
  Widget::InitParams params1 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(std::move(params1));
  MouseView* mouse_view1 = new MouseView;
  widget1.SetContentsView(mouse_view1);
  widget1.Show();
  widget1.SetBounds(gfx::Rect(300, 300));

  Widget widget2;
  Widget::InitParams params2 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(std::move(params2));
  widget2.Show();
  widget2.SetBounds(gfx::Rect(400, 0, 300, 300));

  ui::test::EventGenerator generator(GetRootWindow(&widget1));
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.MoveMouseBy(0, 0);

  EXPECT_EQ(1, mouse_view1->EnteredCalls());
  EXPECT_EQ(0, mouse_view1->ExitedCalls());

  widget2.SetCapture(nullptr);
  EXPECT_EQ(0, mouse_view1->EnteredCalls());
  // Grabbing native capture on Windows generates a ui::ET_MOUSE_EXITED event
  // in addition to the one generated by Chrome.
  EXPECT_LT(0, mouse_view1->ExitedCalls());
}

namespace {

// Widget observer which grabs capture when the widget is activated.
class CaptureOnActivationObserver : public WidgetObserver {
 public:
  CaptureOnActivationObserver() = default;
  ~CaptureOnActivationObserver() override = default;

  // WidgetObserver:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      widget->SetCapture(nullptr);
      activation_observed_ = true;
    }
  }

  bool activation_observed() const { return activation_observed_; }

 private:
  bool activation_observed_ = false;

  DISALLOW_COPY_AND_ASSIGN(CaptureOnActivationObserver);
};

}  // namespace

// Test that setting capture on widget activation of a non-toplevel widget
// (e.g. a bubble on Linux) succeeds.
TEST_F(WidgetCaptureTest, SetCaptureToNonToplevel) {
  Widget toplevel;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel.Init(std::move(toplevel_params));
  toplevel.Show();

  Widget* child = new Widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  child_params.parent = toplevel.GetNativeView();
  child_params.context = toplevel.GetNativeWindow();
  child->Init(std::move(child_params));

  CaptureOnActivationObserver observer;
  child->AddObserver(&observer);
  child->Show();

#if defined(OS_MACOSX)
  // On Mac, activation is asynchronous. A single trip to the runloop should be
  // sufficient. On Aura platforms, note that since the child widget isn't top-
  // level, the aura window manager gets asked whether the widget is active, not
  // the OS.
  base::RunLoop().RunUntilIdle();
#endif

  EXPECT_TRUE(observer.activation_observed());
  EXPECT_TRUE(child->HasCapture());

  child->RemoveObserver(&observer);
}


#if defined(OS_WIN)
namespace {

// Used to verify OnMouseEvent() has been invoked.
class MouseEventTrackingWidget : public Widget {
 public:
  MouseEventTrackingWidget() = default;
  ~MouseEventTrackingWidget() override = default;

  bool GetAndClearGotMouseEvent() {
    bool value = got_mouse_event_;
    got_mouse_event_ = false;
    return value;
  }

  // Widget:
  void OnMouseEvent(ui::MouseEvent* event) override {
    got_mouse_event_ = true;
    Widget::OnMouseEvent(event);
  }

 private:
  bool got_mouse_event_ = false;

  DISALLOW_COPY_AND_ASSIGN(MouseEventTrackingWidget);
};

}  // namespace

// Verifies if a mouse event is received on a widget that doesn't have capture
// on Windows that it is correctly processed by the widget that doesn't have
// capture. This behavior is not desired on OSes other than Windows.
TEST_F(WidgetCaptureTest, MouseEventDispatchedToRightWindow) {
  MouseEventTrackingWidget widget1;
  Widget::InitParams params1 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params1.native_widget = new DesktopNativeWidgetAura(&widget1);
  params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(std::move(params1));
  widget1.Show();

  MouseEventTrackingWidget widget2;
  Widget::InitParams params2 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(std::move(params2));
  widget2.Show();

  // Set capture to widget2 and verity it gets it.
  widget2.SetCapture(widget2.GetRootView());
  EXPECT_FALSE(widget1.HasCapture());
  EXPECT_TRUE(widget2.HasCapture());

  widget1.GetAndClearGotMouseEvent();
  widget2.GetAndClearGotMouseEvent();
  // Send a mouse event to the RootWindow associated with |widget1|. Even though
  // |widget2| has capture, |widget1| should still get the event.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      widget1.GetNativeWindow()->GetHost()->event_sink()->OnEventFromSource(
          &mouse_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(widget1.GetAndClearGotMouseEvent());
  EXPECT_FALSE(widget2.GetAndClearGotMouseEvent());
}
#endif  // defined(OS_WIN)

class WidgetInputMethodInteractiveTest : public DesktopWidgetTestInteractive {
 public:
  WidgetInputMethodInteractiveTest() = default;

  // testing::Test:
  void SetUp() override {
    DesktopWidgetTestInteractive::SetUp();
#if defined(OS_WIN)
    // On Windows, Widget::Deactivate() works by activating the next topmost
    // window on the z-order stack. This only works if there is at least one
    // other window, so make sure that is the case.
    deactivate_widget_ = CreateWidget();
    deactivate_widget_->Show();
#endif
  }

  void TearDown() override {
    if (deactivate_widget_)
      deactivate_widget_->CloseNow();
    DesktopWidgetTestInteractive::TearDown();
  }

 private:
  Widget* deactivate_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WidgetInputMethodInteractiveTest);
};

#if defined(OS_MACOSX)
#define MAYBE_Activation DISABLED_Activation
#else
#define MAYBE_Activation Activation
#endif
// Test input method focus changes affected by top window activaction.
TEST_F(WidgetInputMethodInteractiveTest, MAYBE_Activation) {
  Widget* widget = CreateWidget();
  Textfield* textfield = new Textfield;
  widget->GetRootView()->AddChildView(textfield);
  textfield->RequestFocus();

  ShowSync(widget);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
  widget->CloseNow();
}

// Test input method focus changes affected by focus changes within 1 window.
TEST_F(WidgetInputMethodInteractiveTest, OneWindow) {
  Widget* widget = CreateWidget();
  Textfield* textfield1 = new Textfield;
  Textfield* textfield2 = new Textfield;
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  widget->GetRootView()->AddChildView(textfield1);
  widget->GetRootView()->AddChildView(textfield2);

  ShowSync(widget);

  textfield1->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  textfield2->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

// Widget::Deactivate() doesn't work for CrOS, because it uses NWA instead of
// DNWA (which just activates the last active window) and involves the
// AuraTestHelper which sets the input method as DummyInputMethod.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
  DeactivateSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  ActivateSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget);
  textfield1->RequestFocus();
  ActivateSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());
#endif
  widget->CloseNow();
}

// Test input method focus changes affected by focus changes cross 2 windows
// which shares the same top window.
TEST_F(WidgetInputMethodInteractiveTest, TwoWindows) {
  Widget* parent = CreateWidget();
  parent->SetBounds(gfx::Rect(100, 100, 100, 100));

  Widget* child = CreateChildNativeWidgetWithParent(parent);
  child->SetBounds(gfx::Rect(0, 0, 50, 50));
  child->Show();

  Textfield* textfield_parent = new Textfield;
  Textfield* textfield_child = new Textfield;
  textfield_parent->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  parent->GetRootView()->AddChildView(textfield_parent);
  child->GetRootView()->AddChildView(textfield_child);
  ShowSync(parent);

  EXPECT_EQ(parent->GetInputMethod(), child->GetInputMethod());

  textfield_parent->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            parent->GetInputMethod()->GetTextInputType());

  textfield_child->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            parent->GetInputMethod()->GetTextInputType());

// Widget::Deactivate() doesn't work for CrOS, because it uses NWA instead of
// DNWA (which just activates the last active window) and involves the
// AuraTestHelper which sets the input method as DummyInputMethod.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || defined(OS_MACOSX)
  DeactivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            parent->GetInputMethod()->GetTextInputType());

  textfield_parent->RequestFocus();
  DeactivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            parent->GetInputMethod()->GetTextInputType());
#endif

  parent->CloseNow();
}

// Test input method focus changes affected by textfield's state changes.
TEST_F(WidgetInputMethodInteractiveTest, TextField) {
  Widget* widget = CreateWidget();
  Textfield* textfield = new Textfield;
  widget->GetRootView()->AddChildView(textfield);
  ShowSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  textfield->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
  widget->CloseNow();
}

// Test input method should not work for accelerator.
TEST_F(WidgetInputMethodInteractiveTest, AcceleratorInTextfield) {
  Widget* widget = CreateWidget();
  Textfield* textfield = new Textfield;
  widget->GetRootView()->AddChildView(textfield);
  ShowSync(widget);
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  textfield->RequestFocus();

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED,
                         ui::VKEY_F, ui::EF_ALT_DOWN);
  ui::Accelerator accelerator(key_event);
  widget->GetFocusManager()->RegisterAccelerator(
      accelerator, ui::AcceleratorManager::kNormalPriority,
      textfield);

  widget->OnKeyEvent(&key_event);
  EXPECT_TRUE(key_event.stopped_propagation());

  widget->GetFocusManager()->UnregisterAccelerators(textfield);

  ui::KeyEvent key_event2(key_event);
  widget->OnKeyEvent(&key_event2);
  EXPECT_FALSE(key_event2.stopped_propagation());

  widget->CloseNow();
}

}  // namespace test
}  // namespace views
