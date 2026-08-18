// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/input_protection/input_protection_specification.h"
#include "ui/views/metrics.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

class TestBubbleDelegate : public BubbleDialogDelegate {
 public:
  explicit TestBubbleDelegate(View* anchor)
      : BubbleDialogDelegate(anchor, BubbleBorder::TOP_LEFT) {
    SetContentsView(std::make_unique<View>());
  }
};

class TestInputProtectedView : public View {
  METADATA_HEADER(TestInputProtectedView, View)

 public:
  explicit TestInputProtectedView(
      std::optional<gfx::Rect> local_protected_bounds = std::nullopt)
      : local_protected_bounds_(local_protected_bounds) {
    InputProtectionSpecification::Install(
        *this,
        base::BindRepeating(&TestInputProtectedView::GetLocalProtectedBounds));
  }

  ~TestInputProtectedView() override = default;

  std::vector<gfx::Rect> GetLocalProtectedBounds() const {
    return {local_protected_bounds_.value_or(GetLocalBounds())};
  }

 private:
  std::optional<gfx::Rect> local_protected_bounds_;
};

BEGIN_METADATA(TestInputProtectedView)
END_METADATA

// Used in tests to wait for a widget bounds change.
class WidgetBoundsWaiter : public WidgetObserver {
 public:
  WidgetBoundsWaiter(Widget* widget, const gfx::Rect& target_bounds)
      : target_bounds_(target_bounds) {
    observation_.Observe(widget);
    if (widget->GetWindowBoundsInScreen() == target_bounds_) {
      finished_ = true;
    }
  }
  WidgetBoundsWaiter(const WidgetBoundsWaiter&) = delete;
  WidgetBoundsWaiter& operator=(const WidgetBoundsWaiter&) = delete;
  ~WidgetBoundsWaiter() override = default;

  void Wait() {
    if (!finished_) {
      run_loop_.Run();
    }
  }

 private:
  void OnWidgetBoundsChanged(Widget* widget, const gfx::Rect& bounds) override {
    if (widget->GetWindowBoundsInScreen() == target_bounds_) {
      finished_ = true;
      if (run_loop_.running()) {
        run_loop_.Quit();
      }
    }
  }

  const gfx::Rect target_bounds_;
  bool finished_ = false;
  base::RunLoop run_loop_;
  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
};

}  // namespace

class OccludedWidgetInputProtectorTestBase : public WidgetTest {
 public:
  OccludedWidgetInputProtectorTestBase()
      : WidgetTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  const std::map<Widget*, gfx::Rect>& always_on_top_widgets() {
    return OccludedWidgetInputProtector::GetInstance()->always_on_top_widgets_;
  }

  const base::circular_deque<OccludedWidgetInputProtector::HistoricalOcclusion>&
  occlusion_history() {
    return OccludedWidgetInputProtector::GetInstance()->occlusion_history_;
  }

  const std::set<raw_ptr<Widget>>& resizing_widgets() {
    return OccludedWidgetInputProtector::GetInstance()->resizing_widgets_;
  }

  bool IsObserving(Widget* widget) {
    return widget->HasObserver(OccludedWidgetInputProtector::GetInstance());
  }

