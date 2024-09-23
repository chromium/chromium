// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_test_base.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// Observer watching for window visibility and bounds change events. This is
// used to verify that the child and clipping window operations are done in the
// right order.
class NativeViewHostWindowObserver : public aura::WindowObserver {
 public:
  enum EventType {
    EVENT_NONE,
    EVENT_SHOWN,
    EVENT_HIDDEN,
    EVENT_BOUNDS_CHANGED,
    EVENT_DESTROYED,
  };

  struct EventDetails {
    static int id;

    EventDetails(EventType event_type,
                 aura::Window& window,
                 const gfx::Rect& event_bounds)
        : type(event_type), bounds(event_bounds) {
      if (window.GetId() == aura::Window::kInitialId) {
        window.SetId(++id);
      }
      window_id = window.GetId();
    }

    EventType type;
    int window_id;
    gfx::Rect bounds;
    bool operator!=(const EventDetails& rhs) {
      return type != rhs.type || window_id != rhs.window_id ||
             bounds != rhs.bounds;
    }
  };

  NativeViewHostWindowObserver() = default;

  NativeViewHostWindowObserver(const NativeViewHostWindowObserver&) = delete;
  NativeViewHostWindowObserver& operator=(const NativeViewHostWindowObserver&) =
      delete;

  ~NativeViewHostWindowObserver() override = default;

  const std::vector<EventDetails>& events() const { return events_; }

  // aura::WindowObserver overrides
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    EventDetails event(visible ? EVENT_SHOWN : EVENT_HIDDEN, *window,
                       window->GetBoundsInRootWindow());

    // Dedupe events as a single Hide() call can result in several
    // notifications.
    if (events_.size() == 0u || events_.back() != event)
      events_.push_back(event);
  }

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    EventDetails event(EVENT_BOUNDS_CHANGED, *window,
                       window->GetBoundsInRootWindow());
    events_.push_back(event);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    EventDetails event(EVENT_DESTROYED, *window, gfx::Rect());
    events_.push_back(event);
  }

 private:
  std::vector<EventDetails> events_;
  gfx::Rect bounds_at_visibility_changed_;
};

int NativeViewHostWindowObserver::EventDetails::id = 1;

class NativeViewHostAuraTest : public test::NativeViewHostTestBase {
 public:
  NativeViewHostAuraTest() = default;

  NativeViewHostAuraTest(const NativeViewHostAuraTest&) = delete;
  NativeViewHostAuraTest& operator=(const NativeViewHostAuraTest&) = delete;

  NativeViewHostAura* native_host() {
    return static_cast<NativeViewHostAura*>(GetNativeWrapper());
  }

  Widget* child() { return child_.get(); }

  aura::Window* clipping_window() {
    return native_host()->clipping_window_.get();
  }

  void CreateHost() {
    CreateTopLevel();
    CreateTestingHost();
    child_ = CreateChildForHost(toplevel()->GetNativeView(),
                                toplevel()->client_view(), new View, host());
  }

  // test::NativeViewHostTestBase:
  void TearDown() override {
    child_.reset();
    test::NativeViewHostTestBase::TearDown();
  }

 private:
  std::unique_ptr<Widget> child_;
};

// Verifies NativeViewHostAura stops observing native view on destruction.
TEST_F(NativeViewHostAuraTest, StopObservingNativeViewOnDestruct) {
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  NativeViewHostAura* aura_host = native_host();

  EXPECT_TRUE(child_win->HasObserver(aura_host));
  DestroyHost();
  EXPECT_FALSE(child_win->HasObserver(aura_host));
}

