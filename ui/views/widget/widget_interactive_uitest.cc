// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/public/activation_client.h"

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/aura/env.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace views::test {

namespace {

// A View that closes the Widget and exits the current message-loop when it
// receives a mouse-release event.
class ExitLoopOnRelease : public View {
  METADATA_HEADER(ExitLoopOnRelease, View)

 public:
  explicit ExitLoopOnRelease(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    DCHECK(quit_closure_);
  }

  ExitLoopOnRelease(const ExitLoopOnRelease&) = delete;
  ExitLoopOnRelease& operator=(const ExitLoopOnRelease&) = delete;

  ~ExitLoopOnRelease() override = default;

 private:
  // View:
  void OnMouseReleased(const ui::MouseEvent& event) override {
    GetWidget()->Close();
    std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
};

BEGIN_METADATA(ExitLoopOnRelease)
END_METADATA

// A view that does a capture on ui::EventType::kGestureTapDown events.
class GestureCaptureView : public View {
  METADATA_HEADER(GestureCaptureView, View)

 public:
  GestureCaptureView() = default;

  GestureCaptureView(const GestureCaptureView&) = delete;
  GestureCaptureView& operator=(const GestureCaptureView&) = delete;

  ~GestureCaptureView() override = default;

 private:
  // View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::EventType::kGestureTapDown) {
      GetWidget()->SetCapture(this);
      event->StopPropagation();
    }
  }
};

BEGIN_METADATA(GestureCaptureView)
END_METADATA

// A view that always processes all mouse events.
class MouseView : public View {
  METADATA_HEADER(MouseView, View)

 public:
  MouseView() = default;

  MouseView(const MouseView&) = delete;
  MouseView& operator=(const MouseView&) = delete;

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
};

BEGIN_METADATA(MouseView)
END_METADATA

// A View that shows a different widget, sets capture on that widget, and
// initiates a nested message-loop when it receives a mouse-press event.
class NestedLoopCaptureView : public View {
  METADATA_HEADER(NestedLoopCaptureView, View)

 public:
  explicit NestedLoopCaptureView(std::unique_ptr<Widget> widget)
      : run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
        widget_(std::move(widget)) {}

  NestedLoopCaptureView(const NestedLoopCaptureView&) = delete;
  NestedLoopCaptureView& operator=(const NestedLoopCaptureView&) = delete;

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

  std::unique_ptr<Widget> widget_;
};

BEGIN_METADATA(NestedLoopCaptureView)
END_METADATA

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
// A view that runs closures in response to drag events.
class DragView : public View, public DragController {
  METADATA_HEADER(DragView, View)

 public:
  DragView(base::OnceClosure on_drag_enter,
           base::OnceClosure on_drag_exit,
           base::OnceClosure on_capture_lost,
           base::OnceClosure on_mouse_exit)
      : on_drag_enter_(std::move(on_drag_enter)),
        on_drag_exit_(std::move(on_drag_exit)),
        on_capture_lost_(std::move(on_capture_lost)),
        on_mouse_exit_(std::move(on_mouse_exit)) {
    set_drag_controller(this);
  }

  DragView(const DragView&) = delete;
  DragView& operator=(const DragView&) = delete;

  ~DragView() override = default;

 private:
  // DragController:
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& current_pt) override {
    EXPECT_EQ(sender, this);
    return true;
  }

  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& press_pt) override {
    EXPECT_EQ(sender, this);
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {
    data->provider().SetString(u"test");

    // Without this, Lacros won't add the chromium/x-data-transfer-endpoint MIME
    // type to the list of available types, and without that Exo won't start a
    // drag session.
    data->SetSource(
        std::make_unique<ui::DataTransferEndpoint>(ui::EndpointType::kDefault));
  }

  // View:

  // See the comment for `received_drag_event_` for why this is Lacros-only.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMouseDragged) {
      received_drag_event_ = true;
    }
    View::OnMouseEvent(event);
  }
#endif

  bool GetDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) override { return true; }

  void OnDragEntered(const ui::DropTargetEvent& event) override {
    if (on_drag_enter_) {
      std::move(on_drag_enter_).Run();
    }
  }

  void OnDragExited() override {
    if (on_drag_exit_) {
      std::move(on_drag_exit_).Run();
    }
  }

  void OnMouseCaptureLost() override {
    if (on_capture_lost_) {
      std::move(on_capture_lost_).Run();
    }
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    // Depending on the initial mouse position and the timing when the OS
    // informs us about it, we might get an extra mouse exit event that is
    // unrelated to DnD.
    if (received_drag_event_ && on_mouse_exit_) {
      std::move(on_mouse_exit_).Run();
    }
  }

  // Whether we've received an EventType::kMouseDragged event yet.
  //
  // This is needed on Lacros, where we sometimes get an EventType::kMouseExited
  // event that's unrelated to DnD. To prevent that from messing up the test
  // flow, we ignore all such events until we receive an
  // EventType::kMouseDragged event. On all other platforms, initializing it to
  // true disables this workaround.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool received_drag_event_ = false;
#else
  bool received_drag_event_ = true;
#endif
  base::OnceClosure on_drag_enter_, on_drag_exit_, on_capture_lost_,
      on_mouse_exit_;
};

BEGIN_METADATA(DragView)
END_METADATA
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

ui::mojom::WindowShowState GetWidgetShowState(const Widget* widget) {
  // Use IsMaximized/IsMinimized/IsFullScreen instead of GetWindowPlacement
  // because the former is implemented on all platforms but the latter is not.
  if (widget->IsFullscreen())
    return ui::mojom::WindowShowState::kFullscreen;
  if (widget->IsMaximized())
    return ui::mojom::WindowShowState::kMaximized;
  if (widget->IsMinimized())
    return ui::mojom::WindowShowState::kMinimized;
  return widget->IsActive() ? ui::mojom::WindowShowState::kNormal
                            : ui::mojom::WindowShowState::kInactive;
}

// Give the OS an opportunity to process messages for an activation change, when
// there is actually no change expected (e.g. ShowInactive()).
void RunPendingMessagesForActiveStatusChange() {
#if BUILDFLAG(IS_MAC)
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
  widget->Activate();
  views::test::WaitForWidgetActive(widget, true);
}

// Like for ActivateSync(), wait for a widget to become active, but Show() the
// widget rather than calling Activate().
void ShowSync(Widget* widget) {
  widget->Show();
  views::test::WaitForWidgetActive(widget, true);
}

void DeactivateSync(Widget* widget) {
#if BUILDFLAG(IS_MAC)
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
  widget->Deactivate();
  views::test::WaitForWidgetActive(widget, false);
#endif
}

#if BUILDFLAG(IS_WIN)
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

std::unique_ptr<Textfield> CreateTextfield() {
  auto textfield = std::make_unique<Textfield>();
  // Focusable views must have an accessible name in order to pass the
  // accessibility paint checks. The name can be literal text, placeholder
  // text or an associated label.
  textfield->GetViewAccessibility().SetName(u"Foo");
  return textfield;
}

}  // namespace

class WidgetTestInteractive : public WidgetTest {
 public:
  WidgetTestInteractive() = default;
  ~WidgetTestInteractive() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    WidgetTest::SetUp();
  }
};

#if BUILDFLAG(IS_WIN)
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
  std::unique_ptr<Widget> widget1 = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  widget1->GetContentsView()->AddChildView(focusable_view1);
  widget1->Show();
  aura::Window* root_window1 = GetRootWindow(widget1.get());
  focusable_view1->RequestFocus();

  EXPECT_TRUE(root_window1 != nullptr);
  wm::ActivationClient* activation_client1 =
      wm::GetActivationClient(root_window1);
  EXPECT_TRUE(activation_client1 != nullptr);
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());

  // Create widget 2 and expect the active window to be its window.
  View* focusable_view2 = new View;
  std::unique_ptr<Widget> widget2 = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  widget1->GetContentsView()->AddChildView(focusable_view2);
  widget2->Show();
  aura::Window* root_window2 = GetRootWindow(widget2.get());
  focusable_view2->RequestFocus();
  ActivatePlatformWindow(widget2.get());

  wm::ActivationClient* activation_client2 =
      wm::GetActivationClient(root_window2);
  EXPECT_TRUE(activation_client2 != nullptr);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2->GetNativeView());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));

  // Now set focus back to widget 1 and expect the active window to be its
  // window.
  focusable_view1->RequestFocus();
  ActivatePlatformWindow(widget1.get());
  EXPECT_EQ(activation_client2->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());
}

