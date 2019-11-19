// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/transient_window_manager.h"

namespace views {
namespace {

NativeWidgetAura* Init(aura::Window* parent, Widget* widget) {
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent;
  widget->Init(std::move(params));
  return static_cast<NativeWidgetAura*>(widget->native_widget());
}

// TestFocusRules is intended to provide a way to manually set a window's
// activatability so that the focus rules can be tested.
class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;
  ~TestFocusRules() override = default;

  void set_can_activate(bool can_activate) { can_activate_ = can_activate; }

  // wm::BaseFocusRules overrides:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }

  bool CanActivateWindow(const aura::Window* window) const override {
    return can_activate_;
  }

 private:
  bool can_activate_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

class NativeWidgetAuraTest : public ViewsTestBase {
 public:
  NativeWidgetAuraTest() = default;
  ~NativeWidgetAuraTest() override = default;

  TestFocusRules* test_focus_rules() { return test_focus_rules_; }

  // testing::Test overrides:
  void SetUp() override {
    ViewsTestBase::SetUp();
    test_focus_rules_ = new TestFocusRules;
    focus_controller_ =
        std::make_unique<wm::FocusController>(test_focus_rules_);
    wm::SetActivationClient(root_window(), focus_controller_.get());
    host()->SetBoundsInPixels(gfx::Rect(640, 480));
  }

 private:
  std::unique_ptr<wm::FocusController> focus_controller_;
  TestFocusRules* test_focus_rules_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetAuraTest);
};

TEST_F(NativeWidgetAuraTest, CenterWindowLargeParent) {
  // Make a parent window larger than the host represented by
  // WindowEventDispatcher.
  std::unique_ptr<aura::Window> parent(new aura::Window(nullptr));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 1024, 800));
  std::unique_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect((640 - 100) / 2,
                      (480 - 100) / 2,
                      100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, CenterWindowSmallParent) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher.
  std::unique_ptr<aura::Window> parent(new aura::Window(nullptr));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  std::unique_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect((480 - 100) / 2,
                      (320 - 100) / 2,
                      100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

// Verifies CenterWindow() constrains to parent size.
TEST_F(NativeWidgetAuraTest, CenterWindowSmallParentNotAtOrigin) {
  // Make a parent window smaller than the host represented by
  // WindowEventDispatcher and offset it slightly from the origin.
  std::unique_ptr<aura::Window> parent(new aura::Window(nullptr));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(20, 40, 480, 320));
  std::unique_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());
  window->CenterWindow(gfx::Size(500, 600));

  // |window| should be no bigger than |parent|.
  EXPECT_EQ("20,40 480x320", window->GetNativeWindow()->bounds().ToString());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, CreateMinimized) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MINIMIZED;
  params.bounds.SetRect(0, 0, 1024, 800);
  std::unique_ptr<Widget> widget(new Widget());
  widget->Init(std::move(params));
  widget->Show();

  EXPECT_TRUE(widget->IsMinimized());
  widget->CloseNow();
}

// A WindowObserver that counts kShowStateKey property changes.
class TestWindowObserver : public aura::WindowObserver {
 public:
  explicit TestWindowObserver(gfx::NativeWindow window) : window_(window) {
    window_->AddObserver(this);
  }
  ~TestWindowObserver() override {
    window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != aura::client::kShowStateKey)
      return;
    count_++;
    state_ = window_->GetProperty(aura::client::kShowStateKey);
  }

  int count() const { return count_; }
  ui::WindowShowState state() const { return state_; }
  void Reset() { count_ = 0; }

 private:
  gfx::NativeWindow window_;
  int count_ = 0;
  ui::WindowShowState state_ = ui::WindowShowState::SHOW_STATE_DEFAULT;

 DISALLOW_COPY_AND_ASSIGN(TestWindowObserver);
};

