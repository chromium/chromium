// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/window_occlusion_tracker_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_WIN)
#include <windows.h>

#include "ui/base/view_prop.h"
#include "ui/base/win/window_event_target.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {
namespace test {

using DesktopNativeWidgetAuraTest = DesktopWidgetTest;

// Verifies creating a Widget with a parent that is not in a RootWindow doesn't
// crash.
TEST_F(DesktopNativeWidgetAuraTest, CreateWithParentNotInRootWindow) {
  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_NOT_DRAWN);
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.parent = window.get();
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(params));
}

// Verifies that the Aura windows making up a widget instance have the correct
// bounds after the widget is resized.
TEST_F(DesktopNativeWidgetAuraTest, DesktopAuraWindowSizeTest) {
  Widget widget;

  // On Linux we test this with popup windows because the WM may ignore the size
  // suggestion for normal windows.
#if defined(OS_LINUX)
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
#else
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
#endif

  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  gfx::Rect bounds(0, 0, 100, 100);
  widget.SetBounds(bounds);
  widget.Show();

  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());

  gfx::Rect new_bounds(0, 0, 200, 200);
  widget.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->GetRootWindow()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(), widget.GetNativeView()->bounds().ToString());
  EXPECT_EQ(new_bounds.ToString(),
            widget.GetNativeView()->parent()->bounds().ToString());
}

// Verifies GetNativeView() is initially hidden. If the native view is initially
// shown then animations can not be disabled.
TEST_F(DesktopNativeWidgetAuraTest, NativeViewInitiallyHidden) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));
  EXPECT_FALSE(widget.GetNativeView()->IsVisible());
}

// Verifies that the native view isn't activated if Widget requires that.
TEST_F(DesktopNativeWidgetAuraTest, NativeViewNoActivate) {
  // Widget of TYPE_POPUP can't be activated.
  Widget widget;
  Widget::InitParams init_params = CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  EXPECT_FALSE(widget.CanActivate());
  EXPECT_EQ(nullptr, aura::client::GetFocusClient(widget.GetNativeWindow())
                         ->GetFocusedWindow());
}

#if defined(OS_WIN)
// Verifies that if the DesktopWindowTreeHost is already shown, the native view
// still reports not visible as we haven't shown the content window.
TEST_F(DesktopNativeWidgetAuraTest, WidgetNotVisibleOnlyWindowTreeHostShown) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));
  ShowWindow(widget.GetNativeView()->GetHost()->GetAcceleratedWidget(),
             SW_SHOWNORMAL);
  EXPECT_FALSE(widget.IsVisible());
}
#endif

TEST_F(DesktopNativeWidgetAuraTest, DesktopAuraWindowShowFrameless) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(init_params));

  // Make sure that changing frame type doesn't crash when there's no non-client
  // view.
  ASSERT_EQ(nullptr, widget.non_client_view());
  widget.DebugToggleFrameType();
  widget.Show();

#if defined(OS_WIN)
  // On Windows also make sure that handling WM_SYSCOMMAND doesn't crash with
  // custom frame. Frame type needs to be toggled again if Aero Glass is
  // disabled.
  if (widget.ShouldUseNativeFrame())
    widget.DebugToggleFrameType();
  SendMessage(widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
              WM_SYSCOMMAND, SC_RESTORE, 0);
#endif  // OS_WIN
}

#if defined(OS_CHROMEOS)
// TODO(crbug.com/916272): investigate fixing and enabling on Chrome OS.
#define MAYBE_GlobalCursorState DISABLED_GlobalCursorState
#else
#define MAYBE_GlobalCursorState GlobalCursorState
#endif

