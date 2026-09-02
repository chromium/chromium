// This test ensures that a native view can be attached to a NativeViewHost
// even if it's currently attached to a NativeViewHost in a different
// WindowTreeHost. It guards against crashes where WindowObservers query the
// root window or bounds in root during reparenting transitions between
// different root windows (e.g., https://crbug.com/1516544).
// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_aura_with_clip_window.h"
#include "ui/views/controls/native/native_view_host_test_base.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/default_activation_client.h"

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
    bool operator==(const EventDetails& rhs) const = default;
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
    if (events_.size() == 0u || events_.back() != event) {
      events_.push_back(event);
    }
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

  NativeViewHostWrapper* native_host() { return GetNativeWrapper(); }

  Widget* child_widget() { return child_.get(); }

  aura::Window* clipping_window() {
    return static_cast<NativeViewHostAuraWithClipWindow*>(GetNativeWrapper())
        ->clipping_window_.get();
  }

  aura::Window* native_view() { return host()->native_view(); }

  aura::WindowObserver* GetWindowObserver() {
    if (base::FeatureList::IsEnabled(
            views::features::kNativeViewHostManagesLayers)) {
      return static_cast<NativeViewHostAura*>(GetNativeWrapper());
    }
    return static_cast<NativeViewHostAuraWithClipWindow*>(GetNativeWrapper());
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

 protected:
  std::unique_ptr<Widget> child_;
};

// Verifies NativeViewHostAura stops observing native view on destruction.
TEST_F(NativeViewHostAuraTest, StopObservingNativeViewOnDestruct) {
  CreateHost();
  aura::Window* child_win = child_widget()->GetNativeView();
  aura::WindowObserver* aura_host = GetWindowObserver();

  EXPECT_TRUE(child_win->HasObserver(aura_host));
  DestroyHost();
  EXPECT_FALSE(child_win->HasObserver(aura_host));
}

// Tests that the kHostViewKey is correctly set and cleared in legacy mode
// (layer managed by parent window).
TEST_F(NativeViewHostAuraTest, HostViewPropertyKeyLegacy) {
  if (base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateTopLevel();
  CreateTestingHost();

  child_ = CreateChildForHost(toplevel()->GetNativeView(),
                              toplevel()->client_view(), new View, host());

  aura::Window* child_win = child_widget()->GetNativeView();
  EXPECT_EQ(native_view(), child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
  EXPECT_FALSE(native_view());  // detached.

  host()->Attach(child_win);
  EXPECT_EQ(native_view(), child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));

  DestroyHost();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
}

// Tests that the kHostViewKey is NOT set in default mode (layer managed by
// views).
TEST_F(NativeViewHostAuraTest, HostViewPropertyKeyManaged) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }

  CreateTopLevel();
  CreateTestingHost();

  child_ = CreateChildForHost(toplevel()->GetNativeView(),
                              toplevel()->client_view(), new View, host());

  aura::Window* child_win = child_widget()->GetNativeView();
  EXPECT_EQ(native_view(), child_win);
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));

  host()->Attach(child_win);
  EXPECT_EQ(native_view(), child_win);
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));

  DestroyHost();
}

// Tests that the NativeViewHost reports the cursor set on its native view.
TEST_F(NativeViewHostAuraTest, CursorForNativeView) {
  CreateHost();

  toplevel()->SetCursor(ui::mojom::CursorType::kHand);
  child_widget()->SetCursor(ui::mojom::CursorType::kWait);
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
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));

  // Without fast resize, the clipping window should size to the native view
  // with the native view positioned at the origin of the clipping window and
  // the clipping window positioned where the native view was requested.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(5, 10, 100, 100, 100, 100);
  EXPECT_EQ(gfx::Rect(5, 10, 100, 100), host()->native_view()->bounds());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());

  // With fast resize, the native view should remain the same size but be
  // clipped to the requested size.
  host()->set_fast_resize(true);
  native_host()->ShowWidget(10, 25, 50, 50, 50, 50);
  EXPECT_EQ(gfx::Rect(10, 25, 100, 100), host()->native_view()->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50),
            host()->native_view()->layer()->clip_rect());

  // Turning off fast resize should make the native view start resizing again.
  host()->set_fast_resize(false);
  host()->DeprecatedLayoutImmediately();  // Emulate the relayout to remove the
                                          // clip
  native_host()->ShowWidget(10, 25, 50, 50, 50, 50);
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50), host()->native_view()->bounds());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());

  DestroyHost();
}