// Verifies bubbles result in a focus lost when shown.
TEST_F(DesktopWidgetTestInteractive, FocusChangesOnBubble) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  View* focusable_view =
      widget->GetContentsView()->AddChildView(std::make_unique<View>());
  focusable_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget->Show();
  focusable_view->RequestFocus();
  EXPECT_TRUE(focusable_view->HasFocus());

  // Show a bubble.
  auto owned_bubble_delegate_view =
      std::make_unique<views::BubbleDialogDelegateView>(focusable_view,
                                                        BubbleBorder::NONE);
  owned_bubble_delegate_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  BubbleDialogDelegateView* bubble_delegate_view =
      owned_bubble_delegate_view.get();
  BubbleDialogDelegateView::CreateBubble(std::move(owned_bubble_delegate_view))
      ->Show();
  bubble_delegate_view->RequestFocus();

  // |focusable_view| should no longer have focus.
  EXPECT_FALSE(focusable_view->HasFocus());
  EXPECT_TRUE(bubble_delegate_view->HasFocus());

  bubble_delegate_view->GetWidget()->CloseNow();

  // Closing the bubble should result in focus going back to the contents view.
  EXPECT_TRUE(focusable_view->HasFocus());
}

class TouchEventHandler : public ui::EventHandler {
 public:
  explicit TouchEventHandler(Widget* widget) : widget_(widget) {
    widget_->GetNativeWindow()->GetHost()->window()->AddPreTargetHandler(this);
  }

  TouchEventHandler(const TouchEventHandler&) = delete;
  TouchEventHandler& operator=(const TouchEventHandler&) = delete;

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
    if (event->type() == ui::EventType::kTouchPressed) {
      ActivateViaMouse();
    }
  }

  raw_ptr<Widget> widget_;
  base::OnceClosure quit_closure_;
};

// TODO(dtapuska): Disabled due to it being flaky crbug.com/817531
TEST_F(DesktopWidgetTestInteractive, DISABLED_TouchNoActivateWindow) {
  View* focusable_view = new View;
  focusable_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  widget->GetContentsView()->AddChildView(focusable_view);
  widget->Show();

  {
    TouchEventHandler touch_event_handler(widget.get());
    ASSERT_TRUE(
        ui_controls::SendTouchEvents(ui_controls::kTouchPress, 1, 100, 100));
    touch_event_handler.WaitForEvents();
  }
}

#endif  // BUILDFLAG(IS_WIN)

// Tests mouse move outside of the window into the "resize controller" and back
// will still generate an OnMouseEntered and OnMouseExited event..
TEST_F(WidgetTestInteractive, CheckResizeControllerEvents) {
  std::unique_ptr<Widget> toplevel =
      base::WrapUnique(CreateTopLevelFramelessPlatformWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));

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
  ui::MouseEvent moved_out(ui::EventType::kMouseMoved, p1, p1,
                           ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_out);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the active view.
  gfx::Point p2(95, 95);
  ui::MouseEvent moved_over(ui::EventType::kMouseMoved, p2, p2,
                            ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the outer resizing border.
  gfx::Point p3(102, 95);
  ui::MouseEvent moved_resizer(ui::EventType::kMouseMoved, p3, p3,
                               ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_resizer);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(1, view->ExitedCalls());

  // Move onto the view again.
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());
}

// Test view focus restoration when a widget is deactivated and re-activated.
TEST_F(WidgetTestInteractive, ViewFocusOnWidgetActivationChanges) {
  std::unique_ptr<Widget> widget1 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  View* view1 =
      widget1->GetContentsView()->AddChildView(std::make_unique<View>());
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  std::unique_ptr<Widget> widget2 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  View* view2a = new View;
  View* view2b = new View;
  view2a->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view2b->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget2->GetContentsView()->AddChildView(view2a);
  widget2->GetContentsView()->AddChildView(view2b);

  ShowSync(widget1.get());
  EXPECT_TRUE(widget1->IsActive());
  view1->RequestFocus();
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());

  ShowSync(widget2.get());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(nullptr, widget1->GetFocusManager()->GetFocusedView());
  view2a->RequestFocus();
  EXPECT_EQ(view2a, widget2->GetFocusManager()->GetFocusedView());
  view2b->RequestFocus();
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget1.get());
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(nullptr, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget2.get());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(nullptr, widget1->GetFocusManager()->GetFocusedView());
}

TEST_F(WidgetTestInteractive, ZOrderCheckBetweenTopWindows) {
  std::unique_ptr<Widget> w1 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> w2 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> w3 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  ShowSync(w1.get());
  ShowSync(w2.get());
  ShowSync(w3.get());

  EXPECT_FALSE(w1->AsWidget()->IsStackedAbove(w2->AsWidget()->GetNativeView()));
  EXPECT_FALSE(w2->AsWidget()->IsStackedAbove(w3->AsWidget()->GetNativeView()));
  EXPECT_FALSE(w1->AsWidget()->IsStackedAbove(w3->AsWidget()->GetNativeView()));
  EXPECT_TRUE(w2->AsWidget()->IsStackedAbove(w1->AsWidget()->GetNativeView()));
  EXPECT_TRUE(w3->AsWidget()->IsStackedAbove(w2->AsWidget()->GetNativeView()));
  EXPECT_TRUE(w3->AsWidget()->IsStackedAbove(w1->AsWidget()->GetNativeView()));

  w2->AsWidget()->StackAboveWidget(w1->AsWidget());
  EXPECT_TRUE(w2->AsWidget()->IsStackedAbove(w1->AsWidget()->GetNativeView()));
  w1->AsWidget()->StackAboveWidget(w2->AsWidget());
  EXPECT_FALSE(w2->AsWidget()->IsStackedAbove(w1->AsWidget()->GetNativeView()));
}