  ui::MouseEvent CreateMouseEventAtScreenPoint(View* target_view,
                                               const gfx::Point& screen_point) {
    gfx::Point local_point = screen_point;
    View::ConvertPointFromScreen(target_view, &local_point);
    return ui::MouseEvent(ui::EventType::kMousePressed, local_point,
                          local_point, ui::EventTimeForNow(), 0, 0);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  void PruneCachedOcclusionHistory() {
    OccludedWidgetInputProtector::GetInstance()->PruneCachedOcclusionHistory();
  }

  void TearDown() override {
    OccludedWidgetInputProtector::GetInstance()->ClearForTesting();
    delegates_.clear();
    WidgetTest::TearDown();
  }

 protected:
  // Keeps the delegate alive for the duration of the test. This is necessary
  // because when using CLIENT_OWNS_WIDGET, the Widget does not take ownership
  // of the delegate, and we must ensure the delegate outlives the widget to
  // avoid memory leaks and dangling pointers.
  void KeepDelegateAlive(std::unique_ptr<WidgetDelegate> delegate) {
    delegates_.push_back(std::move(delegate));
  }

  std::unique_ptr<Widget> CreateWidgetWithZOrder(
      ui::ZOrderLevel z_order = ui::ZOrderLevel::kNormal,
      bool remove_standard_frame = false,
      bool use_input_protected_view = true) {
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.z_order = z_order;
    params.remove_standard_frame = remove_standard_frame;
    params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
    auto delegate = std::make_unique<WidgetDelegate>();
    delegate->SetContentsView(use_input_protected_view
                                  ? std::make_unique<TestInputProtectedView>()
                                  : std::make_unique<View>());
    params.delegate = delegate.get();
    KeepDelegateAlive(std::move(delegate));
    auto widget = std::make_unique<Widget>();
    widget->Init(std::move(params));
    return widget;
  }

 private:
  std::vector<std::unique_ptr<WidgetDelegate>> delegates_;
};

class OccludedWidgetInputProtectorTest
    : public OccludedWidgetInputProtectorTestBase {
 public:
  OccludedWidgetInputProtectorTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OccludedWidgetInputProtectorTest, TracksAlwaysOnTopWidget) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(IsObserving(widget.get()));

  // Not tracked yet because it is not visible.
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, DoesNotTrackNormalWidget) {
  auto widget = CreateWidgetWithZOrder();
  EXPECT_FALSE(IsObserving(widget.get()));

  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, CleanupOnDestroy) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget.reset();
  EXPECT_TRUE(always_on_top_widgets().empty());
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesZOrderLevelChanges) {
  // Start with a normal widget.
  auto widget = CreateWidgetWithZOrder();
  EXPECT_FALSE(IsObserving(widget.get()));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Change Z-order to always-on-top.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);

  // It should now be observed and tracked (since it is visible).
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));

  // Change back to normal.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesZOrderLevelChangesWhileHidden) {
  auto widget = CreateWidgetWithZOrder();
  EXPECT_FALSE(IsObserving(widget.get()));

  // Change to AOT while hidden.
  widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  // Should be observed now, but not in the visible set.
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Showing should add it to the set.
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));

  // Hiding it.
  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));

  // Changing to normal while hidden.
  widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_FALSE(IsObserving(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, TracksHigherZOrderLevels) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kSecuritySurface);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  EXPECT_TRUE(IsObserving(widget.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, TracksMultipleWidgets) {
  auto widget1 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  auto widget2 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);

  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));

  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));

  widget1->Show();
  WidgetVisibleWaiter(widget1.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));

  widget2->Show();
  WidgetVisibleWaiter(widget2.get()).Wait();
  EXPECT_TRUE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget2.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));

  widget1->Hide();
  WidgetVisibleWaiter(widget1.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_TRUE(IsObserving(widget1.get()));
  EXPECT_TRUE(always_on_top_widgets().contains(widget2.get()));

  widget2->Hide();
  WidgetVisibleWaiter(widget2.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget1.get()));
  EXPECT_FALSE(always_on_top_widgets().contains(widget2.get()));
  EXPECT_TRUE(IsObserving(widget2.get()));
}