// Tests that window transitions from normal to minimized and back do not
// involve extra show state transitions.
TEST_F(NativeWidgetAuraTest, ToggleState) {
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.bounds.SetRect(0, 0, 1024, 800);
  Widget widget;
  widget.Init(std::move(params));
  std::unique_ptr<TestWindowObserver> observer(
      new TestWindowObserver(widget.GetNativeWindow()));
  widget.Show();
  EXPECT_FALSE(widget.IsMinimized());
  EXPECT_EQ(0, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_DEFAULT, observer->state());

  widget.Minimize();
  EXPECT_TRUE(widget.IsMinimized());
  EXPECT_EQ(1, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_MINIMIZED, observer->state());
  observer->Reset();

  widget.Show();
  widget.Restore();
  EXPECT_EQ(1, observer->count());
  EXPECT_EQ(ui::WindowShowState::SHOW_STATE_NORMAL, observer->state());

  observer.reset();
  EXPECT_FALSE(widget.IsMinimized());
  widget.CloseNow();
}

class TestLayoutManagerBase : public aura::LayoutManager {
 public:
  TestLayoutManagerBase() = default;
  ~TestLayoutManagerBase() override = default;

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManagerBase);
};

// Used by ShowMaximizedDoesntBounceAround. See it for details.
class MaximizeLayoutManager : public TestLayoutManagerBase {
 public:
  MaximizeLayoutManager() = default;
  ~MaximizeLayoutManager() override = default;

 private:
  // aura::LayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override {
    // This simulates what happens when adding a maximized window.
    SetChildBoundsDirect(child, gfx::Rect(0, 0, 300, 300));
  }

  DISALLOW_COPY_AND_ASSIGN(MaximizeLayoutManager);
};

// This simulates BrowserView, which creates a custom RootView so that
// OnNativeWidgetSizeChanged that is invoked during Init matters.
class TestWidget : public views::Widget {
 public:
  TestWidget() = default;

  // Returns true if the size changes to a non-empty size, and then to another
  // size.
  bool did_size_change_more_than_once() const {
    return did_size_change_more_than_once_;
  }

  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override {
    if (last_size_.IsEmpty())
      last_size_ = new_size;
    else if (!did_size_change_more_than_once_ && new_size != last_size_)
      did_size_change_more_than_once_ = true;
    Widget::OnNativeWidgetSizeChanged(new_size);
  }

 private:
  bool did_size_change_more_than_once_ = false;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidget);
};

// Verifies the size of the widget doesn't change more than once during Init if
// the window ends up maximized. This is important as otherwise
// RenderWidgetHostViewAura ends up getting resized during construction, which
// leads to noticable flashes.
TEST_F(NativeWidgetAuraTest, ShowMaximizedDoesntBounceAround) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  root_window()->SetLayoutManager(new MaximizeLayoutManager);
  std::unique_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = nullptr;
  params.context = root_window();
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
  params.bounds = gfx::Rect(10, 10, 100, 200);
  widget->Init(std::move(params));
  EXPECT_FALSE(widget->did_size_change_more_than_once());
  widget->CloseNow();
}

class PropertyTestLayoutManager : public TestLayoutManagerBase {
 public:
  PropertyTestLayoutManager() = default;
  ~PropertyTestLayoutManager() override = default;

  bool added() const { return added_; }

 private:
  // aura::LayoutManager:
  void OnWindowAddedToLayout(aura::Window* child) override {
    EXPECT_EQ(aura::client::kResizeBehaviorCanResize |
                  aura::client::kResizeBehaviorCanMaximize |
                  aura::client::kResizeBehaviorCanMinimize,
              child->GetProperty(aura::client::kResizeBehaviorKey));
    added_ = true;
  }

  bool added_ = false;

  DISALLOW_COPY_AND_ASSIGN(PropertyTestLayoutManager);
};

class PropertyTestWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit PropertyTestWidgetDelegate(Widget* widget) : widget_(widget) {}
  ~PropertyTestWidgetDelegate() override = default;

 private:
  // views::WidgetDelegate:
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  bool CanResize() const override { return true; }
  void DeleteDelegate() override { delete this; }
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }

  Widget* widget_;
  DISALLOW_COPY_AND_ASSIGN(PropertyTestWidgetDelegate);
};

// Verifies the resize behavior when added to the layout manager.
TEST_F(NativeWidgetAuraTest, TestPropertiesWhenAddedToLayout) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  PropertyTestLayoutManager* layout_manager = new PropertyTestLayoutManager();
  root_window()->SetLayoutManager(layout_manager);
  std::unique_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new PropertyTestWidgetDelegate(widget.get());
  params.parent = nullptr;
  params.context = root_window();
  widget->Init(std::move(params));
  EXPECT_TRUE(layout_manager->added());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, GetClientAreaScreenBounds) {
  // Create a widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window();
  params.bounds.SetRect(10, 20, 300, 400);
  std::unique_ptr<Widget> widget(new Widget());
  widget->Init(std::move(params));

  // For Aura, client area bounds match window bounds.
  gfx::Rect client_bounds = widget->GetClientAreaBoundsInScreen();
  EXPECT_EQ(10, client_bounds.x());
  EXPECT_EQ(20, client_bounds.y());
  EXPECT_EQ(300, client_bounds.width());
  EXPECT_EQ(400, client_bounds.height());
}

// View subclass that tracks whether it has gotten a gesture event.
class GestureTrackingView : public views::View {
 public:
  GestureTrackingView() = default;

  void set_consume_gesture_event(bool value) {
    consume_gesture_event_ = value;
  }

  void clear_got_gesture_event() {
    got_gesture_event_ = false;
  }
  bool got_gesture_event() const {
    return got_gesture_event_;
  }

  // View overrides:
  void OnGestureEvent(ui::GestureEvent* event) override {
    got_gesture_event_ = true;
    if (consume_gesture_event_)
      event->StopPropagation();
  }

 private:
  // Was OnGestureEvent() invoked?
  bool got_gesture_event_ = false;

  // Dictates what OnGestureEvent() returns.
  bool consume_gesture_event_ = true;

  DISALLOW_COPY_AND_ASSIGN(GestureTrackingView);
};