// Test z-order of child widgets relative to their parent.
// TODO(crbug.com/40776787): Disabled on Mac due to flake
#if BUILDFLAG(IS_MAC)
#define MAYBE_ChildStackedRelativeToParent DISABLED_ChildStackedRelativeToParent
#else
#define MAYBE_ChildStackedRelativeToParent ChildStackedRelativeToParent
#endif
TEST_F(WidgetTestInteractive, MAYBE_ChildStackedRelativeToParent) {
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

TEST_F(WidgetTestInteractive, ChildWidgetStackAbove) {
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  auto children = std::to_array<Widget*>(
      {CreateChildPlatformWidget(toplevel->GetNativeView()),
       CreateChildPlatformWidget(toplevel->GetNativeView()),
       CreateChildPlatformWidget(toplevel->GetNativeView())});
  auto order = std::to_array<size_t>({0, 1, 2});

  static_assert(children.size() == order.size());

  children[0]->ShowInactive();
  children[1]->ShowInactive();
  children[2]->ShowInactive();
  ShowSync(toplevel.get());

  do {
    children[order[1]]->StackAboveWidget(children[order[0]]);
    children[order[2]]->StackAboveWidget(children[order[1]]);
    for (size_t i = 0; i < order.size(); i++) {
      for (size_t j = 0; j < order.size(); j++) {
        if (i < j) {
          EXPECT_FALSE(
              IsWindowStackedAbove(children[order[i]], children[order[j]]));
        } else if (i > j) {
          EXPECT_TRUE(
              IsWindowStackedAbove(children[order[i]], children[order[j]]));
        }
      }
    }
  } while (std::next_permutation(order.begin(), order.end()));
}

TEST_F(WidgetTestInteractive, ChildWidgetStackAtTop) {
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  auto children = std::to_array<Widget*>(
      {CreateChildPlatformWidget(toplevel->GetNativeView()),
       CreateChildPlatformWidget(toplevel->GetNativeView()),
       CreateChildPlatformWidget(toplevel->GetNativeView())});
  auto order = std::to_array<size_t>({0, 1, 2});

  static_assert(children.size() == order.size());

  children[0]->ShowInactive();
  children[1]->ShowInactive();
  children[2]->ShowInactive();
  ShowSync(toplevel.get());

  do {
    children[order[1]]->StackAtTop();
    children[order[2]]->StackAtTop();
    for (size_t i = 0; i < order.size(); i++) {
      for (size_t j = 0; j < order.size(); j++) {
        if (i < j) {
          EXPECT_FALSE(
              IsWindowStackedAbove(children[order[i]], children[order[j]]));
        } else if (i > j) {
          EXPECT_TRUE(
              IsWindowStackedAbove(children[order[i]], children[order[j]]));
        }
      }
    }
  } while (std::next_permutation(order.begin(), order.end()));
}

#if BUILDFLAG(IS_WIN)

// Test view focus retention when a widget's HWND is disabled and re-enabled.
TEST_F(WidgetTestInteractive, ViewFocusOnHWNDEnabledChanges) {
  WidgetAutoclosePtr widget(CreateTopLevelFramelessPlatformWidget());
  widget->SetContentsView(std::make_unique<View>());
  for (size_t i = 0; i < 2; ++i) {
    auto child = std::make_unique<View>();
    child->SetFocusBehavior(View::FocusBehavior::ALWAYS);
    widget->GetContentsView()->AddChildView(std::move(child));
  }

  widget->Show();
  widget->GetNativeWindow()->GetHost()->Show();
  const HWND hwnd = HWNDForWidget(widget.get());
  EXPECT_TRUE(::IsWindow(hwnd));
  EXPECT_TRUE(::IsWindowEnabled(hwnd));
  EXPECT_EQ(hwnd, ::GetActiveWindow());

  for (View* view : widget->GetContentsView()->children()) {
    SCOPED_TRACE("Child view " +
                 base::NumberToString(
                     widget->GetContentsView()->GetIndexOf(view).value()));

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

  WidgetActivationTest(const WidgetActivationTest&) = delete;
  WidgetActivationTest& operator=(const WidgetActivationTest&) = delete;

  ~WidgetActivationTest() override = default;

  bool OnNativeWidgetActivationChanged(bool active) override {
    active_ = active;
    return true;
  }

  bool active() const { return active_; }

 private:
  bool active_ = false;
};

// Tests whether the widget only becomes active when the underlying window
// is really active.
TEST_F(WidgetTestInteractive, WidgetNotActivatedOnFakeActivationMessages) {
  auto widget1 = std::make_unique<WidgetActivationTest>();
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.native_widget = new DesktopNativeWidgetAura(widget1.get());
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget1->Init(std::move(init_params));
  widget1->Show();
  EXPECT_EQ(true, widget1->active());

  auto widget2 = std::make_unique<WidgetActivationTest>();
  init_params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.native_widget = new DesktopNativeWidgetAura(widget2.get());
  widget2->Init(std::move(init_params));
  widget2->Show();
  EXPECT_EQ(true, widget2->active());
  EXPECT_EQ(false, widget1->active());

  HWND win32_native_window1 = HWNDForWidget(widget1.get());
  EXPECT_TRUE(::IsWindow(win32_native_window1));

  ::SendMessage(win32_native_window1, WM_NCACTIVATE, 1, 0);
  EXPECT_EQ(false, widget1->active());
  EXPECT_EQ(true, widget2->active());

  ::SetActiveWindow(win32_native_window1);
  EXPECT_EQ(true, widget1->active());
  EXPECT_EQ(false, widget2->active());
}

// On Windows if we create a fullscreen window on a thread, then it affects the
// way other windows on the thread interact with the taskbar. To workaround
// this we reduce the bounds of a fullscreen window by 1px when it loses
// activation. This test verifies the same.
TEST_F(WidgetTestInteractive, FullscreenBoundsReducedOnActivationLoss) {
  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new DesktopNativeWidgetAura(widget1.get());
  widget1->Init(std::move(params));
  widget1->SetBounds(gfx::Rect(0, 0, 200, 200));
  widget1->Show();

  widget1->Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  widget1->SetFullscreen(true);
  EXPECT_TRUE(widget1->IsFullscreen());
  // Ensure that the StopIgnoringPosChanges task in HWNDMessageHandler runs.
  // This task is queued when a widget becomes fullscreen.
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  gfx::Rect fullscreen_bounds = widget1->GetWindowBoundsInScreen();

  auto widget2 = std::make_unique<Widget>();
  params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                        Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new DesktopNativeWidgetAura(widget2.get());
  widget2->Init(std::move(params));
  widget2->SetBounds(gfx::Rect(0, 0, 200, 200));
  widget2->Show();

  widget2->Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget2->GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  gfx::Rect fullscreen_bounds_after_activation_loss =
      widget1->GetWindowBoundsInScreen();

  // After deactivation loss the bounds of the fullscreen widget should be
  // reduced by 1px.
  EXPECT_EQ(fullscreen_bounds.height() -
                fullscreen_bounds_after_activation_loss.height(),
            1);

  widget1->Activate();
  RunPendingMessages();
  EXPECT_EQ(::GetActiveWindow(),
            widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());

  gfx::Rect fullscreen_bounds_after_activate =
      widget1->GetWindowBoundsInScreen();

  // After activation the bounds of the fullscreen widget should be restored.
  EXPECT_EQ(fullscreen_bounds, fullscreen_bounds_after_activate);
}

// Ensure the window rect and client rects are correct with a window that was
// maximized.
TEST_F(WidgetTestInteractive, FullscreenMaximizedWindowBounds) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  widget->set_frame_type(Widget::FrameType::kForceCustom);
  widget->Init(std::move(params));
  widget->SetBounds(gfx::Rect(0, 0, 200, 200));
  widget->Show();

  widget->Maximize();
  EXPECT_TRUE(widget->IsMaximized());

  widget->SetFullscreen(true);
  EXPECT_TRUE(widget->IsFullscreen());
  EXPECT_FALSE(widget->IsMaximized());
  // Ensure that the StopIgnoringPosChanges task in HWNDMessageHandler runs.
  // This task is queued when a widget becomes fullscreen.
  RunPendingMessages();

  aura::WindowTreeHost* host = widget->GetNativeWindow()->GetHost();
  HWND hwnd = host->GetAcceleratedWidget();

  HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  ASSERT_TRUE(!!monitor);
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  ASSERT_TRUE(::GetMonitorInfo(monitor, &monitor_info));

  gfx::Rect monitor_bounds(monitor_info.rcMonitor);
  gfx::Rect window_bounds = widget->GetWindowBoundsInScreen();
  gfx::Rect client_area_bounds = host->GetBoundsInPixels();

  EXPECT_EQ(window_bounds, monitor_bounds);
  EXPECT_EQ(monitor_bounds, client_area_bounds);

  // Setting not fullscreen should return it to maximized.
  widget->SetFullscreen(false);
  EXPECT_FALSE(widget->IsFullscreen());
  EXPECT_TRUE(widget->IsMaximized());

  client_area_bounds = host->GetBoundsInPixels();
  EXPECT_TRUE(monitor_bounds.Contains(client_area_bounds));
  EXPECT_NE(monitor_bounds, client_area_bounds);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
// Tests whether the focused window is set correctly when a modal window is
// created and destroyed. When it is destroyed it should focus the owner window.
TEST_F(DesktopWidgetTestInteractive, WindowModalWindowDestroyedActivationTest) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);
  const std::vector<gfx::NativeView>& focus_changes =
      focus_listener.focus_changes();

  // Create a top level widget.
  auto top_level_widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::mojom::WindowShowState::kNormal;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  top_level_widget->Init(std::move(init_params));
  ShowSync(top_level_widget.get());

  gfx::NativeView top_level_native_view = top_level_widget->GetNativeView();
  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(1u, focus_changes.size());
  EXPECT_EQ(top_level_native_view, focus_changes[0]);

  // Create a modal dialog.
  auto dialog_delegate = std::make_unique<DialogDelegateView>();
  dialog_delegate->SetModalType(ui::mojom::ModalType::kWindow);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate.release(), nullptr, top_level_widget->GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));

  // Note the dialog widget doesn't need a ShowSync. Since it is modal, it gains
  // active status synchronously, even on Mac.
  modal_dialog_widget->Show();

  gfx::NativeView modal_native_view = modal_dialog_widget->GetNativeView();
  ASSERT_EQ(3u, focus_changes.size());
  EXPECT_EQ(gfx::NativeView(), focus_changes[1]);
  EXPECT_EQ(modal_native_view, focus_changes[2]);

#if BUILDFLAG(IS_MAC)
  // Window modal dialogs on Mac are "sheets", which animate to close before
  // activating their parent widget.
  modal_dialog_widget->Close();
  views::test::WaitForWidgetActive(top_level_widget.get(), true);
#else
  views::test::WidgetDestroyedWaiter waiter(modal_dialog_widget);
  modal_dialog_widget->Close();
  waiter.Wait();
#endif

  ASSERT_EQ(5u, focus_changes.size());
  EXPECT_EQ(gfx::NativeView(), focus_changes[3]);
  EXPECT_EQ(top_level_native_view, focus_changes[4]);

  top_level_widget->Close();
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}
#endif

TEST_F(DesktopWidgetTestInteractive, CanActivateFlagIsHonored) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.activatable = Widget::InitParams::Activatable::kNo;
  widget->Init(std::move(init_params));

  widget->Show();
  EXPECT_FALSE(widget->IsActive());
}

#if defined(USE_AURA)

// Test that touch selection quick menu is not activated when opened.
TEST_F(DesktopWidgetTestInteractive, TouchSelectionQuickMenuIsNotActivated) {
  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());
  widget->SetBounds(gfx::Rect(0, 0, 200, 200));

  std::unique_ptr<Textfield> textfield = CreateTextfield();
  auto* const textfield_ptr = textfield.get();
  textfield_ptr->SetBounds(0, 0, 200, 20);
  textfield_ptr->SetText(u"some text");
  widget->GetRootView()->AddChildView(std::move(textfield));

  ShowSync(widget.get());
  textfield_ptr->RequestFocus();
  textfield_ptr->SelectAll(true);
  TextfieldTestApi textfield_test_api(textfield_ptr);

  ui::test::EventGenerator generator(GetRootWindow(widget.get()));
  generator.GestureTapAt(textfield_ptr->GetBoundsInScreen().origin() +
                         gfx::Vector2d(10, 10));
  // The touch selection controller must be created in response to tapping.
  ASSERT_TRUE(textfield_test_api.touch_selection_controller());
  static_cast<TouchSelectionControllerImpl*>(
      textfield_test_api.touch_selection_controller())
      ->ShowQuickMenuImmediatelyForTesting();

  EXPECT_TRUE(textfield_ptr->HasFocus());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN)