// Tests that the kHostViewKey is correctly set and cleared.
TEST_F(NativeViewHostAuraTest, HostViewPropertyKey) {
  // Create the NativeViewHost and attach a NativeView.
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));
  EXPECT_EQ(host()->GetWidget()->GetNativeView(),
            child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
  EXPECT_FALSE(child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_TRUE(clipping_window()->GetProperty(views::kHostViewKey));

  host()->Attach(child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));
  EXPECT_EQ(host()->GetWidget()->GetNativeView(),
            child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  DestroyHost();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
  EXPECT_FALSE(child_win->GetProperty(aura::client::kHostWindowKey));
}

// Tests that the NativeViewHost reports the cursor set on its native view.
TEST_F(NativeViewHostAuraTest, CursorForNativeView) {
  CreateHost();

  toplevel()->SetCursor(ui::mojom::CursorType::kHand);
  child()->SetCursor(ui::mojom::CursorType::kWait);
  ui::MouseEvent move_event(ui::EventType::kMouseMoved, gfx::Point(0, 0),
                            gfx::Point(0, 0), ui::EventTimeForNow(), 0, 0);

  EXPECT_EQ(ui::mojom::CursorType::kWait, host()->GetCursor(move_event).type());

  DestroyHost();
}

// Test that destroying the top level widget before destroying the attached
// NativeViewHost works correctly. Specifically the associated NVH should be
// destroyed and there shouldn't be any errors.
TEST_F(NativeViewHostAuraTest, DestroyWidget) {
  ResetHostDestroyedCount();
  CreateHost();
  ReleaseHost();
  EXPECT_EQ(0, host_destroyed_count());
  DestroyTopLevel();
  EXPECT_EQ(1, host_destroyed_count());
}

// Test that the fast resize path places the clipping and content windows were
// they are supposed to be.
TEST_F(NativeViewHostAuraTest, FastResizePath) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));

  // Without fast resize, the clipping window should size to the native view
  // with the native view positioned at the origin of the clipping window and
  // the clipping window positioned where the native view was requested.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(5, 10, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(5, 10, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  // With fast resize, the native view should remain the same size but be
  // clipped the requested size.
  host()->set_fast_resize(true);
  native_host()->ShowWidget(10, 25, 50, 50, 50, 50);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Turning off fast resize should make the native view start resizing again.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(10, 25, 50, 50, 50, 50);
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  DestroyHost();
}

// Test that the clipping and content windows' bounds are set to the correct
// values while the native size is not equal to the View size. During fast
// resize, the size and transform of the NativeView should not be modified.
TEST_F(NativeViewHostAuraTest, BoundsWhileScaling) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  EXPECT_EQ(gfx::Transform(), host()->native_view()->transform());

  // Without fast resize, the clipping window should size to the native view
  // with the native view positioned at the origin of the clipping window and
  // the clipping window positioned where the native view was requested. The
  // size of the native view should be 200x200 (so it's content will be
  // shown at half-size).
  host()->set_fast_resize(false);
  native_host()->ShowWidget(5, 10, 100, 100, 200, 200);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(5, 10, 100, 100).ToString(),
            clipping_window()->bounds().ToString());
  gfx::Transform expected_transform;
  expected_transform.Scale(0.5, 0.5);
  EXPECT_EQ(expected_transform, host()->native_view()->transform());

  // With fast resize, the native view should remain the same size but be
  // clipped the requested size. Also, its transform should not be changed.
  host()->set_fast_resize(true);
  native_host()->ShowWidget(10, 25, 50, 50, 200, 200);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());
  EXPECT_EQ(expected_transform, host()->native_view()->transform());

  // Turning off fast resize should make the native view start resizing again,
  // and its transform modified to show at the new quarter-size.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(10, 25, 50, 50, 200, 200);
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());
  expected_transform = gfx::Transform();
  expected_transform.Scale(0.25, 0.25);
  EXPECT_EQ(expected_transform, host()->native_view()->transform());

  // When the NativeView is detached, its original transform should be restored.
  auto* const detached_view = host()->native_view();
  host()->Detach();
  EXPECT_EQ(gfx::Transform(), detached_view->transform());
  // Attach it again so it's torn down with everything else at the end.
  host()->Attach(detached_view);

  DestroyHost();
}