// Verifies a capture isn't set on touch press and that the view that gets
// the press gets the release.
TEST_F(NativeWidgetAuraTest, DontCaptureOnGesture) {
  // Create two views (both sized the same). |child| is configured not to
  // consume the gesture event.
  GestureTrackingView* view = new GestureTrackingView();
  GestureTrackingView* child = new GestureTrackingView();
  child->set_consume_gesture_event(false);
  view->SetLayoutManager(std::make_unique<FillLayout>());
  view->AddChildView(child);
  std::unique_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window();
  params.bounds = gfx::Rect(0, 0, 100, 200);
  widget->Init(std::move(params));
  widget->SetContentsView(view);
  widget->Show();

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(41, 51), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  ui::EventDispatchDetails details = event_sink()->OnEventFromSource(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  // Both views should get the press.
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_TRUE(child->got_gesture_event());
  view->clear_got_gesture_event();
  child->clear_got_gesture_event();
  // Touch events should not automatically grab capture.
  EXPECT_FALSE(widget->HasCapture());

  // Release touch. Only |view| should get the release since that it consumed
  // the press.
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(250, 251), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  details = event_sink()->OnEventFromSource(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(view->got_gesture_event());
  EXPECT_FALSE(child->got_gesture_event());
  view->clear_got_gesture_event();

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  widget->Close();
}

// Verifies views with layers are targeted for events properly.
TEST_F(NativeWidgetAuraTest, PreferViewLayersToChildWindows) {
  // Create two widgets: |parent| and |child|. |child| is a child of |parent|.
  views::View* parent_root = new views::View;
  std::unique_ptr<Widget> parent(new Widget());
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_params.context = root_window();
  parent->Init(std::move(parent_params));
  parent->SetContentsView(parent_root);
  parent->SetBounds(gfx::Rect(0, 0, 400, 400));
  parent->Show();

  std::unique_ptr<Widget> child(new Widget());
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent->GetNativeWindow();
  child->Init(std::move(child_params));
  child->SetBounds(gfx::Rect(0, 0, 200, 200));
  child->Show();

  // Point is over |child|.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(50, 50)));

  // Create a view with a layer and stack it at the bottom (below |child|).
  views::View* view_with_layer = new views::View;
  parent_root->AddChildView(view_with_layer);
  view_with_layer->SetBounds(0, 0, 50, 50);
  view_with_layer->SetPaintToLayer();

  // Make sure that |child| still gets the event.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Move |view_with_layer| to the top and make sure it gets the
  // event when the point is within |view_with_layer|'s bounds.
  view_with_layer->layer()->parent()->StackAtTop(
      view_with_layer->layer());
  EXPECT_EQ(parent->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Point is over |child|, it should get the event.
  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(70, 70)));

  delete view_with_layer;
  view_with_layer = nullptr;

  EXPECT_EQ(child->GetNativeWindow(),
            parent->GetNativeWindow()->GetEventHandlerForPoint(
                gfx::Point(20, 20)));

  // Work around for bug in NativeWidgetAura.
  // TODO: fix bug and remove this.
  parent->Close();
}

// Verifies views with layers are targeted for events properly.
TEST_F(NativeWidgetAuraTest,
       ShouldDescendIntoChildForEventHandlingChecksVisibleBounds) {
  // Create two widgets: |parent| and |child|. |child| is a child of |parent|.
  View* parent_root_view = new View;
  Widget parent;
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_params.context = root_window();
  parent.Init(std::move(parent_params));
  parent.SetContentsView(parent_root_view);
  parent.SetBounds(gfx::Rect(0, 0, 400, 400));
  parent.Show();

  Widget child;
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent.GetNativeWindow();
  child.Init(std::move(child_params));
  child.SetBounds(gfx::Rect(0, 0, 200, 200));
  child.Show();

  // Point is over |child|.
  EXPECT_EQ(
      child.GetNativeWindow(),
      parent.GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(50, 50)));

  View* parent_root_view_child = new View;
  parent_root_view->AddChildView(parent_root_view_child);
  parent_root_view_child->SetBounds(0, 0, 10, 10);

  // Create a View whose layer extends outside the bounds of its parent. Event
  // targetting should only consider the visible bounds.
  View* parent_root_view_child_child = new View;
  parent_root_view_child->AddChildView(parent_root_view_child_child);
  parent_root_view_child_child->SetBounds(0, 0, 100, 100);
  parent_root_view_child_child->SetPaintToLayer();
  parent_root_view_child_child->layer()->parent()->StackAtTop(
      parent_root_view_child_child->layer());

  // 20,20 is over |parent_root_view_child_child|'s layer, but not the visible
  // bounds of |parent_root_view_child_child|, so |child| should be the event
  // target.
  EXPECT_EQ(
      child.GetNativeWindow(),
      parent.GetNativeWindow()->GetEventHandlerForPoint(gfx::Point(20, 20)));
}