// Test that the clipping and content windows' bounds are set to the correct
// values while the native size is not equal to the View size. During fast
// resize, the size and transform of the NativeView should not be modified.
TEST_F(NativeViewHostAuraTest, BoundsWhileScaling) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
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
  EXPECT_EQ(gfx::Rect(5, 10, 200, 200), host()->native_view()->bounds());

  gfx::Transform expected_transform;
  expected_transform.Scale(0.5, 0.5);
  EXPECT_EQ(expected_transform, host()->native_view()->transform());

  // With fast resize, the native view should remain the same size but be
  // clipped the requested size. Also, its transform should not be changed.
  host()->set_fast_resize(true);
  native_host()->ShowWidget(10, 25, 50, 50, 200, 200);
  EXPECT_EQ(gfx::Rect(10, 25, 200, 200), host()->native_view()->bounds());
  EXPECT_EQ(expected_transform, host()->native_view()->transform());

  // Turning off fast resize should make the native view start resizing again,
  // and its transform modified to show at the new quarter-size.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(10, 25, 50, 50, 200, 200);
  EXPECT_EQ(gfx::Rect(10, 25, 200, 200), host()->native_view()->bounds());

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
  if (base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
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

class NativeViewHostAuraClipTest : public NativeViewHostAuraTest,
                                   public testing::WithParamInterface<bool> {
 public:
  NativeViewHostAuraClipTest() = default;
  NativeViewHostAuraClipTest(const NativeViewHostAuraClipTest&) = delete;
  NativeViewHostAuraClipTest& operator=(const NativeViewHostAuraClipTest&) =
      delete;

 protected:
  const bool apply_rounded_corners = GetParam();
};

INSTANTIATE_TEST_SUITE_P(All, NativeViewHostAuraClipTest, testing::Bool());

TEST_P(NativeViewHostAuraClipTest, ClipByParent) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(0, 0, 500, 500));
  toplevel()->Show();

  View* container = new View();
  toplevel()->client_view()->AddChildView(container);

  View* parent_view = new View();
  parent_view->SetBounds(100, 100, 50, 50);
  container->AddChildView(parent_view);
  parent_view->AddChildView(host());

  const gfx::RoundedCornersF kRadii(5, 10, 15, 20);
  if (apply_rounded_corners) {
    host()->SetNativeViewCornerRadii(kRadii);
  }

  // 1) child view is fully visible (same size as parent), so it shouldn't be
  // clipped.
  host()->SetBoundsRect(gfx::Rect(0, 0, 50, 50));
  test::RunScheduledLayout(toplevel());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(kRadii, host()->native_view()->layer()->rounded_corner_radii());
  }

  // 2) child view is fully visible (smaller than parent), so it shouldn't be
  // clipped.
  host()->SetBoundsRect(gfx::Rect(5, 10, 30, 30));
  test::RunScheduledLayout(toplevel());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(kRadii, host()->native_view()->layer()->rounded_corner_radii());
  }

  // 3) child view is bigger than parent in both direction (width and height),
  // so only a non-center part is visible.
  host()->SetBoundsRect(gfx::Rect(-20, -10, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(20, 10, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 4) child view's origin is aligned with parent's so only top left is
  // visible.
  host()->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(5, 0, 0, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 5) child view's bottom right corner is aligned with parent's bottom right,
  // so only bottom right part is visible.
  host()->SetBoundsRect(gfx::Rect(-50, -50, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(50, 50, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 15, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 6) the child view is outside of parent, so entire window is clipped. (not
  // visible).
  host()->SetBoundsRect(gfx::Rect(100, 100, 50, 50));
  test::RunScheduledLayout(toplevel());
  EXPECT_FALSE(host()->native_view()->IsVisible());

  // 7) child view is placed so that it intersects with the parent but only
  // left bottom is visible (origin y is smaller and right x is bigger).
  host()->SetBoundsRect(gfx::Rect(20, -20, 40, 40));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(0, 20, 30, 20),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 0, 20),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 8) child is placed so that it intersects with the parent but only
  // top right is visible (bottom y is bigger and origin x is smaller).
  host()->SetBoundsRect(gfx::Rect(-20, 20, 40, 40));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(20, 0, 20, 30),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 10, 0, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  DestroyHost();
}

TEST_P(NativeViewHostAuraClipTest, ClipByParentRTL) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_("he");

  CreateHost();
  toplevel()->SetBounds(gfx::Rect(0, 0, 500, 500));
  toplevel()->Show();

  View* container = new View();
  toplevel()->client_view()->AddChildView(container);

  View* parent_view = new View();
  parent_view->SetBounds(100, 100, 50, 50);
  container->AddChildView(parent_view);
  parent_view->AddChildView(host());

  const gfx::RoundedCornersF kRadii(5, 10, 15, 20);
  const gfx::RoundedCornersF kMirroredRadii(10, 5, 20, 15);
  if (apply_rounded_corners) {
    host()->SetNativeViewCornerRadii(kRadii);
  }

  // 1) child view is fully visible (same size as parent), so it shouldn't be
  // clipped.
  host()->SetBoundsRect(gfx::Rect(0, 0, 50, 50));
  test::RunScheduledLayout(toplevel());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(kMirroredRadii,
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 2) child view is fully visible (smaller than parent), so it shouldn't be
  // clipped.
  host()->SetBoundsRect(gfx::Rect(5, 10, 30, 30));
  test::RunScheduledLayout(toplevel());
  EXPECT_TRUE(host()->native_view()->layer()->clip_rect().IsEmpty());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(kMirroredRadii,
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 3) child view is bigger than parent in both direction (width and height),
  // so only a non-center part is visible.
  host()->SetBoundsRect(gfx::Rect(-20, -10, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(30, 10, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 4) child view's origin is aligned with parent's so only top right is
  // visible in RTL. Logical (0, 0) means right side. Physical clip should be on
  // left.
  host()->SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(50, 0, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 5, 0, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 5) child view's bottom right corner is aligned with parent's bottom right,
  // so only bottom left part is visible in RTL.
  host()->SetBoundsRect(gfx::Rect(-50, -50, 100, 100));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(0, 50, 50, 50),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 0, 15),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 6) the child view is outside of parent, so entire window is clipped. (not
  // visible).
  host()->SetBoundsRect(gfx::Rect(100, 100, 50, 50));
  test::RunScheduledLayout(toplevel());
  EXPECT_FALSE(host()->native_view()->IsVisible());

  // 7) child view is placed so that it intersects with the parent but only
  // left bottom is visible logically (origin y is smaller and right x is
  // bigger).
  host()->SetBoundsRect(gfx::Rect(20, -20, 40, 40));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 20),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 20, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  // 8) child is placed so that it intersects with the parent but only
  // top right is visible logically (bottom y is bigger and origin x is
  // smaller).
  host()->SetBoundsRect(gfx::Rect(-20, 20, 40, 40));
  test::RunScheduledLayout(toplevel());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 30),
            host()->native_view()->layer()->clip_rect());
  EXPECT_TRUE(host()->native_view()->IsVisible());
  if (apply_rounded_corners) {
    EXPECT_EQ(gfx::RoundedCornersF(10, 0, 0, 0),
              host()->native_view()->layer()->rounded_corner_radii());
  }

  DestroyHost();
}