// Test installing and uninstalling a clip.
TEST_F(NativeViewHostAuraTest, InstallClip) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  gfx::Rect client_bounds = toplevel()->client_view()->bounds();

  // Without a clip, the clipping window should always be positioned at the
  // requested coordinates with the native view positioned at the origin of the
  // clipping window.
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  // Clip to the bottom right quarter of the native view.
  native_host()->InstallClip(60 - client_bounds.x(), 70 - client_bounds.y(), 50,
                             50);
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(-50, -50, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 70, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Clip to the center of the native view.
  native_host()->InstallClip(35 - client_bounds.x(), 45 - client_bounds.y(), 50,
                             50);
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(-25, -25, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(35, 45, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Uninstalling the clip should make the clipping window match the native view
  // again.
  native_host()->UninstallClip();
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  DestroyHost();
}

// Ensure native view is parented to the root window after detaching. This is
// a regression test for http://crbug.com/389261.
TEST_F(NativeViewHostAuraTest, ParentAfterDetach) {
  CreateHost();
  // Trigger layout so that the visibility is set to false (because the bounds
  // is empty).
  test::RunScheduledLayout(host());

  aura::Window* child_win = child()->GetNativeView();
  aura::Window* root_window = child_win->GetRootWindow();
  aura::WindowTreeHost* child_win_tree_host = child_win->GetHost();

  NativeViewHostWindowObserver test_observer;
  child_win->AddObserver(&test_observer);

  host()->Detach();
  EXPECT_EQ(root_window, child_win->GetRootWindow());
  EXPECT_EQ(child_win_tree_host, child_win->GetHost());

  DestroyHost();
  DestroyTopLevel();
  // The window is detached, so no longer associated with any Widget
  // hierarchy. The root window still owns it, but the test harness checks
  // for orphaned windows during TearDown().
  EXPECT_EQ(0u, test_observer.events().size())
      << (*test_observer.events().begin()).type;
  delete child_win;

  ASSERT_EQ(1u, test_observer.events().size());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_DESTROYED,
            test_observer.events().back().type);
}

// Ensure the clipping window is hidden before any other operations.
// This is a regression test for http://crbug.com/388699.
TEST_F(NativeViewHostAuraTest, RemoveClippingWindowOrder) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);

  NativeViewHostWindowObserver test_observer;
  clipping_window()->AddObserver(&test_observer);

  aura::Window* child_win = child()->GetNativeView();
  child_win->AddObserver(&test_observer);

  host()->Detach();

  ASSERT_GE(test_observer.events().size(), 1u);
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_HIDDEN,
            test_observer.events()[0].type);
  EXPECT_EQ(clipping_window()->GetId(), test_observer.events()[0].window_id);

  clipping_window()->RemoveObserver(&test_observer);
  child()->GetNativeView()->RemoveObserver(&test_observer);

  DestroyHost();
  delete child_win;  // See comments in ParentAfterDetach.
}

// Ensure the native view receives the correct bounds notification when it is
// attached. This is a regression test for https://crbug.com/399420.
TEST_F(NativeViewHostAuraTest, Attach) {
  CreateHost();
  host()->Detach();

  child()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 0, 0));
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  gfx::Rect client_bounds = toplevel()->client_view()->bounds();
  host()->SetBoundsRect(client_bounds);

  NativeViewHostWindowObserver test_observer;
  child()->GetNativeView()->AddObserver(&test_observer);

  host()->Attach(child()->GetNativeView());

  // Visibiliity is not updated until layout happens.
  test::RunScheduledLayout(host());

  auto expected_bounds = client_bounds;

  ASSERT_EQ(3u, test_observer.events().size());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_BOUNDS_CHANGED,
            test_observer.events()[0].type);
  EXPECT_EQ(child()->GetNativeView()->GetId(),
            test_observer.events()[0].window_id);
  EXPECT_EQ(expected_bounds.ToString(),
            test_observer.events()[0].bounds.ToString());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_SHOWN,
            test_observer.events()[1].type);
  EXPECT_EQ(child()->GetNativeView()->GetId(),
            test_observer.events()[1].window_id);
  EXPECT_EQ(expected_bounds.ToString(),
            test_observer.events()[1].bounds.ToString());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_SHOWN,
            test_observer.events()[2].type);
  EXPECT_EQ(clipping_window()->GetId(), test_observer.events()[2].window_id);
  EXPECT_EQ(expected_bounds.ToString(),
            test_observer.events()[2].bounds.ToString());

  child()->GetNativeView()->RemoveObserver(&test_observer);
  DestroyHost();
}