TEST_F(DesktopWidgetTestInteractive, DisableViewDoesNotActivateWidget) {
#else
TEST_F(WidgetTestInteractive, DisableViewDoesNotActivateWidget) {
#endif  // !BUILDFLAG(IS_WIN)

  // Create first widget and view, activate the widget, and focus the view.
  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params1 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params1.activatable = Widget::InitParams::Activatable::kYes;
  widget1->Init(std::move(params1));

  View* view1 = widget1->GetRootView()->AddChildView(std::make_unique<View>());
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  widget1->Show();
  ActivateSync(widget1.get());

  FocusManager* focus_manager1 = widget1->GetFocusManager();
  ASSERT_TRUE(focus_manager1);
  focus_manager1->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager1->GetFocusedView());

  // Create second widget and view, activate the widget, and focus the view.
  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params2 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  params2.activatable = Widget::InitParams::Activatable::kYes;
  widget2->Init(std::move(params2));

  View* view2 = widget2->GetRootView()->AddChildView(std::make_unique<View>());
  view2->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  widget2->Show();
  ActivateSync(widget2.get());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget1->IsActive());

  FocusManager* focus_manager2 = widget2->GetFocusManager();
  ASSERT_TRUE(focus_manager2);
  focus_manager2->SetFocusedView(view2);
  EXPECT_EQ(view2, focus_manager2->GetFocusedView());

  // Disable the first view and make sure it loses focus, but its widget is not
  // activated.
  view1->SetEnabled(false);
  EXPECT_NE(view1, focus_manager1->GetFocusedView());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());
}

TEST_F(WidgetTestInteractive, ShowCreatesActiveWindow) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  ShowSync(widget.get());
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kNormal);
}

TEST_F(WidgetTestInteractive, ShowInactive) {
  WidgetTest::WaitForSystemAppActivation();
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  ShowInactiveSync(widget.get());
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kInactive);
}

TEST_F(WidgetTestInteractive, InactiveBeforeShow) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->IsVisible());

  ShowSync(widget.get());

  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(WidgetTestInteractive, ShowInactiveAfterShow) {
  // Create 2 widgets to ensure window layering does not change.
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> widget2 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  ShowSync(widget2.get());
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget2->IsVisible());
  EXPECT_TRUE(widget2->IsActive());

  ShowSync(widget.get());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  ShowInactiveSync(widget.get());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kNormal);
}

TEST_F(WidgetTestInteractive, ShowAfterShowInactive) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->SetBounds(gfx::Rect(100, 100, 100, 100));

  ShowInactiveSync(widget.get());
  ShowSync(widget.get());
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kNormal);
}

TEST_F(WidgetTestInteractive, WidgetShouldBeActiveWhenShow) {
  // TODO(crbug.com/40185137): This test fails if put under
  // NativeWidgetAuraTest.
  std::unique_ptr<Widget> anchor_widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  anchor_widget->Show();
  test::WaitForWidgetActive(anchor_widget.get(), true);
  EXPECT_TRUE(anchor_widget->IsActive());
#if !BUILDFLAG(IS_MAC)
  EXPECT_TRUE(anchor_widget->GetNativeWindow()->HasFocus());
#endif
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
TEST_F(WidgetTestInteractive, InactiveWidgetDoesNotGrabActivation) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  ShowSync(widget.get());
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kNormal);

  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget2->Init(std::move(params));
  widget2->Show();
  RunPendingMessagesForActiveStatusChange();

  EXPECT_EQ(GetWidgetShowState(widget2.get()),
            ui::mojom::WindowShowState::kInactive);
  EXPECT_EQ(GetWidgetShowState(widget.get()),
            ui::mojom::WindowShowState::kNormal);
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)

// ExitFullscreenRestoreState doesn't use DesktopAura widgets. On Mac, there are
// currently only Desktop widgets and fullscreen changes have to coordinate with
// the OS. See BridgedNativeWidgetUITest for native Mac fullscreen tests.
// Maximize on mac is also (intentionally) a no-op.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ExitFullscreenRestoreState DISABLED_ExitFullscreenRestoreState
#else
#define MAYBE_ExitFullscreenRestoreState ExitFullscreenRestoreState
#endif

// Test that window state is not changed after getting out of full screen.
TEST_F(WidgetTestInteractive, MAYBE_ExitFullscreenRestoreState) {
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));

  toplevel->Show();
  RunPendingMessages();

  // This should be a normal state window.
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal,
            GetWidgetShowState(toplevel.get()));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::mojom::WindowShowState::kFullscreen,
            GetWidgetShowState(toplevel.get()));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::mojom::WindowShowState::kFullscreen,
            GetWidgetShowState(toplevel.get()));

  // And it should still be in normal state after getting out of full screen.
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal,
            GetWidgetShowState(toplevel.get()));

  // Now, make it maximized.
  toplevel->Maximize();
  EXPECT_EQ(ui::mojom::WindowShowState::kMaximized,
            GetWidgetShowState(toplevel.get()));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::mojom::WindowShowState::kFullscreen,
            GetWidgetShowState(toplevel.get()));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::mojom::WindowShowState::kFullscreen,
            GetWidgetShowState(toplevel.get()));

  // And it stays maximized after getting out of full screen.
  EXPECT_EQ(ui::mojom::WindowShowState::kMaximized,
            GetWidgetShowState(toplevel.get()));
}

// Testing initial focus is assigned properly for normal top-level widgets,
// and subclasses that specify a initially focused child view.
TEST_F(WidgetTestInteractive, InitialFocus) {
  // By default, there is no initially focused view (even if there is a
  // focusable subview).
  std::unique_ptr<Widget> toplevel = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  View* view = new View;
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  toplevel->GetContentsView()->AddChildView(view);

  ShowSync(toplevel.get());
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
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  ShowSync(widget.get());
  ASSERT_FALSE(widget->IsMinimized());

  PropertyWaiter minimize_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_waiter.Wait());

  PropertyWaiter restore_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      false);
  widget->Restore();
  EXPECT_TRUE(restore_waiter.Wait());
}

// Maximize is not implemented on macOS, see crbug.com/868599
#if !BUILDFLAG(IS_MAC)
// Widget::Show/ShowInactive should not restore a maximized window
TEST_F(DesktopWidgetTestInteractive, ShowAfterMaximize) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  ShowSync(widget.get());
  ASSERT_FALSE(widget->IsMaximized());

  PropertyWaiter maximize_waiter(
      base::BindRepeating(&Widget::IsMaximized, base::Unretained(widget.get())),
      true);
  widget->Maximize();
  EXPECT_TRUE(maximize_waiter.Wait());

  ShowSync(widget.get());
  EXPECT_TRUE(widget->IsMaximized());

  ShowInactiveSync(widget.get());
  EXPECT_TRUE(widget->IsMaximized());
}
#endif

#if BUILDFLAG(IS_WIN)
// TODO(davidbienvenu): Get this test to pass on Linux and ChromeOS by hiding
// the root window when desktop widget is minimized.
// Tests that root window visibility toggles correctly when the desktop widget
// is minimized and maximized on Windows, and the Widget remains visible.
TEST_F(DesktopWidgetTestInteractive, RestoreAndMinimizeVisibility) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  aura::Window* root_window = GetRootWindow(widget.get());
  ShowSync(widget.get());
  ASSERT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(root_window->IsVisible());

  PropertyWaiter minimize_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(root_window->IsVisible());

  PropertyWaiter restore_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      false);
  widget->Restore();
  EXPECT_TRUE(restore_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(root_window->IsVisible());
}

// Test that focus is restored to the widget after a minimized window
// is activated.
TEST_F(DesktopWidgetTestInteractive, MinimizeAndActivateFocus) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  aura::Window* root_window = GetRootWindow(widget.get());
  auto* widget_window = widget->GetNativeWindow();
  ShowSync(widget.get());
  ASSERT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(root_window->IsVisible());
  widget_window->Focus();
  EXPECT_TRUE(widget_window->HasFocus());
  widget->GetContentsView()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget->GetContentsView()->RequestFocus();
  EXPECT_TRUE(widget->GetContentsView()->HasFocus());

  PropertyWaiter minimize_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      true);
  widget->Minimize();
  EXPECT_TRUE(minimize_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(root_window->IsVisible());

  PropertyWaiter restore_widget_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget.get())),
      false);
  widget->Activate();
  EXPECT_TRUE(widget->GetContentsView()->HasFocus());
  EXPECT_TRUE(restore_widget_waiter.Wait());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(root_window->IsVisible());
  EXPECT_TRUE(widget_window->CanFocus());
}

