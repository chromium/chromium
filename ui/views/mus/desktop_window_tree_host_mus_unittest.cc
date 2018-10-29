// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_client_test_api.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/shadow_types.h"

namespace views {

class DesktopWindowTreeHostMusTest : public ViewsTestBase,
                                     public WidgetObserver {
 public:
  DesktopWindowTreeHostMusTest()
      : widget_activated_(nullptr), widget_deactivated_(nullptr) {}
  ~DesktopWindowTreeHostMusTest() override {}

  // Creates a test widget. Takes ownership of |delegate|.
  std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate = nullptr,
                                       aura::Window* parent = nullptr) {
    std::unique_ptr<Widget> widget = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.delegate = delegate;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 1, 111, 123);
    params.parent = parent;
    widget->Init(params);
    widget->AddObserver(this);
    return widget;
  }

  const Widget* widget_activated() const { return widget_activated_; }
  const Widget* widget_deactivated() const { return widget_deactivated_; }

 private:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      widget_activated_ = widget;
    } else {
      if (widget_activated_ == widget)
        widget_activated_ = nullptr;
      widget_deactivated_ = widget;
    }
  }

  Widget* widget_activated_;
  Widget* widget_deactivated_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostMusTest);
};

class ExpectsNullCursorClientDuringTearDown : public aura::WindowObserver {
 public:
  explicit ExpectsNullCursorClientDuringTearDown(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ~ExpectsNullCursorClientDuringTearDown() override { EXPECT_FALSE(window_); }

 private:
  // aura::WindowObserver:
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(window->GetRootWindow());
    EXPECT_FALSE(cursor_client);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  aura::Window* window_;
  DISALLOW_COPY_AND_ASSIGN(ExpectsNullCursorClientDuringTearDown);
};

TEST_F(DesktopWindowTreeHostMusTest, Visibility) {
  std::unique_ptr<Widget> widget(CreateWidget());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  // It's important the parent is also hidden as this value is sent to the
  // server.
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->IsVisible());
  EXPECT_TRUE(widget->GetNativeView()->parent()->IsVisible());
  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->IsVisible());
  EXPECT_FALSE(widget->GetNativeView()->parent()->IsVisible());
}

TEST_F(DesktopWindowTreeHostMusTest, Capture) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();
  EXPECT_FALSE(widget1->HasCapture());

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();
  EXPECT_FALSE(widget2->HasCapture());

  widget1->SetCapture(widget1->GetRootView());
  EXPECT_TRUE(widget1->HasCapture());
  EXPECT_FALSE(widget2->HasCapture());
  EXPECT_EQ(widget1->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget2->SetCapture(widget2->GetRootView());
  EXPECT_TRUE(widget2->HasCapture());
  EXPECT_EQ(widget2->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget1->ReleaseCapture();
  EXPECT_TRUE(widget2->HasCapture());
  EXPECT_FALSE(widget1->HasCapture());
  EXPECT_EQ(widget2->GetNativeWindow(), MusClient::Get()
                                            ->window_tree_client()
                                            ->capture_synchronizer()
                                            ->capture_window()
                                            ->GetWindow());

  widget2->ReleaseCapture();
  EXPECT_FALSE(widget2->HasCapture());
  EXPECT_FALSE(widget1->HasCapture());
  EXPECT_EQ(nullptr, MusClient::Get()
                         ->window_tree_client()
                         ->capture_synchronizer()
                         ->capture_window());
}

TEST_F(DesktopWindowTreeHostMusTest, Deactivate) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  widget1->Activate();
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());

  RunPendingMessages();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  EXPECT_EQ(widget_activated(), widget1.get());

  widget1->Deactivate();
  EXPECT_FALSE(widget1->IsActive());
}

TEST_F(DesktopWindowTreeHostMusTest, HideWindowTreeHostWindowChangesActive) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();
  widget1->Activate();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  ASSERT_TRUE(widget1->GetNativeWindow()->parent());

  // This simulates what happens when a hide happens from the server.
  widget1->GetNativeWindow()->GetHost()->Hide();
  EXPECT_FALSE(widget1->GetNativeWindow()->HasFocus());
  EXPECT_FALSE(widget1->IsActive());
}