// Ensure native view is parented to the root window after detaching. This is
// a regression test for http://crbug.com/389261.
TEST_F(NativeViewHostAuraTest, ParentAfterDetach) {
  CreateHost();
  // Trigger layout so that the visibility is set to false (because the bounds
  // is empty).
  test::RunScheduledLayout(host());

  aura::Window* child_win = child_widget()->GetNativeView();
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
  if (base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  native_host()->ShowWidget(10, 20, 100, 100, 100, 100);

  NativeViewHostWindowObserver test_observer;
  clipping_window()->AddObserver(&test_observer);

  aura::Window* child_win = child_widget()->GetNativeView();
  child_win->AddObserver(&test_observer);

  host()->Detach();

  ASSERT_GE(test_observer.events().size(), 1u);
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_HIDDEN,
            test_observer.events()[0].type);
  EXPECT_EQ(clipping_window()->GetId(), test_observer.events()[0].window_id);

  clipping_window()->RemoveObserver(&test_observer);
  child_widget()->GetNativeView()->RemoveObserver(&test_observer);

  DestroyHost();
}

// Ensure the native view receives the correct bounds notification when it is
// attached. This is a regression test for https://crbug.com/399420.
TEST_F(NativeViewHostAuraTest, Attach) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateHost();
  host()->Detach();

  child_widget()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 0, 0));
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  gfx::Rect client_bounds = toplevel()->client_view()->bounds();
  host()->SetBoundsRect(client_bounds);

  NativeViewHostWindowObserver test_observer;
  child_widget()->GetNativeView()->AddObserver(&test_observer);

  host()->Attach(child_widget()->GetNativeView());

  // Visibiliity is not updated until layout happens.
  test::RunScheduledLayout(host());

  auto expected_bounds = client_bounds;

  ASSERT_EQ(2u, test_observer.events().size());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_BOUNDS_CHANGED,
            test_observer.events()[0].type);
  EXPECT_EQ(child_widget()->GetNativeView()->GetId(),
            test_observer.events()[0].window_id);
  EXPECT_EQ(expected_bounds, test_observer.events()[0].bounds);
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_SHOWN,
            test_observer.events()[1].type);
  EXPECT_EQ(child_widget()->GetNativeView()->GetId(),
            test_observer.events()[1].window_id);
  EXPECT_EQ(expected_bounds, test_observer.events()[1].bounds);

  child_widget()->GetNativeView()->RemoveObserver(&test_observer);
  DestroyHost();
}