// Verify that the cursor state is shared between two native widgets.
TEST_F(DesktopNativeWidgetAuraTest, MAYBE_GlobalCursorState) {
  // Create two native widgets, each owning different root windows.
  Widget widget_a;
  Widget::InitParams init_params_a =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params_a.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_a.Init(std::move(init_params_a));

  Widget widget_b;
  Widget::InitParams init_params_b =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params_b.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_b.Init(std::move(init_params_b));

  aura::client::CursorClient* cursor_client_a = aura::client::GetCursorClient(
      widget_a.GetNativeView()->GetHost()->window());
  aura::client::CursorClient* cursor_client_b = aura::client::GetCursorClient(
      widget_b.GetNativeView()->GetHost()->window());

  // Verify the cursor can be locked using one client and unlocked using
  // another.
  EXPECT_FALSE(cursor_client_a->IsCursorLocked());
  EXPECT_FALSE(cursor_client_b->IsCursorLocked());

  cursor_client_a->LockCursor();
  EXPECT_TRUE(cursor_client_a->IsCursorLocked());
  EXPECT_TRUE(cursor_client_b->IsCursorLocked());

  cursor_client_b->UnlockCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorLocked());
  EXPECT_FALSE(cursor_client_b->IsCursorLocked());

  // Verify that mouse events can be disabled using one client and then
  // re-enabled using another. Note that disabling mouse events should also
  // have the side effect of making the cursor invisible.
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  cursor_client_b->DisableMouseEvents();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());
  EXPECT_FALSE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client_b->IsMouseEventsEnabled());

  cursor_client_a->EnableMouseEvents();
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  // Verify that setting the cursor using one cursor client
  // will set it for all root windows.
  EXPECT_EQ(ui::CursorType::kNone, cursor_client_a->GetCursor().native_type());
  EXPECT_EQ(ui::CursorType::kNone, cursor_client_b->GetCursor().native_type());

  cursor_client_b->SetCursor(ui::CursorType::kPointer);
  EXPECT_EQ(ui::CursorType::kPointer,
            cursor_client_a->GetCursor().native_type());
  EXPECT_EQ(ui::CursorType::kPointer,
            cursor_client_b->GetCursor().native_type());

  // Verify that hiding the cursor using one cursor client will
  // hide it for all root windows. Note that hiding the cursor
  // should not disable mouse events.
  cursor_client_a->HideCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());
  EXPECT_TRUE(cursor_client_a->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client_b->IsMouseEventsEnabled());

  // Verify that the visibility state cannot be changed using one
  // cursor client when the cursor was locked using another.
  cursor_client_b->LockCursor();
  cursor_client_a->ShowCursor();
  EXPECT_FALSE(cursor_client_a->IsCursorVisible());
  EXPECT_FALSE(cursor_client_b->IsCursorVisible());

  // Verify the cursor becomes visible on unlock (since a request
  // to make it visible was queued up while the cursor was locked).
  cursor_client_b->UnlockCursor();
  EXPECT_TRUE(cursor_client_a->IsCursorVisible());
  EXPECT_TRUE(cursor_client_b->IsCursorVisible());
}

// Verifies FocusController doesn't attempt to access |content_window_| during
// destruction. Previously the FocusController was destroyed after the window.
// This could be problematic as FocusController references |content_window_| and
// could attempt to use it after |content_window_| was destroyed. This test
// verifies this doesn't happen. Note that this test only failed under ASAN.
TEST_F(DesktopNativeWidgetAuraTest, DontAccessContentWindowDuringDestruction) {
  aura::test::TestWindowDelegate delegate;
  {
    Widget widget;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(std::move(init_params));

    // Owned by |widget|.
    aura::Window* window = new aura::Window(&delegate);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->Show();
    widget.GetNativeWindow()->parent()->AddChild(window);

    widget.Show();
  }
}

namespace {

std::unique_ptr<Widget> CreateAndShowControlWidget(aura::Window* parent) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent;
  params.native_widget = CreatePlatformNativeWidgetImpl(params, widget.get(),
                                                        kStubCapture, nullptr);
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

}  // namespace

#if defined(OS_CHROMEOS)
// TODO(crbug.com/916272): investigate fixing and enabling on Chrome OS.
#define MAYBE_ReorderDoesntRecomputeOcclusion \
  DISABLED_ReorderDoesntRecomputeOcclusion