TEST_F(OccludedWidgetInputProtectorTest, HandlesDestroyWhileHidden) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  EXPECT_FALSE(always_on_top_widgets().contains(widget.get()));
  EXPECT_TRUE(IsObserving(widget.get()));

  widget.reset();
  EXPECT_TRUE(always_on_top_widgets().empty());
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_NoAOTWidgets) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto widget = CreateWidgetWithZOrder();
  widget->SetBounds(kBounds);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* view = widget->GetClientContentsView();

  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view, widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB, 0);

  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_LocatedEvent) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  const gfx::Rect kNormalBounds(0, 0, 200, 200);
  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  // Point inside AOT widget.
  ui::MouseEvent inside_event = CreateMouseEventAtScreenPoint(
      view,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      inside_event, *view));

  // Point outside AOT widget.
  gfx::Point screen_point_outside =
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().bottom_right();
  screen_point_outside.Offset(10, 10);
  ui::MouseEvent outside_event =
      CreateMouseEventAtScreenPoint(view, screen_point_outside);
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      outside_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_LocatedEvent_ProtectedByParentSpecification) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  const gfx::Rect kNormalBounds(0, 0, 200, 200);
  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  View* parent = normal_widget->GetClientContentsView();
  // Install specification on parent.
  InputProtectionSpecification::Install(
      *parent, base::BindRepeating([](const View* v) {
        return std::vector<gfx::Rect>{v->GetLocalBounds()};
      }));

  View* child = parent->AddChildView(std::make_unique<View>());
  child->SetBoundsRect(gfx::Rect(0, 0, 200, 200));

  // Point inside AOT widget.
  ui::MouseEvent inside_event = CreateMouseEventAtScreenPoint(
      child,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());

  // Querying child should block because parent defines protected bounds (which
  // cover child).
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      inside_event, *child));

  // Point outside AOT widget.
  gfx::Point screen_point_outside =
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().bottom_right();
  screen_point_outside.Offset(10, 10);
  ui::MouseEvent outside_event =
      CreateMouseEventAtScreenPoint(child, screen_point_outside);
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      outside_event, *child));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NonLocatedEvent_ProtectedByParentSpecification) {
  const gfx::Rect kAotBounds(0, 0, 100, 100);
  const gfx::Rect kNormalWidgetBounds(0, 0, 200, 200);

  // AOT widget occludes the top-left part of the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kAotBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create normal widget.
  auto normal_widget =
      CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal, false, false);
  normal_widget->SetBounds(kNormalWidgetBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  // Setup parent and child views.
  View* parent_view = normal_widget->GetClientContentsView()->AddChildView(
      std::make_unique<View>());
  parent_view->SetBounds(0, 0, 150, 150);
  View* child_view = parent_view->AddChildView(std::make_unique<View>());
  child_view->SetBounds(10, 10, 50, 50);

  // Set the specification on the parent_view.
  // It protects the parent_view's local bounds (0, 0, 150, 150).
  InputProtectionSpecification::Install(
      *parent_view, base::BindRepeating([](const View* v) {
        return std::vector<gfx::Rect>{v->GetLocalBounds()};
      }));

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Querying on child_view should block because it inherits the specification
  // from `parent_view`, and the protected area (`parent_view` bounds, which
  // overlap with AOT) is partially occluded.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *child_view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_LocatedEvent_AdditiveParentAndChildSpecification) {
  const gfx::Rect kAotBounds(100, 100, 50, 50);
  const gfx::Rect kNormalWidgetBounds(0, 0, 200, 200);

  // Always-on-top widget occludes the bottom-right part of the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kAotBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create normal widget.
  auto normal_widget =
      CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal, false, false);
  normal_widget->SetBounds(kNormalWidgetBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  // Setup parent and child views.
  View* parent_view = normal_widget->GetClientContentsView()->AddChildView(
      std::make_unique<View>());
  // The parent view covers the entire widget.
  parent_view->SetBounds(0, 0, 200, 200);
  View* child_view = parent_view->AddChildView(std::make_unique<View>());
  // The child view covers most of the parent view.
  child_view->SetBounds(0, 0, 150, 150);

  // Set specification on parent view to protect parent bounds.
  InputProtectionSpecification::Install(
      *parent_view, base::BindRepeating([](const View* v) {
        return std::vector<gfx::Rect>{v->GetLocalBounds()};
      }));
  auto* parent_spec = parent_view->GetProperty(kInputProtectionKey);

  // Set specification on child view to protect only the top-left part.
  InputProtectionSpecification::Install(
      *child_view, base::BindRepeating([](const View*) {
        return std::vector<gfx::Rect>{gfx::Rect(0, 0, 80, 80)};
      }));
  auto* child_spec = child_view->GetProperty(kInputProtectionKey);

  // Verify the geometry in screen coordinates to make assumptions explicit.
  gfx::Rect child_view_bounds_in_screen = child_view->GetBoundsInScreen();
  gfx::Rect aot_client_bounds_in_screen =
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen();

  // Click point is chosen to be the center of the always-on-top client area.
  gfx::Point click_point = aot_client_bounds_in_screen.CenterPoint();

  // Verify that the click is inside the always-on-top widget client area.
  EXPECT_TRUE(aot_client_bounds_in_screen.Contains(click_point));

  // Verify that the click is inside the child view physical bounds.
  EXPECT_TRUE(child_view_bounds_in_screen.Contains(click_point));

  // Verify that the click is outside the child view custom protected bounds.
  std::vector<gfx::Rect> child_protected_bounds =
      child_spec->GetProtectedBoundsInScreen(*child_view);
  ASSERT_EQ(child_protected_bounds.size(), 1u);
  EXPECT_FALSE(child_protected_bounds[0].Contains(click_point));

  // Verify that the click is inside the parent view custom protected bounds.
  std::vector<gfx::Rect> parent_protected_bounds =
      parent_spec->GetProtectedBoundsInScreen(*parent_view);
  ASSERT_EQ(parent_protected_bounds.size(), 1u);
  EXPECT_TRUE(parent_protected_bounds[0].Contains(click_point));

  ui::MouseEvent click_event =
      CreateMouseEventAtScreenPoint(child_view, click_point);

  // Should block because the parent specification still protects the click
  // point even though we targeted the child view (which has its own
  // specification that does not cover the click point).
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      click_event, *child_view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_ViewWithoutProtectedBounds) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  const gfx::Rect kNormalBounds(0, 0, 200, 200);
  auto normal_widget =
      CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal, false,
                             /*use_input_protected_view=*/false);
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  // Located events do not require widget opt-in. The click is blocked because
  // it lands inside the default view bounds and is occluded by the AOT widget.
  ui::MouseEvent inside_event = CreateMouseEventAtScreenPoint(
      view,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      inside_event, *view));
}