TEST_F(NativeViewHostAuraTest, HostMovedWithSameLayerBounds) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }
  CreateTopLevel();
  toplevel()->SetBounds({0, 0, 200, 200});

  View* container =
      toplevel()->client_view()->AddChildView(std::make_unique<View>());

  NativeViewHost* host1 =
      container->AddChildView(std::make_unique<NativeViewHost>());
  host1->SetBoundsRect({10, 10, 80, 80});

  NativeViewHost* host2 =
      container->AddChildView(std::make_unique<NativeViewHost>());
  host2->SetBoundsRect({30, 40, 80, 80});

  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
  child_params.parent = toplevel()->GetNativeView();
  child->Init(std::move(child_params));
  child->SetContentsView(new View);

  aura::Window* child_win = child->GetNativeView();
  host1->Attach(child_win);
  test::RunScheduledLayout(toplevel());

  const gfx::Point client_origin = toplevel()->client_view()->bounds().origin();
  const gfx::Rect bounds_in_host1{client_origin.x() + 10,
                                  client_origin.y() + 10, 80, 80};
  EXPECT_EQ(bounds_in_host1, child_win->bounds());
  EXPECT_EQ((gfx::Rect{80, 80}), child_win->layer()->bounds());

  NativeViewHostWindowObserver test_observer;
  child_win->AddObserver(&test_observer);

  // Detach from host1 and attach to host2, which has the same size but a
  // different location. The child window's layer bounds relative to
  // host2->layer() remains (0, 0, 80, 80). Because the layer bounds do not
  // change, Layer::SetBounds() is a no-op, but Window::SetBoundsInternal must
  // still update the child window's bounds to match host2's position
  // and notify observers.
  host1->Detach();
  host2->Attach(child_win);
  test::RunScheduledLayout(toplevel());

  const gfx::Rect bounds_in_host2{client_origin.x() + 30,
                                  client_origin.y() + 40, 80, 80};
  EXPECT_EQ(bounds_in_host2, child_win->bounds());
  EXPECT_EQ((gfx::Rect{80, 80}), child_win->layer()->bounds());

  bool found_bounds_changed = false;
  for (const auto& event : test_observer.events()) {
    if (event.type == NativeViewHostWindowObserver::EVENT_BOUNDS_CHANGED) {
      found_bounds_changed = true;
      break;
    }
  }
  EXPECT_TRUE(found_bounds_changed);

  child_win->RemoveObserver(&test_observer);
  host2->Detach();
}