TEST_F(DesktopWindowTreeHostMusTest, BecomesActiveOnMousePress) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->ShowInactive();
  aura::test::WaitForAllChangesToComplete();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->GetNativeWindow()->HasFocus());

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ignore_result(
      widget->GetNativeWindow()->GetHost()->event_sink()->OnEventFromSource(
          &mouse_event));
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->GetNativeWindow()->HasFocus());
  aura::client::FocusClient* widget_focus_client =
      aura::client::GetFocusClient(widget->GetNativeWindow());
  EXPECT_EQ(widget_focus_client, MusClient::Get()
                                     ->window_tree_client()
                                     ->focus_synchronizer()
                                     ->active_focus_client());

  // The mouse event should generate a focus request to the server.
  EXPECT_TRUE(
      aura::WindowTreeClientPrivate(MusClient::Get()->window_tree_client())
          .HasChangeInFlightOfType(aura::ChangeType::FOCUS));
}

TEST_F(DesktopWindowTreeHostMusTest, ActivateBeforeShow) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  // Activation can be attempted before visible.
  widget1->Activate();
  widget1->Show();
  widget1->Activate();
  EXPECT_TRUE(widget1->IsActive());
  // The Widget's NativeWindow (|DesktopNativeWidgetAura::content_window_|)
  // should be active.
  EXPECT_TRUE(widget1->GetNativeWindow()->HasFocus());
  // Env's active FocusClient should match the active window.
  aura::client::FocusClient* widget_focus_client =
      aura::client::GetFocusClient(widget1->GetNativeWindow());
  ASSERT_TRUE(widget_focus_client);
  EXPECT_EQ(widget_focus_client, MusClient::Get()
                                     ->window_tree_client()
                                     ->focus_synchronizer()
                                     ->active_focus_client());
}

// Tests that changes to kTopViewInset will cause the client area to be updated.
TEST_F(DesktopWindowTreeHostMusTest, ServerTopInsetChangeUpdatesClientArea) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  auto set_top_inset = [&widget](int value) {
    widget->GetNativeWindow()->GetRootWindow()->SetProperty(
        aura::client::kTopViewInset, value);
  };

  EXPECT_EQ(widget->GetRootView()->bounds(), widget->client_view()->bounds());

  set_top_inset(3);
  gfx::Rect root_bounds = widget->GetRootView()->bounds();
  root_bounds.Inset(gfx::Insets(3, 0, 0, 0));

  set_top_inset(0);
  EXPECT_EQ(widget->GetRootView()->bounds(), widget->client_view()->bounds());
}

TEST_F(DesktopWindowTreeHostMusTest, CursorClientDuringTearDown) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();

  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->Init(ui::LAYER_SOLID_COLOR);
  ExpectsNullCursorClientDuringTearDown observer(window.get());

  widget->GetNativeWindow()->AddChild(window.release());
  widget.reset();
}

TEST_F(DesktopWindowTreeHostMusTest, StackAtTop) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget1->StackAtTop();
  waiter.Wait();

  // Other than the signal that our StackAtTop() succeeded, we don't have any
  // pieces of public data that we can check. If we actually stopped waiting,
  // count that as success.
}

TEST_F(DesktopWindowTreeHostMusTest, StackAtTopAlreadyOnTop) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget());
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget2->StackAtTop();
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, StackAbove) {
  std::unique_ptr<Widget> widget1(CreateWidget(nullptr));
  widget1->Show();

  std::unique_ptr<Widget> widget2(CreateWidget(nullptr));
  widget2->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::REORDER, true);
  widget1->StackAboveWidget(widget2.get());
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, SetOpacity) {
  std::unique_ptr<Widget> widget1(CreateWidget(nullptr));
  widget1->Show();

  aura::test::ChangeCompletionWaiter waiter(aura::ChangeType::OPACITY, true);
  widget1->SetOpacity(0.5f);
  waiter.Wait();
}

TEST_F(DesktopWindowTreeHostMusTest, TransientParentWiredToHostWindow) {
  std::unique_ptr<Widget> widget1(CreateWidget());
  widget1->Show();

  std::unique_ptr<Widget> widget2(
      CreateWidget(nullptr, widget1->GetNativeView()));
  widget2->Show();

  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  // Even though the widget1->GetNativeView() was specified as the parent we
  // expect the transient parents to be marked at the host level.
  EXPECT_EQ(widget1->GetNativeView()->GetHost()->window(),
            transient_window_client->GetTransientParent(
                widget2->GetNativeView()->GetHost()->window()));
}

