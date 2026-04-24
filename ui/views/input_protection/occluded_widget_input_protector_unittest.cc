// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occluded_widget_input_protector.h"

#include <memory>
#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
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

}  // namespace

class OccludedWidgetInputProtectorTestBase : public WidgetTest {
 public:
  OccludedWidgetInputProtectorTestBase() = default;

  const std::set<Widget*>& always_on_top_widgets() {
    return OccludedWidgetInputProtector::GetInstance()
        ->always_on_top_widgets_for_testing();
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
  auto bubble_widget = base::WrapUnique(BubbleDialogDelegate::CreateBubble(
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