// Ensure the native window is hidden when the host view is hidden.
TEST_F(NativeViewHostAuraTest, SimpleShowAndHide) {
  CreateHost();

  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();

  host()->SetBounds(10, 10, 80, 80);
  EXPECT_TRUE(child_widget()->IsVisible());

  host()->SetVisible(false);
  EXPECT_FALSE(child_widget()->IsVisible());

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
  toplevel()->GetContentsView()->AddChildViewRaw(native_view_host.get());

  auto widget_delegate_view =
      std::make_unique<WidgetDelegateView>(WidgetDelegateView::CreatePassKey());
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

class NativeViewHostAuraTopInsetsTest
    : public NativeViewHostAuraTest,
      public testing::WithParamInterface<bool> {
 public:
  NativeViewHostAuraTopInsetsTest() = default;
  ~NativeViewHostAuraTopInsetsTest() override = default;

  void SetUp() override {
    if (use_clip_window) {
      feature_list_.InitAndDisableFeature(
          views::features::kNativeViewHostManagesLayers);
    } else {
      feature_list_.InitAndEnableFeature(
          views::features::kNativeViewHostManagesLayers);
    }
    NativeViewHostAuraTest::SetUp();
    if (base::FeatureList::IsEnabled(
            views::features::kNativeViewHostManagesLayers)) {
      GTEST_SKIP() << "NativeViewHostManagesLayers is enabled";
    }
  }

 protected:
  const bool use_clip_window = GetParam();

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, NativeViewHostAuraTopInsetsTest, testing::Bool());

TEST_P(NativeViewHostAuraTopInsetsTest, TopInsets) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();
  // The child window is placed relative to the client view. Take that into
  // account.
  gfx::Vector2d offset = toplevel()->client_view()->bounds().OffsetFromOrigin();

  aura::Window* toplevel_window = toplevel()->GetNativeWindow();
  aura::Window* child_window = child_widget()->GetNativeWindow();
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 1) + offset));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  host()->SetHitTestTopInset(10);
  EXPECT_EQ(toplevel_window, GetTarget(toplevel_window, gfx::Point(1, 1)));
  EXPECT_EQ(toplevel_window,
            GetTarget(toplevel_window, gfx::Point(1, 1) + offset));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  // Reset.
  host()->SetHitTestTopInset(0);
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 1) + offset));
  EXPECT_EQ(child_window,
            GetTarget(toplevel_window, gfx::Point(1, 11) + offset));

  DestroyHost();
  DestroyTopLevel();
}

TEST_P(NativeViewHostAuraTopInsetsTest, SetNativeViewClipRect) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();

  aura::Window* child_window = child_widget()->GetNativeWindow();
  ui::Layer* child_layer = child_window->layer();
  ASSERT_TRUE(child_layer);

  gfx::Rect host_bounds = host()->GetLocalBounds();
  ASSERT_FALSE(host_bounds.IsEmpty());

  // Initial state: no clip.
  EXPECT_TRUE(child_layer->clip_rect().IsEmpty());

  // Set clip.
  gfx::Rect clip(10, 10, 30, 30);
  EXPECT_TRUE(host()->SetNativeViewClipRect(clip));
  EXPECT_EQ(clip, child_layer->clip_rect());

  // Set same clip again: should return false.
  EXPECT_FALSE(host()->SetNativeViewClipRect(clip));
  EXPECT_EQ(clip, child_layer->clip_rect());

  // Set different clip.
  gfx::Rect clip2(20, 20, 20, 20);
  EXPECT_TRUE(host()->SetNativeViewClipRect(clip2));
  EXPECT_EQ(clip2, child_layer->clip_rect());

  // Clear clip.
  EXPECT_TRUE(host()->SetNativeViewClipRect(gfx::Rect()));
  EXPECT_TRUE(child_layer->clip_rect().IsEmpty());

  DestroyHost();
  DestroyTopLevel();
}