#else
#define MAYBE_ReorderDoesntRecomputeOcclusion ReorderDoesntRecomputeOcclusion
#endif

TEST_F(DesktopNativeWidgetAuraTest, MAYBE_ReorderDoesntRecomputeOcclusion) {
  // Create the parent widget.
  Widget parent;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent.Init(std::move(init_params));
  parent.Show();

  aura::Window* parent_window = parent.GetNativeWindow();
  parent_window->TrackOcclusionState();

  View* contents_view = parent.GetContentsView();

  // Create child widgets.
  std::unique_ptr<Widget> w1(CreateAndShowControlWidget(parent_window));
  std::unique_ptr<Widget> w2(CreateAndShowControlWidget(parent_window));
  std::unique_ptr<Widget> w3(CreateAndShowControlWidget(parent_window));

  // Create child views.
  View* host_view1 = new View();
  w1->GetNativeView()->SetProperty(kHostViewKey, host_view1);
  contents_view->AddChildView(host_view1);

  View* host_view2 = new View();
  w2->GetNativeView()->SetProperty(kHostViewKey, host_view2);
  contents_view->AddChildView(host_view2);

  View* host_view3 = new View();
  w3->GetNativeView()->SetProperty(kHostViewKey, host_view3);
  contents_view->AddChildView(host_view3);

  // Reorder child views. Expect occlusion to only be recomputed once.
  aura::test::WindowOcclusionTrackerTestApi window_occlusion_tracker_test_api(
      aura::Env::GetInstance()->GetWindowOcclusionTracker());
  const int num_times_occlusion_recomputed =
      window_occlusion_tracker_test_api.GetNumTimesOcclusionRecomputed();
  contents_view->ReorderChildView(host_view3, 0);
  EXPECT_EQ(num_times_occlusion_recomputed + 1,
            window_occlusion_tracker_test_api.GetNumTimesOcclusionRecomputed());
}

void QuitNestedLoopAndCloseWidget(std::unique_ptr<Widget> widget,
                                  base::OnceClosure quit_runloop) {
  std::move(quit_runloop).Run();
}

// Verifies that a widget can be destroyed when running a nested message-loop.
TEST_F(DesktopNativeWidgetAuraTest, WidgetCanBeDestroyedFromNestedLoop) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  widget->Show();

  // Post a task that terminates the nested loop and destroyes the widget. This
  // task will be executed from the nested loop initiated with the call to
  // |RunWithDispatcher()| below.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QuitNestedLoopAndCloseWidget,
                                std::move(widget), run_loop.QuitClosure()));
  run_loop.Run();
}

// This class provides functionality to create fullscreen and top level popup
// windows. It additionally tests whether the destruction of these windows
// occurs correctly in desktop AURA without crashing.
// It provides facilities to test the following cases:-
// 1. Child window destroyed which should lead to the destruction of the
//    parent.
// 2. Parent window destroyed which should lead to the child being destroyed.
class DesktopAuraTopLevelWindowTest : public aura::WindowObserver {
 public:
  DesktopAuraTopLevelWindowTest() = default;

  ~DesktopAuraTopLevelWindowTest() override {
    EXPECT_TRUE(owner_destroyed_);
    EXPECT_TRUE(owned_window_destroyed_);
    top_level_widget_ = nullptr;
    owned_window_ = nullptr;
  }