// Ensure the clipping window is hidden with the native view. This is a
// regression test for https://crbug.com/408877.
TEST_F(NativeViewHostAuraTest, SimpleShowAndHide) {
  CreateHost();

  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();

  host()->SetBounds(10, 10, 80, 80);
  EXPECT_TRUE(clipping_window()->IsVisible());
  EXPECT_TRUE(child()->IsVisible());

  host()->SetVisible(false);
  EXPECT_FALSE(clipping_window()->IsVisible());
  EXPECT_FALSE(child()->IsVisible());

  DestroyHost();
  DestroyTopLevel();
}

namespace {

class TestFocusChangeListener : public FocusChangeListener {
 public:
  explicit TestFocusChangeListener(FocusManager* focus_manager)
      : focus_manager_(focus_manager) {
    focus_manager_->AddFocusChangeListener(this);
  }

  TestFocusChangeListener(const TestFocusChangeListener&) = delete;
  TestFocusChangeListener& operator=(const TestFocusChangeListener&) = delete;

  ~TestFocusChangeListener() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  int did_change_focus_count() const { return did_change_focus_count_; }

 private:
  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override {}
  void OnDidChangeFocus(View* focused_before, View* focused_now) override {
    did_change_focus_count_++;
  }

  raw_ptr<FocusManager> focus_manager_;
  int did_change_focus_count_ = 0;
};

}  // namespace

// Verifies the FocusManager is properly updated if focus is in a child widget
// that is parented to a NativeViewHost and the NativeViewHost is destroyed.
TEST_F(NativeViewHostAuraTest, FocusManagerUpdatedDuringDestruction) {
  CreateTopLevel();
  toplevel()->Show();

  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_owned_by_parent(false);

  std::unique_ptr<NativeViewHost> native_view_host =
      std::make_unique<NativeViewHost>();
  toplevel()->GetContentsView()->AddChildView(native_view_host.get());

  auto widget_delegate_view = std::make_unique<WidgetDelegateView>();
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_CONTROL);
  // Delegate is "owned" via the view and will be deleted with it.
  params.delegate = widget_delegate_view.release();
  params.child = true;
  params.bounds = gfx::Rect(10, 10, 100, 100);
  params.parent = window.get();
  std::unique_ptr<Widget> child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(params));

  native_view_host->Attach(window.get());

  View* view1 = child_widget->GetContentsView()->AddChildView(
      std::make_unique<View>());  // Owned by |child_widget|.
  view1->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  view1->SetBounds(0, 0, 20, 20);
  child_widget->Show();
  view1->RequestFocus();
  EXPECT_EQ(view1, toplevel()->GetFocusManager()->GetFocusedView());

  TestFocusChangeListener focus_change_listener(toplevel()->GetFocusManager());

  // ~NativeViewHost() unparents |window|.
  native_view_host.reset();

  EXPECT_EQ(nullptr, toplevel()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(1, focus_change_listener.did_change_focus_count());

  child_widget.reset();
  EXPECT_EQ(nullptr, toplevel()->GetFocusManager()->GetFocusedView());
}