TEST_P(NativeViewHostAuraTopInsetsTest,
       SetNativeViewClipRectWithRoundedCorners) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();

  aura::Window* child_window = child_widget()->GetNativeWindow();
  ui::Layer* child_layer = child_window->layer();
  ASSERT_TRUE(child_layer);

  gfx::Size host_size = host()->bounds().size();
  int host_width = host_size.width();
  int host_height = host_size.height();
  ASSERT_GE(host_width, 50);
  ASSERT_GE(host_height, 50);

  // Set corner radii.
  const gfx::RoundedCornersF kRadii(5, 10, 15, 20);
  EXPECT_TRUE(host()->SetNativeViewCornerRadii(kRadii));

  // If no clip, rounded corners should be applied.
  EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());

  // Set external clip that clips right and bottom edges.
  gfx::Rect clip(0, 0, host_width - 10, host_height - 10);
  EXPECT_TRUE(host()->SetNativeViewClipRect(clip));

  if (use_clip_window) {
    // NativeViewHostAuraWithClipWindow: corners are not adjusted by clip.
    EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());
  } else {
    // NativeViewHostAura: corners should be adjusted by clip.
    // Only upper-left corner is not clipped.
    EXPECT_EQ(gfx::RoundedCornersF(5, 0, 0, 0),
              child_layer->rounded_corner_radii());
  }

  // Clear clip.
  EXPECT_TRUE(host()->SetNativeViewClipRect(gfx::Rect()));
  EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());

  DestroyHost();
  DestroyTopLevel();
}

TEST_P(NativeViewHostAuraTopInsetsTest,
       SetNativeViewClipRectWithTopInsetsAndRoundedCorners) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  toplevel()->Show();

  aura::Window* child_window = child_widget()->GetNativeWindow();
  ui::Layer* child_layer = child_window->layer();
  ASSERT_TRUE(child_layer);

  gfx::Size host_size = host()->bounds().size();
  int host_width = host_size.width();
  int host_height = host_size.height();
  ASSERT_GE(host_width, 50);
  ASSERT_GE(host_height, 50);

  // Set corner radii.
  const gfx::RoundedCornersF kRadii(5, 10, 15, 20);
  EXPECT_TRUE(host()->SetNativeViewCornerRadii(kRadii));

  // Set top inset.
  host()->SetHitTestTopInset(20);

  // Set external clip.
  gfx::Rect clip(0, 0, host_width - 10, host_height + 10);
  EXPECT_TRUE(host()->SetNativeViewClipRect(clip));

  if (use_clip_window) {
    // NativeViewHostAuraWithClipWindow:
    // Clip on child layer is just the external clip.
    EXPECT_EQ(clip, child_layer->clip_rect());
    // Corners are not adjusted.
    EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());
  } else {
    // NativeViewHostAura:
    // Combined clip: clip (0, 0, host_width-10, host_height+10) intersect
    // top_inset (0, 20, host_width, host_height-20) = (0, 20, host_width-10,
    // host_height-20).
    EXPECT_EQ(gfx::Rect(0, 20, host_width - 10, host_height - 20),
              child_layer->clip_rect());
    // Corners are adjusted based on combined clip (Top and Right clipped).
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 0, 20),
              child_layer->rounded_corner_radii());
  }

  // Clear external clip.
  EXPECT_TRUE(host()->SetNativeViewClipRect(gfx::Rect()));
  if (use_clip_window) {
    EXPECT_TRUE(child_layer->clip_rect().IsEmpty());
    EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());
  } else {
    // Still has top_inset clip: (0, 20, host_width, host_height-20).
    EXPECT_EQ(gfx::Rect(0, 20, host_width, host_height - 20),
              child_layer->clip_rect());
    // Corners adjusted based on top_inset clip (Top clipped).
    EXPECT_EQ(gfx::RoundedCornersF(0, 0, 15, 20),
              child_layer->rounded_corner_radii());
  }

  // Reset top inset.
  host()->SetHitTestTopInset(0);
  EXPECT_TRUE(child_layer->clip_rect().IsEmpty());
  EXPECT_EQ(kRadii, child_layer->rounded_corner_radii());

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
  toplevel()->GetRootView()->AddChildViewRaw(host());
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