TEST_F(
    OccludedWidgetInputProtectorTest,
    ShouldNotBlockNonLocatedEvent_ViewWithoutProtectedBounds_PartiallyOccluded) {
  // AOT widget partially occludes the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create normal widget that is partially occluded (overlapping the AOT widget
  // at 50,50, 50x50).
  const gfx::Rect kNormalBounds(50, 50, 150, 150);
  auto normal_widget =
      CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal, false,
                             /*use_input_protected_view=*/false);
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Not blocked because it is only partially occluded, and views that do not
  // override `GetLocalInputProtectedBounds()` are only blocked when fully
  // occluded.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockNonLocatedEvent_ViewWithoutProtectedBounds_FullyOccluded) {
  // AOT widget fully occludes the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 200, 200));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create normal widget that is fully occluded (fully inside the AOT widget
  // bounds).
  const gfx::Rect kNormalBounds(0, 0, 100, 100);
  auto normal_widget =
      CreateWidgetWithZOrder(ui::ZOrderLevel::kNormal, false,
                             /*use_input_protected_view=*/false);
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because it is fully occluded (even though it has no protected
  // bounds).
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_TrackedWidget) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();
  View* view = aot_widget->GetClientContentsView();

  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB, 0);

  // The protector should recognize the view as being associated with its own
  // tracked always-on-top widget, and not block its events.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_NonLocatedEvent) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB, 0);

  // Non-located events are currently not handled and should not be blocked,
  // even if the view is physically occluded by an always-on-top widget.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NonLocatedEvent_OptIn) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because occluded and opted-in.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));

  // Test that it is not blocked when not occluded.
  auto unoccluded_widget = CreateWidgetWithZOrder();
  unoccluded_widget->SetBounds(gfx::Rect(200, 200, 100, 100));
  unoccluded_widget->EnableInputEventActivationProtection();
  unoccluded_widget->Show();
  WidgetVisibleWaiter(unoccluded_widget.get()).Wait();
  View* unoccluded_view = unoccluded_widget->GetClientContentsView();

  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *unoccluded_view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NestedWidget_ParentEnabled) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create parent widget.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(kBounds);
  parent_widget->EnableInputEventActivationProtection();
  parent_widget->Show();
  WidgetVisibleWaiter(parent_widget.get()).Wait();

  // Create child widget parented to parent_widget.
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent_widget->GetNativeView();
  child_params.bounds = kBounds;
  child_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  auto child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(child_params));
  View* view =
      child_widget->SetContentsView(std::make_unique<TestInputProtectedView>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because parent widget has protection enabled.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *parent_widget->GetClientContentsView()));

  // Blocked because child widget inherits protection from parent_widget.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NestedWidget_NeitherEnabled) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Test that it is not blocked if parent does not enable protection.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(kBounds);
  parent_widget->Show();
  WidgetVisibleWaiter(parent_widget.get()).Wait();

  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent_widget->GetNativeView();
  child_params.bounds = kBounds;
  child_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  auto child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(child_params));
  View* view =
      child_widget->SetContentsView(std::make_unique<TestInputProtectedView>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Not blocked because parent widget does not have protection enabled.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *parent_widget->GetClientContentsView()));

  // Not blocked because child widget does not have protection enabled.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NestedWidget_ChildEnabled) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 800, 800));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create parent widget (unprotected).
  const gfx::Rect kParentWidgetBounds(0, 0, 100, 100);
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(kParentWidgetBounds);
  parent_widget->Show();
  WidgetVisibleWaiter(parent_widget.get()).Wait();

  // Create child widget parented to parent_widget (protected).
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent_widget->GetNativeView();
  child_params.bounds = kParentWidgetBounds;
  child_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  auto child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(child_params));
  child_widget->EnableInputEventActivationProtection();
  View* view =
      child_widget->SetContentsView(std::make_unique<TestInputProtectedView>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because child widget itself has protection enabled.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));

  // Not blocked on the parent widget because it does not have protection
  // enabled.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *parent_widget->GetClientContentsView()));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockEvent_NestedWidget_BothEnabled) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 800, 800));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create parent widget (protected).
  const gfx::Rect kParentWidgetBounds(0, 0, 100, 100);
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(kParentWidgetBounds);
  parent_widget->EnableInputEventActivationProtection();
  parent_widget->Show();
  WidgetVisibleWaiter(parent_widget.get()).Wait();

  // Create child widget parented to parent_widget (protected).
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = parent_widget->GetNativeView();
  child_params.bounds = kParentWidgetBounds;
  child_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  auto child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(child_params));
  child_widget->EnableInputEventActivationProtection();
  View* view =
      child_widget->SetContentsView(std::make_unique<TestInputProtectedView>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because parent widget has protection enabled.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *parent_widget->GetClientContentsView()));

  // Blocked because child widget has protection enabled.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_ParentedWidget) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create a child widget with a standard parent-child relationship (native
  // parenting).
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = aot_widget->GetNativeView();
  child_params.bounds = gfx::Rect(50, 50, 200, 200);
  child_params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  auto child_widget = std::make_unique<Widget>();
  child_widget->Init(std::move(child_params));
  View* view =
      child_widget->SetContentsView(std::make_unique<TestInputProtectedView>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();


  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view,
      child_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());

  // The protector should recognize the view as being associated with a tracked
  // always-on-top widget, via the parent-child relationship, and not block its
  // events.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_AnchoredWidget) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create a bubble anchored to the AOT widget. This establishes a logical
  // anchoring relationship which is resolved via `GetPrimaryWindowWidget`.
  auto bubble_delegate =
      std::make_unique<TestBubbleDelegate>(aot_widget->GetClientContentsView());
  auto bubble_widget =
      base::WrapUnique(BubbleDialogDelegate::CreateBubbleDeprecated(
          bubble_delegate.get(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  bubble_widget->Show();
  WidgetVisibleWaiter(bubble_widget.get()).Wait();

  View* view = bubble_widget->GetClientContentsView();

  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view,
      bubble_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());

  // The protector should recognize the view as being associated with a tracked
  // always-on-top widget, via the anchoring relationship, and not block its
  // events.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, HistoricalOcclusion_Hide) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  const gfx::Rect aot_bounds(10, 10, 100, 100);
  aot_widget->SetBounds(aot_bounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(aot_bounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  View* view = normal_widget->GetClientContentsView();
  ui::MouseEvent event = CreateMouseEventAtScreenPoint(
      view,
      normal_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));

  aot_widget->Hide();
  WidgetVisibleWaiter(aot_widget.get()).WaitUntilInvisible();

  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, HistoricalOcclusion_Close) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  const gfx::Rect aot_bounds(10, 10, 100, 100);
  aot_widget->SetBounds(aot_bounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(aot_bounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  View* view = normal_widget->GetClientContentsView();
  ui::MouseEvent event = CreateMouseEventAtScreenPoint(
      view,
      normal_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
  aot_widget.reset();
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, HistoricalOcclusion_Unregister) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  const gfx::Rect aot_bounds(10, 10, 100, 100);
  aot_widget->SetBounds(aot_bounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(aot_bounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  View* view = normal_widget->GetClientContentsView();
  ui::MouseEvent event = CreateMouseEventAtScreenPoint(
      view,
      normal_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
  aot_widget->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest, HistoricalOcclusion_Move) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  const gfx::Rect new_bounds(200, 200, 100, 100);
  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(new_bounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  View* view = normal_widget->GetClientContentsView();

  WidgetBoundsWaiter waiter(aot_widget.get(), new_bounds);
  aot_widget->SetBounds(new_bounds);
  waiter.Wait();

  ui::MouseEvent event_at_new = CreateMouseEventAtScreenPoint(
      view,
      normal_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event_at_new, *view));

  aot_widget->Hide();
  WidgetVisibleWaiter(aot_widget.get()).WaitUntilInvisible();

  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event_at_new, *view));
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      event_at_new, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       UserResize_DoesNotRecordHistoricalOcclusion) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Get the initial size.
  const size_t initial_size = occlusion_history().size();

  // Simulate user resize starting.
  aot_widget->OnNativeWidgetUserResizeStarted();
  EXPECT_TRUE(resizing_widgets().contains(aot_widget.get()));

  // Change bounds (resize).
  const gfx::Rect target_bounds(100, 100, 200, 200);
  WidgetBoundsWaiter waiter(aot_widget.get(), target_bounds);
  aot_widget->SetBounds(target_bounds);
  waiter.Wait();

  // Verify no new record was added during user manipulation.
  EXPECT_EQ(occlusion_history().size(), initial_size);

  // End resize.
  aot_widget->OnNativeWidgetUserResizeEnded();
  EXPECT_FALSE(resizing_widgets().contains(aot_widget.get()));
}

TEST_F(OccludedWidgetInputProtectorTest,
       UserDrag_DoesNotRecordHistoricalOcclusion) {
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Baseline size.
  const size_t initial_size = occlusion_history().size();

  // Simulate user drag starting.
  aot_widget->OnNativeWidgetUserDragStarted();
  EXPECT_TRUE(aot_widget->is_dragging());

  // Change bounds (move).
  const gfx::Rect target_bounds(100, 100, 100, 100);
  WidgetBoundsWaiter waiter(aot_widget.get(), target_bounds);
  aot_widget->SetBounds(target_bounds);
  waiter.Wait();

  // Verify no new record was added.
  EXPECT_EQ(occlusion_history().size(), initial_size);

  // End drag.
  aot_widget->OnNativeWidgetUserDragEnded();
  EXPECT_FALSE(aot_widget->is_dragging());
}

TEST_F(OccludedWidgetInputProtectorTest, Pruning_PreservesFIFOOrder) {
  auto widget1 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  auto widget2 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  const gfx::Rect bounds1(0, 0, 100, 100);
  const gfx::Rect bounds2(200, 200, 100, 100);
  widget1->SetBounds(bounds1);
  widget2->SetBounds(bounds2);
  widget1->Show();
  WidgetVisibleWaiter(widget1.get()).Wait();
  widget2->Show();
  WidgetVisibleWaiter(widget2.get()).Wait();

  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  PruneCachedOcclusionHistory();
  ASSERT_TRUE(occlusion_history().empty());

  widget1->Hide();
  WidgetVisibleWaiter(widget1.get()).WaitUntilInvisible();

  ASSERT_FALSE(occlusion_history().empty());
  const gfx::Rect recorded_bounds1 = occlusion_history().back().bounds;
  FastForwardBy(GetDoubleClickInterval() / 2);

  widget2->Hide();
  WidgetVisibleWaiter(widget2.get()).WaitUntilInvisible();

  ASSERT_FALSE(occlusion_history().empty());
  const gfx::Rect recorded_bounds2 = occlusion_history().back().bounds;

  const auto& history = occlusion_history();
  EXPECT_EQ(history[0].bounds, recorded_bounds1);
  EXPECT_EQ(history[1].bounds, recorded_bounds2);

  // Jump so that the first record is exactly 1ms past its expiration, while
  // the second record is only roughly halfway through its life.
  FastForwardBy(GetDoubleClickInterval() / 2 + base::Milliseconds(1));
  PruneCachedOcclusionHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].bounds, recorded_bounds2);
}