class SyntheticMouseMoveCounter : public ui::EventHandler {
 public:
  explicit SyntheticMouseMoveCounter(Widget* widget) : widget_(widget) {
    widget_->GetNativeWindow()->AddPreTargetHandler(this);
  }

  SyntheticMouseMoveCounter(const SyntheticMouseMoveCounter&) = delete;
  SyntheticMouseMoveCounter& operator=(const SyntheticMouseMoveCounter&) =
      delete;

  ~SyntheticMouseMoveCounter() override {
    widget_->GetNativeWindow()->RemovePreTargetHandler(this);
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMouseMoved && event->IsSynthesized()) {
      ++count_;
    }
  }

  int num_synthetic_mouse_moves() const { return count_; }

 private:
  int count_ = 0;
  raw_ptr<Widget> widget_;
};

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
TEST_F(DesktopWidgetTestInteractive,
       // TODO(crbug.com/335767870): Re-enable this test
       DISABLED_DoNotSynthesizeMouseMoveOnVisibilityChangeIfOccluded) {
  // Create a top-level widget.
  std::unique_ptr<Widget> widget_below =
      base::WrapUnique(CreateTopLevelPlatformDesktopWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  widget_below->SetBounds(gfx::Rect(300, 300));
  widget_below->Show();

  // Dispatch a mouse event to place cursor inside window bounds.
  base::RunLoop run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(150, 150, run_loop.QuitClosure());
  run_loop.Run();

  // Create a child widget.
  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  child_params.parent = widget_below->GetNativeView();
  child_params.context = widget_below->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(300, 300));
  child->Show();
  base::RunLoop().RunUntilIdle();

  SyntheticMouseMoveCounter counter_below(widget_below.get());
  EXPECT_EQ(0, counter_below.num_synthetic_mouse_moves());

  // Update the child window's visibility. This should trigger a synthetic
  // mouse move event.
  child->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter_below.num_synthetic_mouse_moves());

  // Occlude the existing widget with a new top-level widget.
  std::unique_ptr<Widget> widget_above =
      base::WrapUnique(CreateTopLevelPlatformDesktopWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  widget_above->SetBounds(gfx::Rect(300, 300));
  widget_above->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget_above->AsWidget()->IsStackedAbove(
      widget_below->AsWidget()->GetNativeView()));

  // Update the child window's visibility again, but this should not trigger a
  // synthetic mouse move event, since there's another widget under the cursor.
  child->Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter_below.num_synthetic_mouse_moves());
}
#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
// Tests that minimizing a widget causes the gesture_handler
// to be cleared when the widget is minimized.
TEST_F(DesktopWidgetTestInteractive, EventHandlersClearedOnWidgetMinimize) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  ShowSync(widget.get());
  ASSERT_FALSE(widget->IsMinimized());
  View mouse_handler_view;
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  // This also sets the gesture_handler, and we'll verify that it
  // gets cleared when the widget is minimized.
  root_view->SetMouseAndGestureHandler(&mouse_handler_view);
  EXPECT_TRUE(GetGestureHandler(root_view));

  widget->Minimize();
  {
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        true);
    EXPECT_TRUE(minimize_waiter.Wait());
  }
  EXPECT_FALSE(GetGestureHandler(root_view));
}
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
// Tests that when a desktop native widget has modal transient child, it should
// avoid restore focused view itself as the modal transient child window will do
// that, thus avoids having multiple focused view visually (crbug.com/727641).
TEST_F(DesktopWidgetTestInteractive,
       DesktopNativeWidgetWithModalTransientChild) {
  // Create a desktop native Widget for Widget::Deactivate().
  WidgetAutoclosePtr deactivate_widget(CreateTopLevelNativeWidget());
  ShowSync(deactivate_widget.get());

  // Create a top level desktop native widget.
  WidgetAutoclosePtr top_level(CreateTopLevelNativeWidget());

  std::unique_ptr<Textfield> textfield = CreateTextfield();
  auto* const textfield_ptr = textfield.get();
  textfield_ptr->SetBounds(0, 0, 200, 20);
  top_level->GetRootView()->AddChildView(std::move(textfield));
  ShowSync(top_level.get());
  textfield_ptr->RequestFocus();
  EXPECT_TRUE(textfield_ptr->HasFocus());

  // Create a modal dialog.
  // This instance will be destroyed when the dialog is destroyed.
  auto dialog_delegate = std::make_unique<DialogDelegateView>();
  dialog_delegate->SetModalType(ui::mojom::ModalType::kWindow);
  Widget* modal_dialog_widget = DialogDelegate::CreateDialogWidget(
      dialog_delegate.release(), nullptr, top_level->GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(0, 0, 100, 10));
  std::unique_ptr<Textfield> dialog_textfield = CreateTextfield();
  auto* const dialog_textfield_ptr = dialog_textfield.get();
  dialog_textfield_ptr->SetBounds(0, 0, 50, 5);
  modal_dialog_widget->GetRootView()->AddChildView(std::move(dialog_textfield));
  // Dialog widget doesn't need a ShowSync as it gains active status
  // synchronously.
  modal_dialog_widget->Show();
  dialog_textfield_ptr->RequestFocus();
  EXPECT_TRUE(dialog_textfield_ptr->HasFocus());
  EXPECT_FALSE(textfield_ptr->HasFocus());

  DeactivateSync(top_level.get());
  EXPECT_FALSE(dialog_textfield_ptr->HasFocus());
  EXPECT_FALSE(textfield_ptr->HasFocus());

  // After deactivation and activation of top level widget, only modal dialog
  // should restore focused view.
  ActivateSync(top_level.get());
  EXPECT_TRUE(dialog_textfield_ptr->HasFocus());
  EXPECT_FALSE(textfield_ptr->HasFocus());
}
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // BUILDFLAG(ENABLE_DESKTOP_AURA)

namespace {

// Helper class for CaptureLostTrackingWidget to store whether
// OnMouseCaptureLost has been invoked for a widget.
class CaptureLostState {
 public:
  CaptureLostState() = default;

  CaptureLostState(const CaptureLostState&) = delete;
  CaptureLostState& operator=(const CaptureLostState&) = delete;

  bool GetAndClearGotCaptureLost() {
    bool value = got_capture_lost_;
    got_capture_lost_ = false;
    return value;
  }

  void OnMouseCaptureLost() { got_capture_lost_ = true; }

 private:
  bool got_capture_lost_ = false;
};

// Used to verify OnMouseCaptureLost() has been invoked.
class CaptureLostTrackingWidget : public Widget {
 public:
  explicit CaptureLostTrackingWidget(CaptureLostState* capture_lost_state)
      : capture_lost_state_(capture_lost_state) {}

  CaptureLostTrackingWidget(const CaptureLostTrackingWidget&) = delete;
  CaptureLostTrackingWidget& operator=(const CaptureLostTrackingWidget&) =
      delete;

  // Widget:
  void OnMouseCaptureLost() override {
    capture_lost_state_->OnMouseCaptureLost();
    Widget::OnMouseCaptureLost();
  }

 private:
  // Weak. Stores whether OnMouseCaptureLost has been invoked for this widget.
  raw_ptr<CaptureLostState> capture_lost_state_;
};

}  // namespace

class WidgetCaptureTest : public DesktopWidgetTestInteractive {
 public:
  WidgetCaptureTest() = default;

  WidgetCaptureTest(const WidgetCaptureTest&) = delete;
  WidgetCaptureTest& operator=(const WidgetCaptureTest&) = delete;

  ~WidgetCaptureTest() override = default;

  // Verifies Widget::SetCapture() results in updating native capture along with
  // invoking the right Widget function.
  void TestCapture(bool use_desktop_native_widget) {
    std::unique_ptr<Widget> widget1 =
        std::make_unique<CaptureLostTrackingWidget>(capture_state1_.get());
    InitPlatformWidget(widget1.get(), use_desktop_native_widget);
    widget1->Show();

    std::unique_ptr<Widget> widget2 =
        std::make_unique<CaptureLostTrackingWidget>(capture_state2_.get());
    InitPlatformWidget(widget2.get(), use_desktop_native_widget);
    widget2->Show();

    // Set capture to widget2 and verity it gets it.
    widget2->SetCapture(widget2->GetRootView());
    EXPECT_FALSE(widget1->HasCapture());
    EXPECT_TRUE(widget2->HasCapture());
    EXPECT_FALSE(capture_state1_->GetAndClearGotCaptureLost());
    EXPECT_FALSE(capture_state2_->GetAndClearGotCaptureLost());

    // Set capture to widget1 and verify it gets it.
    widget1->SetCapture(widget1->GetRootView());
    EXPECT_TRUE(widget1->HasCapture());
    EXPECT_FALSE(widget2->HasCapture());
    EXPECT_FALSE(capture_state1_->GetAndClearGotCaptureLost());
    EXPECT_TRUE(capture_state2_->GetAndClearGotCaptureLost());

    // Release and verify no one has it.
    widget1->ReleaseCapture();
    EXPECT_FALSE(widget1->HasCapture());
    EXPECT_FALSE(widget2->HasCapture());
    EXPECT_TRUE(capture_state1_->GetAndClearGotCaptureLost());
    EXPECT_FALSE(capture_state2_->GetAndClearGotCaptureLost());
  }