namespace {

ui::EventTarget* GetTarget(aura::Window* window, const gfx::Point& location) {
  gfx::Point root_location = location;
  aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                     &root_location);
  ui::MouseEvent event(ui::EventType::kMouseMoved, root_location, root_location,
                       base::TimeTicks::Now(), 0, 0);
  return window->GetHost()->dispatcher()->event_targeter()->FindTargetForEvent(
      window->GetRootWindow(), &event);
}

}  // namespace

TEST_F(NativeViewHostAuraTest, TopInsets) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();
  // The child window is placed relative to the client view. Take that into
  // account.
  gfx::Vector2d offset = toplevel()->client_view()->bounds().OffsetFromOrigin();

  aura::Window* toplevel_window = toplevel()->GetNativeWindow();
  aura::Window* child_window = child()->GetNativeWindow();
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 1) + offset));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  host()->SetHitTestTopInset(10);
  EXPECT_EQ(toplevel_window, GetTarget(toplevel_window, gfx::Point(1, 1)));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  host()->SetHitTestTopInset(0);
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 1) + offset));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  DestroyHost();
  DestroyTopLevel();
}

TEST_F(NativeViewHostAuraTest, WindowHiddenWhenAttached) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_owned_by_parent(false);
  window->Show();
  EXPECT_TRUE(window->TargetVisibility());
  CreateTopLevel();
  NativeViewHost* host = toplevel()->GetRootView()->AddChildView(
      std::make_unique<NativeViewHost>());
  host->SetVisible(false);
  host->Attach(window.get());
  // Is |host| is not visible, |window| should immediately be hidden.
  EXPECT_FALSE(window->TargetVisibility());
}

TEST_F(NativeViewHostAuraTest, ClippedWindowNotResizedOnDetach) {
  CreateTopLevel();
  toplevel()->SetSize(gfx::Size(100, 100));
  toplevel()->Show();

  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_owned_by_parent(false);
  window->SetBounds(gfx::Rect(0, 0, 200, 200));
  window->Show();

  NativeViewHost* host = toplevel()->GetRootView()->AddChildView(
      std::make_unique<NativeViewHost>());
  host->SetVisible(true);
  host->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  host->Attach(window.get());
  EXPECT_EQ(gfx::Size(200, 200), window->bounds().size());
  host->Detach();
  EXPECT_EQ(gfx::Size(200, 200), window->bounds().size());
}

class WidgetDelegateForShouldDescendIntoChildForEventHandling
    : public WidgetDelegate {
 public:
  void set_window(aura::Window* window) { window_ = window; }

  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override {
    return child != window_;
  }

 private:
  raw_ptr<aura::Window> window_ = nullptr;
};

TEST_F(NativeViewHostAuraTest, ShouldDescendIntoChildForEventHandling) {
  WidgetDelegateForShouldDescendIntoChildForEventHandling widget_delegate;
  CreateTopLevel(&widget_delegate);
  toplevel()->SetSize(gfx::Size(200, 200));
  toplevel()->Show();

  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->set_owned_by_parent(false);
  window->SetBounds(gfx::Rect(0, 0, 200, 200));
  window->Show();

  widget_delegate.set_window(window.get());

  CreateTestingHost();
  toplevel()->GetRootView()->AddChildView(host());
  host()->SetVisible(true);
  host()->SetBoundsRect(gfx::Rect(0, 0, 200, 200));
  host()->Attach(window.get());

  ui::test::EventGenerator event_generator(window->GetRootWindow());
  gfx::Point press_location(100, 100);
  aura::Window::ConvertPointToTarget(toplevel()->GetNativeView(),
                                     window->GetRootWindow(), &press_location);
  event_generator.MoveMouseTo(press_location);
  event_generator.PressLeftButton();
  // Because the delegate overrides ShouldDescendIntoChildForEventHandling()
  // the NativeView does not get the event, but NativeViewHost will.
  EXPECT_EQ(1, on_mouse_pressed_called_count());
  widget_delegate.set_window(nullptr);
  DestroyHost();
  DestroyTopLevel();
}

}  // namespace views