TEST_F(OccludedWidgetInputProtectorTest, Pruning_HandlesSimultaneousRecords) {
  auto widget1 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  auto widget2 = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget1->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget2->SetBounds(gfx::Rect(200, 200, 100, 100));
  widget1->Show();
  WidgetVisibleWaiter(widget1.get()).Wait();
  widget2->Show();
  WidgetVisibleWaiter(widget2.get()).Wait();

  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  PruneCachedOcclusionHistory();
  widget1->Hide();
  WidgetVisibleWaiter(widget1.get()).WaitUntilInvisible();
  widget2->Hide();
  WidgetVisibleWaiter(widget2.get()).WaitUntilInvisible();

  EXPECT_EQ(occlusion_history().size(), 2u);
  FastForwardBy(GetDoubleClickInterval());
  PruneCachedOcclusionHistory();
  EXPECT_TRUE(occlusion_history().empty());
}

TEST_F(OccludedWidgetInputProtectorTest, Pruning_ExactBoundaryCondition) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Clear any historical records from OS-level repositioning during Show().
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  PruneCachedOcclusionHistory();
  ASSERT_TRUE(occlusion_history().empty());

  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();

  EXPECT_EQ(occlusion_history().size(), 1u);
  FastForwardBy(GetDoubleClickInterval());
  PruneCachedOcclusionHistory();
  EXPECT_TRUE(occlusion_history().empty());
}