  void CreateTopLevelWindow(const gfx::Rect& bounds, bool fullscreen) {
    Widget::InitParams init_params;
    init_params.type = Widget::InitParams::TYPE_WINDOW;
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.layer_type = ui::LAYER_NOT_DRAWN;
    init_params.accept_events = fullscreen;

    widget_.Init(std::move(init_params));

    owned_window_ = new aura::Window(&child_window_delegate_);
    owned_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    owned_window_->SetName("TestTopLevelWindow");
    if (fullscreen) {
      owned_window_->SetProperty(aura::client::kShowStateKey,
                                 ui::SHOW_STATE_FULLSCREEN);
    } else {
      owned_window_->SetType(aura::client::WINDOW_TYPE_MENU);
    }
    owned_window_->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(
        owned_window_,
        widget_.GetNativeView()->GetRootWindow(),
        gfx::Rect(0, 0, 1900, 1600));
    owned_window_->Show();
    owned_window_->AddObserver(this);

    ASSERT_TRUE(owned_window_->parent() != nullptr);
    owned_window_->parent()->AddObserver(this);

    top_level_widget_ =
        views::Widget::GetWidgetForNativeView(owned_window_->parent());
    ASSERT_TRUE(top_level_widget_ != nullptr);
  }

  void DestroyOwnedWindow() {
    ASSERT_TRUE(owned_window_ != nullptr);
    // If async mode is off then clean up state here.
    if (!use_async_mode_) {
      owned_window_->RemoveObserver(this);
      owned_window_->parent()->RemoveObserver(this);
      owner_destroyed_ = true;
      owned_window_destroyed_ = true;
    }
    delete owned_window_;
  }

  void DestroyOwnerWindow() {
    ASSERT_TRUE(top_level_widget_ != nullptr);
    top_level_widget_->CloseNow();
  }

  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    if (window == owned_window_) {
      owned_window_destroyed_ = true;
    } else if (window == top_level_widget_->GetNativeView()) {
      owner_destroyed_ = true;
    } else {
      ADD_FAILURE() << "Unexpected window destroyed callback: " << window;
    }
  }

  aura::Window* owned_window() {
    return owned_window_;
  }

  views::Widget* top_level_widget() {
    return top_level_widget_;
  }

  void set_use_async_mode(bool async_mode) {
    use_async_mode_ = async_mode;
  }

 private:
  views::Widget widget_;
  views::Widget* top_level_widget_ = nullptr;
  aura::Window* owned_window_ = nullptr;
  bool owner_destroyed_ = false;
  bool owned_window_destroyed_ = false;
  aura::test::TestWindowDelegate child_window_delegate_;
  // This flag controls whether we need to wait for the destruction to complete
  // before finishing the test. Defaults to true.
  bool use_async_mode_ = true;

  DISALLOW_COPY_AND_ASSIGN(DesktopAuraTopLevelWindowTest);
};

using DesktopAuraWidgetTest = DesktopWidgetTest;

TEST_F(DesktopAuraWidgetTest, FullscreenWindowDestroyedBeforeOwnerTest) {
  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnedWindow());
  RunPendingMessages();
}

TEST_F(DesktopAuraWidgetTest, FullscreenWindowOwnerDestroyed) {
  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnerWindow());
  RunPendingMessages();
}

TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupTest) {
  DesktopAuraTopLevelWindowTest popup_window;
  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
  RunPendingMessages();
}

// This test validates that when a top level owned popup Aura window is
// resized, the widget is resized as well.
TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupResizeTest) {
  DesktopAuraTopLevelWindowTest popup_window;

  popup_window.set_use_async_mode(false);

  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  gfx::Rect new_size(0, 0, 400, 400);
  popup_window.owned_window()->SetBounds(new_size);

  EXPECT_EQ(popup_window.top_level_widget()->GetNativeView()->bounds().size(),
            new_size.size());

  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
}

// This test validates that when a top level owned popup Aura window is
// repositioned, the widget is repositioned as well.
TEST_F(DesktopAuraWidgetTest, TopLevelOwnedPopupRepositionTest) {
  DesktopAuraTopLevelWindowTest popup_window;

  popup_window.set_use_async_mode(false);

  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  gfx::Rect new_pos(10, 10, 400, 400);
  popup_window.owned_window()->SetBoundsInScreen(
      new_pos,
      display::Screen::GetScreen()->GetDisplayNearestPoint(gfx::Point()));

  EXPECT_EQ(new_pos,
            popup_window.top_level_widget()->GetWindowBoundsInScreen());

  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
}