  void InitPlatformWidget(Widget* widget, bool use_desktop_native_widget) {
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    // The test class by default returns DesktopNativeWidgetAura.
    params.native_widget =
        use_desktop_native_widget
            ? nullptr
            : CreatePlatformNativeWidgetImpl(widget, kDefault, nullptr);
    widget->Init(std::move(params));
  }

 protected:
  void SetUp() override {
    DesktopWidgetTestInteractive::SetUp();
    capture_state1_ = std::make_unique<CaptureLostState>();
    capture_state2_ = std::make_unique<CaptureLostState>();
  }

 private:
  std::unique_ptr<CaptureLostState> capture_state1_;
  std::unique_ptr<CaptureLostState> capture_state2_;
};

// See description in TestCapture().
TEST_F(WidgetCaptureTest, Capture) {
  TestCapture(false);
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
// See description in TestCapture(). Creates DesktopNativeWidget.
TEST_F(WidgetCaptureTest, CaptureDesktopNativeWidget) {
  TestCapture(true);
}
#endif

// Tests to ensure capture is correctly released from a Widget with capture when
// it is destroyed. Test for crbug.com/622201.
TEST_F(WidgetCaptureTest, DestroyWithCapture_CloseNow) {
  CaptureLostState capture_state;
  auto widget = std::make_unique<CaptureLostTrackingWidget>(&capture_state);
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
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
  auto widget = std::make_unique<CaptureLostTrackingWidget>(&capture_state);
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->Show();

  widget->SetCapture(widget->GetRootView());
  EXPECT_TRUE(widget->HasCapture());
  EXPECT_FALSE(capture_state.GetAndClearGotCaptureLost());
  widget->Close();
  EXPECT_TRUE(capture_state.GetAndClearGotCaptureLost());
}

// TODO(kylixrd): Rename this test once Widget ownership is normalized.
TEST_F(WidgetCaptureTest, DestroyWithCapture_ClientOwnsWidget) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  widget.Init(std::move(params));
  widget.Show();

  widget.SetCapture(widget.GetRootView());
  EXPECT_TRUE(widget.HasCapture());
}

// Test that no state is set if capture fails.
TEST_F(WidgetCaptureTest, FailedCaptureRequestIsNoop) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(400, 400);
  widget->Init(std::move(params));

  auto contents_view = std::make_unique<View>();
  MouseView* mouse_view1 =
      contents_view->AddChildView(std::make_unique<MouseView>());
  MouseView* mouse_view2 =
      contents_view->AddChildView(std::make_unique<MouseView>());
  widget->SetContentsView(std::move(contents_view));

  mouse_view1->SetBounds(0, 0, 200, 400);
  mouse_view2->SetBounds(200, 0, 200, 400);

  // Setting capture should fail because |widget| is not visible.
  widget->SetCapture(mouse_view1);
  EXPECT_FALSE(widget->HasCapture());

  widget->Show();
  ui::test::EventGenerator generator(GetRootWindow(widget.get()),
                                     widget->GetNativeWindow());
  generator.set_current_screen_location(
      widget->GetClientAreaBoundsInScreen().CenterPoint());
  generator.PressLeftButton();

  EXPECT_FALSE(mouse_view1->pressed());
  EXPECT_TRUE(mouse_view2->pressed());
}

