// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include <memory>
#include <set>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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
    WidgetTest::TearDown();
  }

 protected:
  std::unique_ptr<Widget> CreateWidgetWithZOrder(
      ui::ZOrderLevel z_order = ui::ZOrderLevel::kNormal) {
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.z_order = z_order;
    params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
    auto widget = std::make_unique<Widget>();
    widget->Init(std::move(params));
    return widget;
  }
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

  View* view =
      widget->GetContentsView()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(kBounds);

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
  View* view =
      normal_widget->GetContentsView()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(kNormalBounds);

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

TEST_F(OccludedWidgetInputProtectorTest, ShouldBlockEvent_TrackedWidget) {
  const gfx::Rect kBounds(0, 0, 100, 100);
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();
  View* view =
      aot_widget->GetContentsView()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(kBounds);

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
  View* view =
      normal_widget->GetContentsView()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(kBounds);

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB, 0);

  // Non-located events are currently not handled and should not be blocked,
  // even if the view is physically occluded by an always-on-top widget.
  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
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
  View* view = child_widget->SetContentsView(std::make_unique<View>());
  child_widget->Show();
  WidgetVisibleWaiter(child_widget.get()).Wait();

  view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

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
      std::make_unique<TestBubbleDelegate>(aot_widget->GetContentsView());
  auto bubble_widget =
      base::WrapUnique(BubbleDialogDelegate::CreateBubbleDeprecated(
          bubble_delegate.get(), Widget::InitParams::CLIENT_OWNS_WIDGET));
  bubble_widget->Show();
  WidgetVisibleWaiter(bubble_widget.get()).Wait();

  View* view = bubble_widget->GetContentsView();
  view->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

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

  View* view = normal_widget->GetContentsView();
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

  View* view = normal_widget->GetContentsView();
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

  View* view = normal_widget->GetContentsView();
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

  View* view = normal_widget->GetContentsView();

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
  View* view =
      normal_widget->GetContentsView()->AddChildView(std::make_unique<View>());
  view->SetBoundsRect(kBounds);

  ui::MouseEvent mouse_event = CreateMouseEventAtScreenPoint(
      view,
      aot_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint());

  EXPECT_FALSE(OccludedWidgetInputProtector::GetInstance()->ShouldBlockEvent(
      mouse_event, *view));
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