// The following code verifies we can correctly destroy a Widget from a mouse
// enter/exit. We could test move/drag/enter/exit but in general we don't run
// nested run loops from such events, nor has the code ever really dealt
// with this situation.

// Generates two moves (first generates enter, second real move), a press, drag
// and release stopping at |last_event_type|.
void GenerateMouseEvents(Widget* widget, ui::EventType last_event_type) {
  const gfx::Rect screen_bounds(widget->GetWindowBoundsInScreen());
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, screen_bounds.CenterPoint(),
                            screen_bounds.CenterPoint(), ui::EventTimeForNow(),
                            0, 0);
  ui::EventSink* sink = WidgetTest::GetEventSink(widget);
  ui::EventDispatchDetails details = sink->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_ENTERED || details.dispatcher_destroyed)
    return;
  details = sink->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_MOVED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, screen_bounds.CenterPoint(),
                             screen_bounds.CenterPoint(), ui::EventTimeForNow(),
                             0, 0);
  details = sink->OnEventFromSource(&press_event);
  if (last_event_type == ui::ET_MOUSE_PRESSED || details.dispatcher_destroyed)
    return;

  gfx::Point end_point(screen_bounds.CenterPoint());
  end_point.Offset(1, 1);
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, end_point, end_point,
                            ui::EventTimeForNow(), 0, 0);
  details = sink->OnEventFromSource(&drag_event);
  if (last_event_type == ui::ET_MOUSE_DRAGGED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, end_point, end_point,
                               ui::EventTimeForNow(), 0, 0);
  details = sink->OnEventFromSource(&release_event);
  if (details.dispatcher_destroyed)
    return;
}

// Creates a widget and invokes GenerateMouseEvents() with |last_event_type|.
void RunCloseWidgetDuringDispatchTest(WidgetTest* test,
                                      ui::EventType last_event_type) {
  // |widget| is deleted by CloseWidgetView.
  Widget* widget = new Widget;
  Widget::InitParams params =
      test->CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 50, 100);
  widget->Init(std::move(params));
  widget->SetContentsView(new CloseWidgetView(last_event_type));
  widget->Show();
  GenerateMouseEvents(widget, last_event_type);
}

// Verifies deleting the widget from a mouse pressed event doesn't crash.
TEST_F(DesktopAuraWidgetTest, CloseWidgetDuringMousePress) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_PRESSED);
}

// Verifies deleting the widget from a mouse released event doesn't crash.
TEST_F(DesktopAuraWidgetTest, CloseWidgetDuringMouseReleased) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_RELEASED);
}

namespace {

// Provides functionality to create a window modal dialog.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  ModalDialogDelegate() = default;
  ~ModalDialogDelegate() override = default;

  // WidgetDelegate overrides.
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_WINDOW; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

}  // namespace

#if defined(OS_CHROMEOS)
// TODO(crbug.com/916272): investigate fixing and enabling on Chrome OS.
#define MAYBE_WindowMouseModalityTest DISABLED_WindowMouseModalityTest
#else
#define MAYBE_WindowMouseModalityTest WindowMouseModalityTest
#endif