TEST_F(DesktopWindowTreeHostMusTest, ShadowDefaults) {
  Widget widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);
  // |DesktopNativeWidgetAura::content_window_| should have no shadow; the wm
  // should provide it if it so desires.
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetProperty(wm::kShadowElevationKey));
  // The wm honors the shadow property from the WindowTreeHost's window.
  EXPECT_EQ(wm::kShadowElevationDefault,
            widget.GetNativeView()->GetHost()->window()->GetProperty(
                wm::kShadowElevationKey));
}

TEST_F(DesktopWindowTreeHostMusTest, NoShadow) {
  Widget widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.shadow_type = Widget::InitParams::SHADOW_TYPE_NONE;
  widget.Init(params);
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetProperty(wm::kShadowElevationKey));
  EXPECT_EQ(wm::kShadowElevationNone,
            widget.GetNativeView()->GetHost()->window()->GetProperty(
                wm::kShadowElevationKey));
}

TEST_F(DesktopWindowTreeHostMusTest, CreateFullscreenWidget) {
  const Widget::InitParams::Type kWidgetTypes[] = {
      Widget::InitParams::TYPE_WINDOW,
      Widget::InitParams::TYPE_WINDOW_FRAMELESS,
  };

  for (auto widget_type : kWidgetTypes) {
    Widget widget;
    Widget::InitParams params(widget_type);
    params.show_state = ui::SHOW_STATE_FULLSCREEN;
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);

    EXPECT_TRUE(widget.IsFullscreen())
        << "Fullscreen creation failed for type=" << widget_type;
  }
}

TEST_F(DesktopWindowTreeHostMusTest, GetWindowBoundsInScreen) {
  ScreenMus* screen = MusClientTestApi::screen();

  // Add a second display to the right of the primary.
  const int64_t kSecondDisplayId = 222;
  screen->display_list().AddDisplay(
      display::Display(kSecondDisplayId, gfx::Rect(800, 0, 640, 480)),
      display::DisplayList::Type::NOT_PRIMARY);

  // Verify bounds for a widget on the first display.
  Widget widget1;
  Widget::InitParams params1(Widget::InitParams::TYPE_WINDOW);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.bounds = gfx::Rect(0, 0, 100, 100);
  widget1.Init(params1);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), widget1.GetWindowBoundsInScreen());

  // Verify bounds for a widget on the secondary display.
  Widget widget2;
  Widget::InitParams params2(Widget::InitParams::TYPE_WINDOW);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.bounds = gfx::Rect(800, 0, 100, 100);
  widget2.Init(params2);
  EXPECT_EQ(gfx::Rect(800, 0, 100, 100), widget2.GetWindowBoundsInScreen());
  EXPECT_EQ(kSecondDisplayId,
            aura::WindowTreeHostMus::ForWindow(widget2.GetNativeWindow())
                ->display_id());
}

// WidgetDelegate implementation that allows setting window-title and whether
// the title should be shown.
class WindowTitleWidgetDelegate : public WidgetDelegateView {
 public:
  WindowTitleWidgetDelegate() = default;
  ~WindowTitleWidgetDelegate() override = default;

  void set_window_title(const base::string16& title) { window_title_ = title; }
  void set_should_show_window_title(bool value) {
    should_show_window_title_ = value;
  }

  // WidgetDelegateView:
  base::string16 GetWindowTitle() const override { return window_title_; }
  bool ShouldShowWindowTitle() const override {
    return should_show_window_title_;
  }

 private:
  base::string16 window_title_;
  bool should_show_window_title_ = true;

  DISALLOW_COPY_AND_ASSIGN(WindowTitleWidgetDelegate);
};