TEST_F(WidgetCaptureTest, CaptureAutoReset) {
  std::unique_ptr<Widget> toplevel =
      base::WrapUnique(CreateTopLevelFramelessPlatformWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  toplevel->SetContentsView(std::make_unique<View>());

  EXPECT_FALSE(toplevel->HasCapture());
  toplevel->SetCapture(nullptr);
  EXPECT_TRUE(toplevel->HasCapture());

  // By default, mouse release removes capture.
  gfx::Point click_location(45, 15);
  ui::MouseEvent release(ui::EventType::kMouseReleased, click_location,
                         click_location, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
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
}

TEST_F(WidgetCaptureTest, ResetCaptureOnGestureEnd) {
  std::unique_ptr<Widget> toplevel =
      base::WrapUnique(CreateTopLevelFramelessPlatformWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  View* container = toplevel->SetContentsView(std::make_unique<View>());

  View* gesture = new GestureCaptureView;
  gesture->SetBounds(0, 0, 30, 30);
  container->AddChildView(gesture);

  MouseView* mouse = new MouseView;
  mouse->SetBounds(30, 0, 30, 30);
  container->AddChildView(mouse);

  toplevel->SetSize(gfx::Size(100, 100));
  toplevel->Show();

  // Start a gesture on |gesture|.
  ui::GestureEvent tap_down(
      15, 15, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  ui::GestureEvent end(15, 15, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::EventType::kGestureEnd));
  toplevel->OnGestureEvent(&tap_down);

  // Now try to click on |mouse|. Since |gesture| will have capture, |mouse|
  // will not receive the event.
  gfx::Point click_location(45, 15);

  ui::MouseEvent press(ui::EventType::kMousePressed, click_location,
                       click_location, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::EventType::kMouseReleased, click_location,
                         click_location, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);

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
}

// Checks that if a mouse-press triggers a capture on a different widget (which
// consumes the mouse-release event), then the target of the press does not have
// capture.
TEST_F(WidgetCaptureTest, DisableCaptureWidgetFromMousePress) {
  // The test creates two widgets: |first| and |second|.
  // The View in |first| makes |second| visible, sets capture on it, and starts
  // a nested loop (like a menu does). The View in |second| terminates the
  // nested loop and closes the widget.
  // The test sends a mouse-press event to |first|, and posts a task to send a
  // release event to |second|, to make sure that the release event is
  // dispatched after the nested loop starts.

  std::unique_ptr<Widget> first =
      base::WrapUnique(CreateTopLevelFramelessPlatformWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Widget> second =
      base::WrapUnique(CreateTopLevelFramelessPlatformWidget(
          Widget::InitParams::CLIENT_OWNS_WIDGET));
  Widget* second_ptr = second.get();

  NestedLoopCaptureView* container = first->SetContentsView(
      std::make_unique<NestedLoopCaptureView>(std::move(second)));

  second_ptr->SetContentsView(
      std::make_unique<ExitLoopOnRelease>(container->GetQuitClosure()));

  first->SetSize(gfx::Size(100, 100));
  first->Show();

  gfx::Point location(20, 20);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Widget::OnMouseEvent, base::Unretained(second_ptr),
                     base::Owned(new ui::MouseEvent(
                         ui::EventType::kMouseReleased, location, location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON))));
  ui::MouseEvent press(ui::EventType::kMousePressed, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  first->OnMouseEvent(&press);
  EXPECT_FALSE(first->HasCapture());
}

// Tests some grab/ungrab events. Only one Widget can have capture at any given
// time.
TEST_F(WidgetCaptureTest, GrabUngrab) {
  std::unique_ptr<Widget> top_level =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  top_level->SetContentsView(std::make_unique<MouseView>());

  auto child1 = std::make_unique<Widget>();
  Widget::InitParams params1 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_CONTROL);
  params1.parent = top_level->GetNativeView();
  params1.bounds = gfx::Rect(10, 10, 100, 100);
  child1->Init(std::move(params1));
  child1->SetContentsView(std::make_unique<MouseView>());

  auto child2 = std::make_unique<Widget>();
  Widget::InitParams params2 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_CONTROL);
  params2.parent = top_level->GetNativeView();
  params2.bounds = gfx::Rect(110, 10, 100, 100);
  child2->Init(std::move(params2));
  child2->SetContentsView(std::make_unique<MouseView>());

  top_level->Show();
  RunPendingMessages();

  // Click on child1.
  ui::test::EventGenerator generator(GetRootWindow(top_level.get()),
                                     child1->GetNativeWindow());
  generator.set_current_screen_location(
      child1->GetClientAreaBoundsInScreen().CenterPoint());
  generator.PressLeftButton();

  EXPECT_FALSE(top_level->HasCapture());
  EXPECT_TRUE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(top_level->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  // Click on child2.
  generator.SetTargetWindow(child2->GetNativeWindow());
  generator.set_current_screen_location(
      child2->GetClientAreaBoundsInScreen().CenterPoint());
  generator.PressLeftButton();

  EXPECT_FALSE(top_level->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_TRUE(child2->HasCapture());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(top_level->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  // Click on top_level.
  generator.SetTargetWindow(top_level->GetNativeWindow());
  generator.set_current_screen_location(
      top_level->GetClientAreaBoundsInScreen().origin());
  generator.PressLeftButton();

  EXPECT_TRUE(top_level->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(top_level->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());
}

// Disabled on Mac. Desktop Mac doesn't have system modal windows since Carbon
// was deprecated. It does have application modal windows, but only Ash requests
// those.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SystemModalWindowReleasesCapture \
  DISABLED_SystemModalWindowReleasesCapture
#elif BUILDFLAG(IS_CHROMEOS_ASH)
// Investigate enabling for Chrome OS. It probably requires help from the window
// service.
#define MAYBE_SystemModalWindowReleasesCapture \
  DISABLED_SystemModalWindowReleasesCapture
#else
#define MAYBE_SystemModalWindowReleasesCapture SystemModalWindowReleasesCapture
#endif

// Test that when opening a system-modal window, capture is released.
TEST_F(WidgetCaptureTest, MAYBE_SystemModalWindowReleasesCapture) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);

  // Create a top level widget.
  auto top_level_widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::mojom::WindowShowState::kNormal;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  top_level_widget->Init(std::move(init_params));
  ShowSync(top_level_widget.get());

  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(top_level_widget->GetNativeView(),
            focus_listener.focus_changes().back());

  EXPECT_FALSE(top_level_widget->HasCapture());
  top_level_widget->SetCapture(nullptr);
  EXPECT_TRUE(top_level_widget->HasCapture());

  // Create a modal dialog.
  auto dialog_delegate = std::make_unique<DialogDelegateView>();
  dialog_delegate->SetModalType(ui::mojom::ModalType::kSystem);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate.release(), nullptr, top_level_widget->GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  ShowSync(modal_dialog_widget);

  EXPECT_FALSE(top_level_widget->HasCapture());
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

// Regression test for http://crbug.com/382421 (Linux-Aura issue).
// TODO(pkotwicz): Make test pass on CrOS and Windows.
// TODO(tapted): Investigate for toolkit-views on Mac http;//crbug.com/441064.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_MouseExitOnCaptureGrab DISABLED_MouseExitOnCaptureGrab
#else
#define MAYBE_MouseExitOnCaptureGrab MouseExitOnCaptureGrab
#endif

// Test that a synthetic mouse exit is sent to the widget which was handling
// mouse events when a different widget grabs capture. Except for Windows,
// which does not send a synthetic mouse exit.
TEST_F(WidgetCaptureTest, MAYBE_MouseExitOnCaptureGrab) {
  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params1 =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget1->Init(std::move(params1));
  MouseView* mouse_view1 =
      widget1->SetContentsView(std::make_unique<MouseView>());
  widget1->Show();
  widget1->SetBounds(gfx::Rect(300, 300));

  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params2 =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget2->Init(std::move(params2));
  widget2->Show();
  widget2->SetBounds(gfx::Rect(400, 0, 300, 300));

  ui::test::EventGenerator generator(GetRootWindow(widget1.get()));
  generator.set_current_screen_location(gfx::Point(100, 100));
  generator.MoveMouseBy(0, 0);

  EXPECT_EQ(1, mouse_view1->EnteredCalls());
  EXPECT_EQ(0, mouse_view1->ExitedCalls());

  widget2->SetCapture(nullptr);
  EXPECT_EQ(0, mouse_view1->EnteredCalls());
  // On Windows, Chrome doesn't synthesize a separate mouse exited event.
  // Instead, it uses ::TrackMouseEvent to get notified of the mouse leaving.
  // Calling SetCapture does not cause Windows to generate a WM_MOUSELEAVE
  // event. See WindowEventDispatcher::OnOtherRootGotCapture() for more info.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(0, mouse_view1->ExitedCalls());
#else
  EXPECT_EQ(1, mouse_view1->ExitedCalls());
#endif  // BUILDFLAG(IS_WIN)
}

namespace {

// Widget observer which grabs capture when the widget is activated.
class CaptureOnActivationObserver : public WidgetObserver {
 public:
  explicit CaptureOnActivationObserver(Widget* widget) {
    widget_observation_.Observe(widget);
  }

  CaptureOnActivationObserver(const CaptureOnActivationObserver&) = delete;
  CaptureOnActivationObserver& operator=(const CaptureOnActivationObserver&) =
      delete;

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
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace

// Test that setting capture on widget activation of a non-toplevel widget
// (e.g. a bubble on Linux) succeeds.
TEST_F(WidgetCaptureTest, SetCaptureToNonToplevel) {
  auto toplevel = std::make_unique<Widget>();
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  toplevel->Init(std::move(toplevel_params));
  toplevel->Show();

  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  child_params.parent = toplevel->GetNativeView();
  child_params.context = toplevel->GetNativeWindow();
  child->Init(std::move(child_params));

  CaptureOnActivationObserver observer(child.get());
  child->Show();

#if BUILDFLAG(IS_MAC)
  // On Mac, activation is asynchronous. A single trip to the runloop should be
  // sufficient. On Aura platforms, note that since the child widget isn't top-
  // level, the aura window manager gets asked whether the widget is active, not
  // the OS.
  base::RunLoop().RunUntilIdle();
#endif

  EXPECT_TRUE(observer.activation_observed());
  EXPECT_TRUE(child->HasCapture());
}

#if BUILDFLAG(IS_WIN)
namespace {

// Used to verify OnMouseEvent() has been invoked.
class MouseEventTrackingWidget : public Widget {
 public:
  MouseEventTrackingWidget() = default;

  MouseEventTrackingWidget(const MouseEventTrackingWidget&) = delete;
  MouseEventTrackingWidget& operator=(const MouseEventTrackingWidget&) = delete;

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
};

}  // namespace

// Verifies if a mouse event is received on a widget that doesn't have capture
// on Windows that it is correctly processed by the widget that doesn't have
// capture. This behavior is not desired on OSes other than Windows.
TEST_F(WidgetCaptureTest, MouseEventDispatchedToRightWindow) {
  auto widget1 = std::make_unique<MouseEventTrackingWidget>();
  Widget::InitParams params1 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  // Not setting bounds on Win64 Arm results in a 0 height window, which
  // won't get mouse events. See https://crbug.com/1418180.
  params1.bounds = gfx::Rect(0, 0, 200, 200);
  params1.native_widget = new DesktopNativeWidgetAura(widget1.get());
  widget1->Init(std::move(params1));
  widget1->Show();

  auto widget2 = std::make_unique<MouseEventTrackingWidget>();
  Widget::InitParams params2 = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params2.bounds = gfx::Rect(0, 0, 200, 200);
  params2.native_widget = new DesktopNativeWidgetAura(widget2.get());
  widget2->Init(std::move(params2));
  widget2->Show();

  // Set capture to widget2 and verity it gets it.
  widget2->SetCapture(widget2->GetRootView());
  EXPECT_FALSE(widget1->HasCapture());
  EXPECT_TRUE(widget2->HasCapture());

  widget1->GetAndClearGotMouseEvent();
  widget2->GetAndClearGotMouseEvent();
  // Send a mouse event to the RootWindow associated with |widget1|. Even though
  // |widget2| has capture, |widget1| should still get the event.
  ui::MouseEvent mouse_event(ui::EventType::kMouseExited, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE,
                             ui::EF_NONE);
  ui::EventDispatchDetails details =
      widget1->GetNativeWindow()->GetHost()->GetEventSink()->OnEventFromSource(
          &mouse_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(widget1->GetAndClearGotMouseEvent());
  EXPECT_FALSE(widget2->GetAndClearGotMouseEvent());
}
#endif  // BUILDFLAG(IS_WIN)

class WidgetInputMethodInteractiveTest : public DesktopWidgetTestInteractive {
 public:
  WidgetInputMethodInteractiveTest() = default;

  WidgetInputMethodInteractiveTest(const WidgetInputMethodInteractiveTest&) =
      delete;
  WidgetInputMethodInteractiveTest& operator=(
      const WidgetInputMethodInteractiveTest&) = delete;

  // testing::Test:
  void SetUp() override {
    DesktopWidgetTestInteractive::SetUp();
#if BUILDFLAG(IS_WIN)
    // On Windows, Widget::Deactivate() works by activating the next topmost
    // window on the z-order stack. This only works if there is at least one
    // other window, so make sure that is the case.
    deactivate_widget_ = base::WrapUnique(
        CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
    deactivate_widget_->Show();
#endif
  }

  void TearDown() override {
    if (deactivate_widget_)
      deactivate_widget_->CloseNow();
    DesktopWidgetTestInteractive::TearDown();
  }

 private:
  std::unique_ptr<Widget> deactivate_widget_;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_Activation DISABLED_Activation
#else
#define MAYBE_Activation Activation
#endif
// Test input method focus changes affected by top window activaction.
TEST_F(WidgetInputMethodInteractiveTest, MAYBE_Activation) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  std::unique_ptr<Textfield> textfield = CreateTextfield();
  auto* const textfield_ptr = textfield.get();
  widget->GetRootView()->AddChildView(std::move(textfield));
  textfield_ptr->RequestFocus();

  ShowSync(widget.get());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget.get());

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
}

// Test input method focus changes affected by focus changes within 1 window.
TEST_F(WidgetInputMethodInteractiveTest, OneWindow) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  Textfield* const textfield1 =
      widget->GetRootView()->AddChildView(CreateTextfield());
  Textfield* const textfield2 =
      widget->GetRootView()->AddChildView(CreateTextfield());
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  ShowSync(widget.get());

  textfield1->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  textfield2->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

// Widget::Deactivate() doesn't work for CrOS, because it uses NWA instead of
// DNWA (which just activates the last active window) and involves the
// AuraTestHelper which sets the input method as DummyInputMethod.
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
  DeactivateSync(widget.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  ActivateSync(widget.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget.get());
  textfield1->RequestFocus();
  ActivateSync(widget.get());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());
#endif
}

// Test input method focus changes affected by focus changes cross 2 windows
// which shares the same top window.
TEST_F(WidgetInputMethodInteractiveTest, TwoWindows) {
  std::unique_ptr<Widget> parent = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  parent->SetBounds(gfx::Rect(100, 100, 100, 100));

  std::unique_ptr<Widget> child =
      base::WrapUnique(CreateChildNativeWidgetWithParent(
          parent.get(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  child->SetBounds(gfx::Rect(0, 0, 50, 50));
  child->Show();

  Textfield* const textfield_parent =
      parent->GetRootView()->AddChildView(CreateTextfield());
  Textfield* const textfield_child =
      child->GetRootView()->AddChildView(CreateTextfield());
  textfield_parent->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  ShowSync(parent.get());

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
#if BUILDFLAG(ENABLE_DESKTOP_AURA) || BUILDFLAG(IS_MAC)
  DeactivateSync(parent.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            parent->GetInputMethod()->GetTextInputType());

  textfield_parent->RequestFocus();
  DeactivateSync(parent.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent.get());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            parent->GetInputMethod()->GetTextInputType());
#endif
}

// Test input method focus changes affected by textfield's state changes.
TEST_F(WidgetInputMethodInteractiveTest, TextField) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  Textfield* const textfield =
      widget->GetRootView()->AddChildView(CreateTextfield());
  ShowSync(widget.get());
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
}

// Test input method should not work for accelerator.
TEST_F(WidgetInputMethodInteractiveTest, AcceleratorInTextfield) {
  std::unique_ptr<Widget> widget = base::WrapUnique(
      CreateTopLevelNativeWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  Textfield* const textfield =
      widget->GetRootView()->AddChildView(CreateTextfield());
  ShowSync(widget.get());
  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  textfield->RequestFocus();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_F,
                         ui::EF_ALT_DOWN);
  ui::Accelerator accelerator(key_event);
  widget->GetFocusManager()->RegisterAccelerator(
      accelerator, ui::AcceleratorManager::kNormalPriority, textfield);

  widget->OnKeyEvent(&key_event);
  EXPECT_TRUE(key_event.stopped_propagation());

  widget->GetFocusManager()->UnregisterAccelerators(textfield);

  ui::KeyEvent key_event2(key_event);
  widget->OnKeyEvent(&key_event2);
  EXPECT_FALSE(key_event2.stopped_propagation());
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)

class DesktopWidgetDragTestInteractive : public DesktopWidgetTestInteractive,
                                         public WidgetObserver {
 public:
  DesktopWidgetDragTestInteractive() = default;

  DesktopWidgetDragTestInteractive(const DesktopWidgetDragTestInteractive&) =
      delete;
  DesktopWidgetDragTestInteractive& operator=(
      const DesktopWidgetDragTestInteractive&) = delete;

  ~DesktopWidgetDragTestInteractive() override = default;

 protected:
  static constexpr gfx::Rect bounds = gfx::Rect(0, 0, 200, 200);

  void InitWidget(Widget* widget,
                  base::OnceClosure on_drag_enter,
                  base::OnceClosure on_drag_exit,
                  base::OnceClosure on_capture_lost) {
    widget->AddObserver(this);

    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.native_widget = new DesktopNativeWidgetAura(widget);
    params.bounds = bounds;
    widget->Init(std::move(params));

    // On X11 and Lacros, we need another mouse event after the drag has started
    // for `DragView::OnDragEntered()` to be called. The best way to wait for
    // the drag to start seems to be to wait for `DragView::OnMouseExited()`,
    // which on these platforms happens only after the drag has started.
    auto on_mouse_exit = base::BindLambdaForTesting([]() {
      gfx::Point target_location =
          aura::Env::GetInstance()->last_mouse_location();
      target_location += gfx::Vector2d(1, 1);
      EXPECT_TRUE(
          ui_controls::SendMouseMove(target_location.x(), target_location.y()));
    });

    widget->client_view()->AddChildView(std::make_unique<DragView>(
        std::move(on_drag_enter), std::move(on_drag_exit),
        std::move(on_capture_lost), std::move(on_mouse_exit)));

    // Update view layout to make sure `DragView` is sized correctly. Else it
    // might still have empty bounds when the drag event is received, preventing
    // it from receiving the event.
    widget->LayoutRootViewIfNecessary();

    ShowSync(widget);
  }

  void StartDrag() {
    // Move the mouse to the widget's center, press the left mouse button, and
    // drag the mouse a bit.
    gfx::Point start_location(bounds.width() / 2, bounds.height() / 2);
    gfx::Point target_location = start_location + gfx::Vector2d(10, 10);
    base::RunLoop move_loop;
    EXPECT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
        start_location.x(), start_location.y(), move_loop.QuitClosure()));
    move_loop.Run();
    base::RunLoop press_loop;
    EXPECT_TRUE(ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::MouseButton::LEFT, ui_controls::MouseButtonState::DOWN,
        press_loop.QuitClosure()));
    press_loop.Run();

    // `SendMouseMoveNotifyWhenDone()` might not call the closure until the drag
    // ends.
    EXPECT_TRUE(
        ui_controls::SendMouseMove(target_location.x(), target_location.y()));
  }

  void WaitForDragEnd() {
    drag_wait_loop_.Run();
    EXPECT_TRUE(drag_entered_);
  }

  bool drag_entered_ = false;

 private:
  // WidgetObserver:
  void OnWidgetDragComplete(Widget* widget) override { drag_wait_loop_.Quit(); }

  base::RunLoop drag_wait_loop_;
};

// Cancels a DnD session started by `RunShellDrag()`.
//
// TODO(crbug.com/332944429): Re-enable on Windows AMR64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_CancelShellDrag DISABLED_CancelShellDrag
#else
#define MAYBE_CancelShellDrag CancelShellDrag
#endif
TEST_F(DesktopWidgetDragTestInteractive, MAYBE_CancelShellDrag) {
  WidgetAutoclosePtr widget(new Widget);

  auto cancel = [&]() {
    drag_entered_ = true;

    widget->CancelShellDrag(widget->client_view());

#if BUILDFLAG(IS_WIN)
    // On Windows we can't just cancel the drag when we want, only the next time
    // the drag is updated. Send another mouse move to give us a chance to
    // cancel the drag.
    gfx::Point target_location =
        aura::Env::GetInstance()->last_mouse_location();
    target_location += gfx::Vector2d(1, 1);
    EXPECT_TRUE(
        ui_controls::SendMouseMove(target_location.x(), target_location.y()));
#endif  // BUILDFLAG(IS_WIN)
  };

  // See the comment in `DesktopWidgetDragTestInteractive::StartDrag()`.
#if BUILDFLAG(IS_WIN)
  base::OnceClosure on_capture_lost = base::BindLambdaForTesting(cancel);
#else
  base::OnceClosure on_capture_lost = base::DoNothing();
#endif  // BUILDFLAG(IS_WIN)

  InitWidget(widget.get(), /*on_drag_enter=*/base::BindLambdaForTesting(cancel),
             /*on_drag_exit=*/base::DoNothing(), std::move(on_capture_lost));

  StartDrag();

  // Wait for the drag to be cancelled by `DragView::OnDragEntered()` /
  // `DragView::OnMouseCaptureLost()`.
  WaitForDragEnd();
}

#endif  // BUILDFLAG(ENABLE_DESKTOP_AURA)

}  // namespace views::test