// This test verifies that whether mouse events when a modal dialog is
// displayed are eaten or recieved by the dialog.
TEST_F(DesktopWidgetTest, MAYBE_WindowMouseModalityTest) {
  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  top_level_widget.Init(std::move(init_params));
  top_level_widget.Show();
  EXPECT_TRUE(top_level_widget.IsVisible());

  // Create a view and validate that a mouse moves makes it to the view.
  EventCountView* widget_view = new EventCountView();
  widget_view->SetBounds(0, 0, 10, 10);
  top_level_widget.GetRootView()->AddChildView(widget_view);

  gfx::Point cursor_location_main(5, 5);
  ui::MouseEvent move_main(ui::ET_MOUSE_MOVED, cursor_location_main,
                           cursor_location_main, ui::EventTimeForNow(),
                           ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      GetEventSink(&top_level_widget)->OnEventFromSource(&move_main);
  ASSERT_FALSE(details.dispatcher_destroyed);

  EXPECT_EQ(1, widget_view->GetEventCount(ui::ET_MOUSE_ENTERED));
  widget_view->ResetCounts();

  // Create a modal dialog and validate that a mouse down message makes it to
  // the main view within the dialog.

  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate;

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, nullptr, top_level_widget.GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  EventCountView* dialog_widget_view = new EventCountView();
  dialog_widget_view->SetBounds(0, 0, 50, 50);
  modal_dialog_widget->GetRootView()->AddChildView(dialog_widget_view);
  modal_dialog_widget->Show();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  gfx::Point cursor_location_dialog(100, 100);
  ui::MouseEvent mouse_down_dialog(
      ui::ET_MOUSE_PRESSED, cursor_location_dialog, cursor_location_dialog,
      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  details =
      GetEventSink(&top_level_widget)->OnEventFromSource(&mouse_down_dialog);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, dialog_widget_view->GetEventCount(ui::ET_MOUSE_PRESSED));

  // Send a mouse move message to the main window. It should not be received by
  // the main window as the modal dialog is still active.
  gfx::Point cursor_location_main2(6, 6);
  ui::MouseEvent mouse_down_main(ui::ET_MOUSE_MOVED, cursor_location_main2,
                                 cursor_location_main2, ui::EventTimeForNow(),
                                 ui::EF_NONE, ui::EF_NONE);
  details =
      GetEventSink(&top_level_widget)->OnEventFromSource(&mouse_down_main);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, widget_view->GetEventCount(ui::ET_MOUSE_MOVED));

  modal_dialog_widget->CloseNow();
  top_level_widget.CloseNow();
}

#if defined(OS_WIN)
// Tests whether we can activate the top level widget when a modal dialog is
// active.
TEST_F(DesktopWidgetTest, WindowModalityActivationTest) {
  TestDesktopWidgetDelegate widget_delegate;
  widget_delegate.InitWidget(CreateParams(Widget::InitParams::TYPE_WINDOW));

  Widget* top_level_widget = widget_delegate.GetWidget();
  top_level_widget->Show();
  EXPECT_TRUE(top_level_widget->IsVisible());

  HWND win32_window = views::HWNDForWidget(top_level_widget);
  EXPECT_TRUE(::IsWindow(win32_window));

  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate;

  // We should be able to activate the window even if the WidgetDelegate
  // says no, when a modal dialog is active.
  widget_delegate.SetCanActivate(false);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, nullptr, top_level_widget->GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  modal_dialog_widget->Show();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  LRESULT activate_result = ::SendMessage(
      win32_window,
      WM_MOUSEACTIVATE,
      reinterpret_cast<WPARAM>(win32_window),
      MAKELPARAM(WM_LBUTTONDOWN, HTCLIENT));
  EXPECT_EQ(activate_result, MA_ACTIVATE);

  modal_dialog_widget->CloseNow();
}

// This test validates that sending WM_CHAR/WM_SYSCHAR/WM_SYSDEADCHAR
// messages via the WindowEventTarget interface implemented by the
// HWNDMessageHandler class does not cause a crash due to an unprocessed
// event
TEST_F(DesktopWidgetTest, CharMessagesAsKeyboardMessagesDoesNotCrash) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(params));
  widget.Show();

  ui::WindowEventTarget* target =
      reinterpret_cast<ui::WindowEventTarget*>(ui::ViewProp::GetValue(
          widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
          ui::WindowEventTarget::kWin32InputEventTarget));
  ASSERT_NE(nullptr, target);
  bool handled = false;
  target->HandleKeyboardMessage(WM_CHAR, 0, 0, &handled);
  target->HandleKeyboardMessage(WM_SYSCHAR, 0, 0, &handled);
  target->HandleKeyboardMessage(WM_SYSDEADCHAR, 0, 0, &handled);
  widget.CloseNow();
}

#endif  // defined(OS_WIN)

}  // namespace test
}  // namespace views