// Verifies that widget->FlashFrame() sets aura::client::kDrawAttentionKey,
// and activating the window clears it.
TEST_F(NativeWidgetAuraTest, FlashFrame) {
  std::unique_ptr<Widget> widget(new Widget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = root_window();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  aura::Window* window = widget->GetNativeWindow();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(false);
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->FlashFrame(true);
  EXPECT_TRUE(window->GetProperty(aura::client::kDrawAttentionKey));
  widget->Activate();
  EXPECT_FALSE(window->GetProperty(aura::client::kDrawAttentionKey));
}

TEST_F(NativeWidgetAuraTest, NoCrashOnThemeAfterClose) {
  std::unique_ptr<aura::Window> parent(new aura::Window(nullptr));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  std::unique_ptr<Widget> widget(new Widget());
  Init(parent.get(), widget.get());
  widget->Show();
  widget->Close();
  base::RunLoop().RunUntilIdle();
  widget->GetNativeTheme();  // Shouldn't crash.
}

// Used to track calls to WidgetDelegate::OnWidgetMove().
class MoveTestWidgetDelegate : public WidgetDelegateView {
 public:
  MoveTestWidgetDelegate() = default;
  ~MoveTestWidgetDelegate() override = default;

  void ClearGotMove() { got_move_ = false; }
  bool got_move() const { return got_move_; }

  // WidgetDelegate overrides:
  void OnWidgetMove() override { got_move_ = true; }

 private:
  bool got_move_ = false;

  DISALLOW_COPY_AND_ASSIGN(MoveTestWidgetDelegate);
};

// This test simulates what happens when a window is normally maximized. That
// is, it's layer is acquired for animation then the window is maximized.
// Acquiring the layer resets the bounds of the window. This test verifies the
// Widget is still notified correctly of a move in this case.
TEST_F(NativeWidgetAuraTest, OnWidgetMovedInvokedAfterAcquireLayer) {
  // |delegate| deletes itself when the widget is destroyed.
  MoveTestWidgetDelegate* delegate = new MoveTestWidgetDelegate;
  Widget* widget =
      Widget::CreateWindowWithContextAndBounds(delegate,
                                               root_window(),
                                               gfx::Rect(10, 10, 100, 200));
  widget->Show();
  delegate->ClearGotMove();
  // Simulate a maximize with animation.
  delete widget->GetNativeView()->RecreateLayer().release();
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));
  EXPECT_TRUE(delegate->got_move());
  widget->CloseNow();
}

// Tests that if a widget has a view which should be initially focused when the
// widget is shown, this view should not get focused if the associated window
// can not be activated.
TEST_F(NativeWidgetAuraTest, PreventFocusOnNonActivableWindow) {
  test_focus_rules()->set_can_activate(false);
  views::test::TestInitialFocusWidgetDelegate delegate(root_window());
  delegate.GetWidget()->Show();
  EXPECT_FALSE(delegate.view()->HasFocus());

  test_focus_rules()->set_can_activate(true);
  views::test::TestInitialFocusWidgetDelegate delegate2(root_window());
  delegate2.GetWidget()->Show();
  EXPECT_TRUE(delegate2.view()->HasFocus());
}

// Tests that the transient child bubble window is only visible if the parent is
// visible.
TEST_F(NativeWidgetAuraTest, VisibilityOfChildBubbleWindow) {
  // Create a parent window.
  Widget parent;
  Widget::InitParams parent_params(Widget::InitParams::TYPE_WINDOW);
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_params.context = root_window();
  parent.Init(std::move(parent_params));
  parent.SetBounds(gfx::Rect(0, 0, 480, 320));

  // Add a child bubble window to the above parent window and show it.
  Widget child;
  Widget::InitParams child_params(Widget::InitParams::TYPE_BUBBLE);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent.GetNativeWindow();
  child.Init(std::move(child_params));
  child.SetBounds(gfx::Rect(0, 0, 200, 200));
  child.Show();

  // Check that the bubble window is added as the transient child and it is
  // hidden because parent window is hidden.
  wm::TransientWindowManager* manager =
      wm::TransientWindowManager::GetOrCreate(child.GetNativeWindow());
  EXPECT_EQ(parent.GetNativeWindow(), manager->transient_parent());
  EXPECT_FALSE(parent.IsVisible());
  EXPECT_FALSE(child.IsVisible());

  // Show the parent window should make the transient child bubble visible.
  parent.Show();
  EXPECT_TRUE(parent.IsVisible());
  EXPECT_TRUE(child.IsVisible());
}

}  // namespace
}  // namespace views