TEST(NativeViewHostFeatureTest, FeatureFlagControlsDefault) {
  // When feature is disabled, default should be false.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        views::features::kNativeViewHostManagesLayers);
    NativeViewHost host;
    EXPECT_FALSE(host.layer_managed_by_views());
  }

  // When feature is enabled, default should be true.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        views::features::kNativeViewHostManagesLayers);
    NativeViewHost host;
    EXPECT_TRUE(host.layer_managed_by_views());
  }
}

TEST_F(NativeViewHostAuraTest, LayerHierarchyManaged) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }

  CreateTopLevel();
  CreateTestingHost();

  child_ = CreateChildForHost(toplevel()->GetNativeView(),
                              toplevel()->client_view(), new View, host());

  aura::Window* child_win = child_widget()->GetNativeView();
  ui::Layer* child_layer = child_win->layer();
  ui::Layer* host_layer = host()->layer();

  EXPECT_TRUE(host_layer);
  EXPECT_EQ(host_layer, child_layer->parent());

  host()->Detach();
  EXPECT_FALSE(host()->layer());
  // After detach, child_win is reparented to root window.
  EXPECT_TRUE(child_win->parent());
  EXPECT_EQ(child_win->parent()->layer(), child_layer->parent());

  host()->Attach(child_win);
  EXPECT_TRUE(host()->layer());
  EXPECT_EQ(host()->layer(), child_layer->parent());

  DestroyHost();
}

TEST_F(NativeViewHostAuraTest, LayerHierarchyLegacy) {
  if (base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }

  CreateTopLevel();
  CreateTestingHost();

  child_ = CreateChildForHost(toplevel()->GetNativeView(),
                              toplevel()->client_view(), new View, host());

  EXPECT_FALSE(host()->layer());

  aura::Window* child_win = child_widget()->GetNativeView();

  ui::Layer* child_layer = child_win->layer();
  aura::Window* parent_win = toplevel()->GetNativeView();

  // Window Hierarchy: parent_win->clip_widow->child_win;
  EXPECT_EQ(parent_win->layer(), child_layer->parent()->parent());

  host()->Detach();
  EXPECT_FALSE(host()->layer());
  // After detach, child_win is reparented to root window.
  EXPECT_TRUE(child_win->parent());
  EXPECT_EQ(child_win->parent()->layer(), child_layer->parent());

  host()->Attach(child_win);
  EXPECT_FALSE(host()->layer());
  EXPECT_EQ(parent_win->layer(), child_layer->parent()->parent());

  DestroyHost();
}