TEST_F(OccludedWidgetInputProtectorTest,
       HistoricalOcclusion_RedundantBoundsChange) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  widget->SetBounds(gfx::Rect(10, 10, 100, 100));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Clear any historical records from OS-level repositioning during Show().
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));
  PruneCachedOcclusionHistory();
  ASSERT_TRUE(occlusion_history().empty());

  // Simulate a redundant bounds change (same bounds).
  OccludedWidgetInputProtector::GetInstance()->OnWidgetBoundsChanged(
      widget.get(), widget->GetNonDecoratedClientAreaBoundsInScreen());

  // Size should remain zero.
  EXPECT_TRUE(occlusion_history().empty());

  // Now hide.
  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();

  // Should only have one record from the hide operation.
  EXPECT_EQ(occlusion_history().size(), 1u);
}

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_FeatureDisabled) {
  // Disable feature.
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(features::kEnableInputProtection);

  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());

  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldBlockNonLocatedEvent_ViewWithProtectedBounds_PartiallyOccluded) {
  // AOT widget partially occludes the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create normal widget that is partially occluded (overlapping the AOT widget
  // at 50,50, 50x50).
  const gfx::Rect kNormalBounds(50, 50, 150, 150);
  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();
  View* view = normal_widget->GetClientContentsView();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Blocked because views that override `GetLocalInputProtectedBounds()` are
  // blocked even on partial occlusion.
  EXPECT_TRUE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       ShouldNotBlockLocatedEvent_OutsideProtectedArea) {
  const gfx::Rect kAotBounds(0, 0, 100, 100);
  const gfx::Rect kNormalWidgetBounds(0, 0, 200, 200);
  const gfx::Rect kProtectedBounds(0, 0, 50, 50);
  // A point inside the AOT widget but outside the protected area (which is at
  // the top-left).
  const gfx::Point kClickPoint(kAotBounds.right() - 20,
                               kAotBounds.bottom() - 20);

  // AOT widget occludes only the top-left part of the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kAotBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // The view defines a small protected area at local kProtectedBounds.
  auto delegate = std::make_unique<WidgetDelegate>();
  auto* view = delegate->SetContentsView(
      std::make_unique<TestInputProtectedView>(kProtectedBounds));

  // Create normal widget that is partially occluded by the AOT widget.
  //
  // The `delegate` must be declared before `normal_widget` so that the widget
  // is destroyed first.
  auto normal_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.delegate = delegate.get();
  normal_widget->Init(std::move(params));
  normal_widget->SetBounds(kNormalWidgetBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  // Click is outside the protected area but inside the AOT widget bounds.
  ui::MouseEvent click_outside_protected =
      CreateMouseEventAtScreenPoint(view, kClickPoint);

  // Should not be blocked because click is outside the protected area.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      click_outside_protected, *view));
}

TEST_F(
    OccludedWidgetInputProtectorTest,
    ShouldNotBlockNonLocatedEvent_ViewWithPartialProtectedBounds_ProtectedAreaUnoccluded) {
  const gfx::Rect kAotBounds(150, 150, 100, 100);
  const gfx::Rect kNormalWidgetBounds(0, 0, 200, 200);
  const gfx::Rect kProtectedBounds(0, 0, 50, 50);

  // AOT widget occludes only the bottom-right part of the normal widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kAotBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // The view defines a protected area at top-left kProtectedBounds.
  // Since AOT is at kAotBounds (bottom-right), the protected area is fully
  // unoccluded.
  auto delegate = std::make_unique<WidgetDelegate>();
  auto* view = delegate->SetContentsView(
      std::make_unique<TestInputProtectedView>(kProtectedBounds));

  // Create normal widget that is partially occluded by the AOT widget.
  //
  // The `delegate` must be declared before `normal_widget` so that the widget
  // is destroyed first.
  auto normal_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.delegate = delegate.get();
  normal_widget->Init(std::move(params));
  normal_widget->SetBounds(kNormalWidgetBounds);
  normal_widget->EnableInputEventActivationProtection();
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);

  // Should not be blocked because the protected area is fully unoccluded
  // (even though the widget is partially occluded in the bottom-right).
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      key_event, *view));
}