TEST_F(DesktopWindowTreeHostMusTest, WindowTitle) {
  // Owned by |widget|.
  WindowTitleWidgetDelegate* delegate = new WindowTitleWidgetDelegate();
  std::unique_ptr<Widget> widget(CreateWidget(delegate));
  aura::Window* window = widget->GetNativeWindow()->GetRootWindow();

  // Set the title in the delegate and verify it propagates.
  const base::string16 title1 = base::ASCIIToUTF16("X");
  delegate->set_window_title(title1);
  widget->UpdateWindowTitle();
  EXPECT_TRUE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title1, window->GetTitle());

  // Hiding the title should not change the title.
  delegate->set_should_show_window_title(false);
  widget->UpdateWindowTitle();
  EXPECT_FALSE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title1, window->GetTitle());

  // Show the title again with a different value.
  delegate->set_should_show_window_title(true);
  const base::string16 title2 = base::ASCIIToUTF16("Z");
  delegate->set_window_title(title2);
  widget->UpdateWindowTitle();
  EXPECT_TRUE(window->GetProperty(aura::client::kTitleShownKey));
  EXPECT_EQ(title2, window->GetTitle());
}

TEST_F(DesktopWindowTreeHostMusTest, Accessibility) {
  std::unique_ptr<Widget> widget = CreateWidget();
  // Widget frame views do not participate in accessibility node hierarchy
  // because the frame is provided by the window manager.
  views::NonClientView* non_client_view = widget->non_client_view();
  EXPECT_TRUE(non_client_view->GetViewAccessibility().is_ignored());
  EXPECT_TRUE(
      non_client_view->frame_view()->GetViewAccessibility().is_ignored());
  EXPECT_TRUE(widget->client_view()->GetViewAccessibility().is_ignored());
}

// Used to ensure the visibility of the root window is changed before that of
// the content window. This is necessary else close/hide animations end up
// animating a hidden (black) window.
class WidgetWindowVisibilityObserver : public aura::WindowObserver {
 public:
  explicit WidgetWindowVisibilityObserver(Widget* widget)
      : content_window_(widget->GetNativeWindow()),
        root_window_(content_window_->GetRootWindow()) {
    EXPECT_NE(content_window_, root_window_);
    content_window_->AddObserver(this);
    root_window_->AddObserver(this);
    EXPECT_TRUE(content_window_->IsVisible());
    EXPECT_TRUE(root_window_->IsVisible());
  }

  ~WidgetWindowVisibilityObserver() override {
    content_window_->RemoveObserver(this);
    root_window_->RemoveObserver(this);
  }

  bool got_content_window_hidden() const { return got_content_window_hidden_; }

  bool got_root_window_hidden() const { return got_root_window_hidden_; }

 private:
  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override {
    if (visible)
      return;

    if (!got_root_window_hidden_) {
      EXPECT_EQ(window, root_window_);
      got_root_window_hidden_ = true;
    } else if (!got_content_window_hidden_) {
      EXPECT_EQ(window, content_window_);
      got_content_window_hidden_ = true;
    }
  }

  aura::Window* content_window_;
  aura::Window* root_window_;

  // Set to true when |content_window_| is hidden. This is only checked after
  // the |root_window_| is hidden.
  bool got_content_window_hidden_ = false;

  // Set to true when |root_window_| is hidden.
  bool got_root_window_hidden_ = false;

  DISALLOW_COPY_AND_ASSIGN(WidgetWindowVisibilityObserver);
};

// See comments above WidgetWindowVisibilityObserver for details on what this
// verifies.
TEST_F(DesktopWindowTreeHostMusTest,
       HideChangesRootWindowVisibilityBeforeContentWindowVisibility) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  WidgetWindowVisibilityObserver observer(widget.get());
  widget->Close();
  EXPECT_TRUE(observer.got_content_window_hidden());
  EXPECT_TRUE(observer.got_root_window_hidden());
}

TEST_F(DesktopWindowTreeHostMusTest, MinimizeActivate) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  widget->Minimize();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget->IsMinimized());

  // Activate() should restore the window.
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget->IsMinimized());
}

TEST_F(DesktopWindowTreeHostMusTest, MaximizeMinimizeRestore) {
  std::unique_ptr<Widget> widget(CreateWidget());
  widget->Show();
  EXPECT_TRUE(widget->IsActive());

  widget->Maximize();
  widget->Minimize();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsMaximized());

  widget->Restore();
  // Restore() *always* sets the state to normal, not the pre-minimized state.
  // This mirrors the logic in NativeWidgetAura. See
  // DesktopWindowTreeHostMus::RestoreToPreminimizedState() for details.
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsMaximized());
}

}  // namespace views