TEST_F(NativeViewHostAuraTest, NestedLayerHierarchy) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }

  CreateTopLevel();
  toplevel()->SetBounds(gfx::Rect(0, 0, 500, 500));
  toplevel()->Show();

  View* container =
      toplevel()->client_view()->AddChildView(std::make_unique<View>());
  View* parent_view = new View();
  parent_view->SetPaintToLayer();
  container->AddChildView(parent_view);
  parent_view->SetBounds(10, 10, 100, 100);

  CreateTestingHost();

  child_ = CreateChildForHost(toplevel()->GetNativeView(), parent_view,
                              new View, host());
  host()->SetBounds(5, 5, 50, 50);

  test::RunScheduledLayout(toplevel());

  aura::Window* child_win = child_widget()->GetNativeView();
  ui::Layer* child_layer = child_win->layer();
  ui::Layer* host_layer = host()->layer();
  ui::Layer* parent_layer = parent_view->layer();

  EXPECT_TRUE(host_layer);
  EXPECT_EQ(parent_layer, host_layer->parent());
  EXPECT_EQ(host_layer, child_layer->parent());

  // Account for client view offset in widget.
  gfx::Vector2d offset = toplevel()->client_view()->bounds().OffsetFromOrigin();
  gfx::Point expected_origin = gfx::Point(15, 15) + offset;

  EXPECT_EQ(gfx::Rect(expected_origin, gfx::Size(50, 50)), child_win->bounds());
  EXPECT_EQ(gfx::Rect(5, 5, 50, 50), host_layer->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), child_layer->bounds());

  EXPECT_TRUE(child_win->IsVisible());
  EXPECT_TRUE(child_layer->visible());

  parent_view->SetVisible(false);
  EXPECT_FALSE(child_win->IsVisible());
  EXPECT_FALSE(child_layer->visible());

  parent_view->SetVisible(true);
  test::RunScheduledLayout(toplevel());
  EXPECT_TRUE(child_win->IsVisible());
  EXPECT_TRUE(child_layer->visible());

  host()->SetVisible(false);
  EXPECT_FALSE(child_win->IsVisible());
  EXPECT_FALSE(child_layer->visible());

  DestroyHost();
}

TEST_F(NativeViewHostAuraTest, AttachFromAnotherWindowTreeHostWithObserver) {
  if (!base::FeatureList::IsEnabled(
          views::features::kNativeViewHostManagesLayers)) {
    GTEST_SKIP();
  }

  root_window()->SetName("PrimaryRootWindow");

  std::unique_ptr<aura::WindowTreeHost> second_host =
      aura::WindowTreeHost::Create(
          ui::PlatformWindowInitProperties{gfx::Rect{100, 100}});
  second_host->InitHost();
  second_host->window()->SetName("SecondaryRootWindow");
  new wm::DefaultActivationClient(second_host->window());
  auto focus_client =
      std::make_unique<aura::test::TestFocusClient>(second_host->window());
  second_host->window()->Show();

  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams widget1_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  widget1_params.parent = second_host->window();
  widget1_params.name = "Widget1Window";
  widget1_params.bounds = gfx::Rect{0, 0, 100, 100};
  widget1->Init(std::move(widget1_params));
  widget1->Show();

  NativeViewHost* host1 =
      widget1->client_view()->AddChildView(std::make_unique<NativeViewHost>());

  auto winA = std::make_unique<aura::Window>(nullptr);
  winA->SetName("winA");
  winA->Init(ui::LAYER_TEXTURED);
  winA->SetBounds({10, 10, 50, 50});
  winA->Show();
  host1->Attach(winA.get());

  class BoundsInRootObserver : public aura::WindowObserver {
   public:
    explicit BoundsInRootObserver(aura::Window* window) : window_(window) {
      window_->AddObserver(this);
    }
    BoundsInRootObserver(const BoundsInRootObserver&) = delete;
    BoundsInRootObserver& operator=(const BoundsInRootObserver&) = delete;
    ~BoundsInRootObserver() override {
      if (window_) {
        window_->RemoveObserver(this);
      }
    }

    void OnWindowParentChanged(aura::Window* window,
                               aura::Window* parent) override {
      if (window->GetRootWindow()) {
        bounds_in_root_ = window->GetBoundsInRootWindow();
      }
    }

    void OnWindowDestroying(aura::Window* window) override {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }

    const gfx::Rect& bounds_in_root() const { return bounds_in_root_; }

   private:
    raw_ptr<aura::Window> window_;
    gfx::Rect bounds_in_root_;
  };

  BoundsInRootObserver observer(winA.get());

  CreateTopLevel();
  toplevel()->GetNativeView()->SetName("TopLevelWindow");
  toplevel()->SetBounds({0, 0, 500, 500});
  toplevel()->Show();

  NativeViewHost* host2 = toplevel()->client_view()->AddChildView(
      std::make_unique<NativeViewHost>());
  host2->SetBounds(5, 5, 50, 50);

  host2->Attach(winA.get());
}

}  // namespace views
