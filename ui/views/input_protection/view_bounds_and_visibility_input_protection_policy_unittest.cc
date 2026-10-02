// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/view_bounds_and_visibility_input_protection_policy.h"

#include <memory>
#include <utility>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class ViewBoundsAndVisibilityInputProtectionPolicyTest : public WidgetTest {
 public:
  ViewBoundsAndVisibilityInputProtectionPolicyTest()
      : WidgetTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

 protected:
  ui::MouseEvent CreateClickEvent(base::TimeTicks timestamp) {
    return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                          gfx::Point(), timestamp, 0, 0);
  }

  void FastForwardPastCooldown(const InputEventActivationProtector& protector) {
    task_environment()->FastForwardBy(protector.cooldown_interval() +
                                      base::Milliseconds(1));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       InitialBoundsChangeFromEmptyIgnored) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  // Construct `ViewBoundsAndVisibilityInputProtectionPolicy` while `bounds()`
  // are empty.
  EXPECT_TRUE(sensitive_view->bounds().IsEmpty());
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  FastForwardPastCooldown(mock_protector);

  // Setting non-empty bounds for the first time does not trigger a cooldown.
  sensitive_view->SetBounds(10, 10, 100, 30);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       IdenticalBoundsChangeIgnored) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  constexpr gfx::Rect kBounds(10, 10, 100, 30);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBoundsRect(kBounds);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Setting identical bounds is a no-op and does not trigger a cooldown.
  sensitive_view->SetBoundsRect(kBounds);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       PositionChangeTriggersCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBounds(10, 10, 100, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Move `sensitive_view`.
  sensitive_view->SetX(sensitive_view->x() + 1);

  // Interaction with `sensitive_view` is blocked.
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Fast forward past the cooldown period.
  FastForwardPastCooldown(mock_protector);

  // Interaction is allowed after the cooldown period.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       SizeChangeTriggersCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBounds(10, 10, 100, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Resize `sensitive_view`.
  sensitive_view->SetSize(
      gfx::Size(sensitive_view->width(), sensitive_view->height() + 20));

  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  FastForwardPastCooldown(mock_protector);

  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       UntrackedSiblingAllowedDuringCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());
  View* child_view = sensitive_view->AddChildView(std::make_unique<View>());
  View* sibling_view = root->AddChildView(std::make_unique<View>());

  constexpr gfx::Rect kViewBounds(10, 10, 100, 30);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBoundsRect(kViewBounds);
  sibling_view->SetBounds(kViewBounds.x(), kViewBounds.bottom() + 10,
                          kViewBounds.width(), kViewBounds.height());

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;

  // The observed view and its descendants are blocked, but the untracked
  // sibling is allowed.
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), child_view, mock_protector));
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sibling_view, mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest, InitialLayoutIgnored) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  root->SetLayoutManager(std::make_unique<FillLayout>());
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  // Construct `ViewBoundsAndVisibilityInputProtectionPolicy` while `bounds()`
  // are empty.
  EXPECT_TRUE(sensitive_view->bounds().IsEmpty());
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;

  constexpr gfx::Rect kWidgetBounds(0, 0, 400, 400);
  widget->SetBounds(kWidgetBounds);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  FastForwardPastCooldown(mock_protector);

  // Lay out the view hierarchy from empty bounds.
  views::test::RunScheduledLayout(widget.get());
  EXPECT_EQ(sensitive_view->bounds(), kWidgetBounds);

  // Initial layout from empty bounds does not trigger a cooldown.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       LayoutChangeTriggersCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  root->SetLayoutManager(std::make_unique<FillLayout>());
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  constexpr gfx::Rect kWidgetBounds(0, 0, 400, 400);
  widget->SetBounds(kWidgetBounds);
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  views::test::RunScheduledLayout(widget.get());

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Initially not in cooldown.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Resizing the `Widget` triggers a new layout pass that resizes the view.
  constexpr gfx::Rect kNewWidgetBounds(kWidgetBounds.x(), kWidgetBounds.y(),
                                       kWidgetBounds.width() + 100,
                                       kWidgetBounds.height() + 100);
  widget->SetBounds(kNewWidgetBounds);
  views::test::RunScheduledLayout(widget.get());
  EXPECT_EQ(sensitive_view->bounds(), kNewWidgetBounds);

  // Interaction is blocked due to the layout change.
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  FastForwardPastCooldown(mock_protector);

  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       ParentShiftProtectsChildren) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* container_a = root->AddChildView(std::make_unique<View>());
  View* child_button = container_a->AddChildView(std::make_unique<View>());

  View* container_b = root->AddChildView(std::make_unique<View>());
  View* sibling_button = container_b->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  constexpr gfx::Rect kContainerBounds(10, 10, 200, 50);
  constexpr gfx::Rect kButtonBounds(0, 0, 80, 30);
  container_a->SetBoundsRect(kContainerBounds);
  child_button->SetBoundsRect(kButtonBounds);
  container_b->SetBounds(kContainerBounds.x(), kContainerBounds.bottom() + 10,
                         kContainerBounds.width(), kContainerBounds.height());
  sibling_button->SetBoundsRect(kButtonBounds);

  // Protect parent `container_a`.
  widget->EnableInputEventActivationProtection(
      std::make_unique<InputEventActivationProtector>(
          std::make_unique<ViewBoundsAndVisibilityInputProtectionPolicy>(
              *container_a)));
  auto* protector = widget->GetInputEventActivationProtector();
  ASSERT_NE(protector, nullptr);

  FastForwardPastCooldown(*protector);

  // Shift `container_a` down.
  container_a->SetY(container_a->y() + 20);

  // Clicks targeting `child_button` are blocked because ancestor `container_a`
  // shifted.
  EXPECT_TRUE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      child_button));

  FastForwardPastCooldown(*protector);

  EXPECT_FALSE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      child_button));

  // Shift `container_a` down again.
  container_a->SetY(container_a->y() + 20);

  // Clicks targeting `sibling_button` in `container_b` are allowed because
  // `container_b` did not shift.
  EXPECT_FALSE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      sibling_button));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       AddedToVisibleWidgetTriggersCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());
  sensitive_view->SetBounds(10, 10, 100, 30);

  // Protect the newly added view on the visible widget.
  widget->EnableInputEventActivationProtection(
      std::make_unique<InputEventActivationProtector>(
          std::make_unique<ViewBoundsAndVisibilityInputProtectionPolicy>(
              *sensitive_view)));
  auto* protector = widget->GetInputEventActivationProtector();
  ASSERT_NE(protector, nullptr);

  // Adding to a visible widget blocks interaction immediately.
  EXPECT_TRUE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      sensitive_view));

  FastForwardPastCooldown(*protector);

  EXPECT_FALSE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      sensitive_view));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       ConstructingBeforeAddedToWidgetCheckFails) {
  auto sensitive_view = std::make_unique<View>();
  sensitive_view->SetBounds(10, 10, 100, 30);

  // Constructing `ViewBoundsAndVisibilityInputProtectionPolicy` before
  // `sensitive_view` is attached to a `Widget` must CHECK-fail.
  EXPECT_CHECK_DEATH(
      (ViewBoundsAndVisibilityInputProtectionPolicy(*sensitive_view)));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       ViewAddedToInvisibleWidgetDoesNotTriggerCooldownUntilShown) {
  // Create widget but do not show it.
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));

  auto sensitive_view = std::make_unique<View>();
  sensitive_view->SetBounds(10, 10, 100, 30);

  // Add the view to an invisible widget and construct the policy.
  View* added_view =
      widget->GetRootView()->AddChildView(std::move(sensitive_view));
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*added_view);
  MockInputEventActivationProtector mock_protector;

  // Adding to an invisible widget does not trigger cooldown.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), added_view, mock_protector));

  // Showing the widget makes the view drawn in a visible widget and triggers
  // the cooldown.
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), added_view, mock_protector));

  FastForwardPastCooldown(mock_protector);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), added_view, mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       OnProtectionResetRestartsCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBounds(10, 10, 100, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Initially not in cooldown.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Calling `OnProtectionReset()` restarts the cooldown.
  policy.OnProtectionReset();
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  FastForwardPastCooldown(mock_protector);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Stopping protection clears the cooldown and prevents reset from restarting
  // it.
  policy.OnProtectionStopped();
  policy.OnProtectionReset();
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // `OnProtectionStarted` starts protection unconditionally.
  policy.OnProtectionStarted();
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       BecomingVisibleTriggersCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();
  sensitive_view->SetBounds(10, 10, 100, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  sensitive_view->SetVisible(false);
  sensitive_view->SetVisible(true);

  // Clicks are blocked immediately after becoming visible.
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  FastForwardPastCooldown(mock_protector);

  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       DeletingObservedViewDoesNotCrash) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());

  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->EnableInputEventActivationProtection(
      std::make_unique<InputEventActivationProtector>(
          std::make_unique<ViewBoundsAndVisibilityInputProtectionPolicy>(
              *sensitive_view)));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  InputEventActivationProtector* protector =
      widget->GetInputEventActivationProtector();
  ASSERT_NE(protector, nullptr);

  // Deleting `sensitive_view` cleans up the observation safely and stops
  // protection.
  root->RemoveChildViewT(sensitive_view);

  EXPECT_FALSE(protector->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), /*allow_key_events=*/false,
      root));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       ViewRemovedFromWidgetStopsProtection) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* root = widget->GetRootView();
  View* sensitive_view = root->AddChildView(std::make_unique<View>());
  sensitive_view->SetBounds(10, 10, 100, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Remove the observed view from the widget.
  std::unique_ptr<View> detached_view = root->RemoveChildViewT(sensitive_view);

  // Once detached, protection is stopped.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), detached_view.get(),
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       HiddenViewDoesNotTriggerCooldownUntilDrawn) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* root = widget->GetRootView();
  View* container = root->AddChildView(std::make_unique<View>());
  container->SetBounds(0, 0, 200, 100);
  container->SetVisible(false);

  View* sensitive_view = container->AddChildView(std::make_unique<View>());
  sensitive_view->SetBounds(10, 10, 100, 30);
  sensitive_view->SetVisible(false);

  // Constructing the policy on a hidden view in a visible widget does not start
  // a cooldown.
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Changing bounds while hidden does not start a cooldown.
  sensitive_view->SetX(sensitive_view->x() + 10);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Setting `sensitive_view` visible while the parent `container` is still
  // hidden (`!sensitive_view->IsDrawn()`) does not start a cooldown.
  sensitive_view->SetVisible(true);
  EXPECT_FALSE(sensitive_view->IsDrawn());
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Making the parent `container` visible makes `sensitive_view` drawn and
  // triggers the cooldown.
  container->SetVisible(true);
  EXPECT_TRUE(sensitive_view->IsDrawn());
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  FastForwardPastCooldown(mock_protector);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       AncestorPositionChangeTriggersCooldownOnObservedChild) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* root = widget->GetRootView();
  View* container = root->AddChildView(std::make_unique<View>());
  container->SetBounds(10, 10, 200, 100);

  constexpr gfx::Rect kButtonBounds(10, 10, 80, 30);
  View* sensitive_button = container->AddChildView(std::make_unique<View>());
  sensitive_button->SetBoundsRect(kButtonBounds);

  // Observe the child button directly.
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_button);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Move the parent `container` without changing the local bounds of
  // `sensitive_button`.
  container->SetY(container->y() + 20);
  EXPECT_EQ(sensitive_button->bounds(), kButtonBounds);

  // Interaction with `sensitive_button` is blocked because the position of
  // `sensitive_button` in the root view changed when `container` moved.
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_button,
      mock_protector));

  FastForwardPastCooldown(mock_protector);
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_button,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       AncestorResizeWithoutChildMovementIgnored) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* root = widget->GetRootView();
  View* container = root->AddChildView(std::make_unique<View>());
  container->SetBounds(10, 10, 200, 100);

  View* sensitive_button = container->AddChildView(std::make_unique<View>());
  sensitive_button->SetBounds(10, 10, 80, 30);

  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_button);
  MockInputEventActivationProtector mock_protector;
  FastForwardPastCooldown(mock_protector);

  // Resize `container` without moving `sensitive_button`.
  container->SetSize(
      gfx::Size(container->width() + 50, container->height() + 50));

  // Cooldown is not triggered because the bounds of `sensitive_button` within
  // the widget did not change.
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_button,
      mock_protector));
}

TEST_F(ViewBoundsAndVisibilityInputProtectionPolicyTest,
       HidingWidgetStopsProtection) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  View* sensitive_view =
      widget->GetRootView()->AddChildView(std::make_unique<View>());
  sensitive_view->SetBounds(10, 10, 100, 30);

  // Constructing on a drawn view in a visible widget starts protection.
  ViewBoundsAndVisibilityInputProtectionPolicy policy(*sensitive_view);
  MockInputEventActivationProtector mock_protector;
  EXPECT_TRUE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  // Hiding the widget stops protection and clears the timestamp, so
  // `OnProtectionReset()` does not re-arm protection while hidden.
  widget->Hide();
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));

  policy.OnProtectionReset();
  EXPECT_FALSE(policy.IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), sensitive_view,
      mock_protector));
}

}  // namespace views::test