TEST_F(OccludedWidgetInputProtectorTest,
       GetInputProtectedBoundsInScreen_ClipsToLocalBounds) {
  const gfx::Rect kNormalBounds(0, 0, 200, 200);
  auto normal_widget = CreateWidgetWithZOrder();
  normal_widget->SetBounds(kNormalBounds);
  normal_widget->Show();
  WidgetVisibleWaiter(normal_widget.get()).Wait();

  // Retrieve the container view.
  View* container = normal_widget->GetClientContentsView();

  // Test a view returning bounds that stretch outside its boundaries. The
  // returned bounds should be clipped to the view's local bounds (0, 0, 100,
  // 100).
  gfx::Rect bad_bounds_1(-50, -50, 200, 200);
  auto* view1 = container->AddChildView<View>(
      std::make_unique<TestInputProtectedView>(bad_bounds_1));
  view1->SetBounds(10, 10, 100, 100);

  // The expected result is the view's actual screen bounds.
  gfx::Rect expected_screen_bounds = view1->GetBoundsInScreen();

  InputProtectionSpecification* spec = view1->GetProperty(kInputProtectionKey);
  ASSERT_TRUE(spec);
  std::vector<gfx::Rect> screen_bounds =
      spec->GetProtectedBoundsInScreen(*view1);
  ASSERT_EQ(screen_bounds.size(), 1u);
  EXPECT_EQ(screen_bounds[0], expected_screen_bounds);

  // Test a view returning bounds that are completely outside its boundaries.
  // The returned bounds should be completely ignored (empty bounds).
  gfx::Rect bad_bounds_2(150, 150, 50, 50);
  auto* view2 = container->AddChildView<View>(
      std::make_unique<TestInputProtectedView>(bad_bounds_2));
  view2->SetBounds(10, 10, 100, 100);

  InputProtectionSpecification* spec2 = view2->GetProperty(kInputProtectionKey);
  ASSERT_TRUE(spec2);
  std::vector<gfx::Rect> screen_bounds2 =
      spec2->GetProtectedBoundsInScreen(*view2);
  EXPECT_TRUE(screen_bounds2.empty());
}

class OccludedWidgetInputProtectorDisabledTest
    : public OccludedWidgetInputProtectorTestBase {
 public:
  OccludedWidgetInputProtectorDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kEnableInputProtection);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OccludedWidgetInputProtectorDisabledTest, DoesNotTrackWhenDisabled) {
  auto widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_FALSE(IsObserving(widget.get()));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  EXPECT_TRUE(always_on_top_widgets().empty());
}

}  // namespace views::test
