// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/window_activation_input_protection_policy.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class TestBubbleDialogDelegate : public BubbleDialogDelegate {
 public:
  explicit TestBubbleDialogDelegate(View* anchor_view)
      : BubbleDialogDelegate(anchor_view, BubbleBorder::TOP_LEFT) {
    set_close_on_deactivate(false);
  }
};

class WindowActivationInputProtectionPolicyTest : public WidgetTest {
 public:
  WindowActivationInputProtectionPolicyTest()
      : WidgetTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
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

  ui::MouseEvent CreateMouseEvent(const gfx::Point& screen_point,
                                  View* target_view) {
    gfx::Point local_point = screen_point;
    View::ConvertPointFromScreen(target_view, &local_point);
    return ui::MouseEvent(ui::EventType::kMousePressed, local_point,
                          local_point, ui::EventTimeForNow(), 0, 0);
  }

  // Shows the widget, waits for it to become visible, and synchronously
  // activates it. We use this helper in tests to ensure that visibility and
  // activation events are processed synchronously, which is required on Mac
  // where native activation is simulated via `SimulateNativeActivate()`.
  void ShowAndActivateWidget(Widget& widget) {
    widget.Show();
    WidgetVisibleWaiter(&widget).Wait();
    SimulateNativeActivate(&widget);
  }

  // Deactivates the given widget. On Mac, this creates and returns a temporary
  // helper widget to steal focus. To keep the target widget deactivated, the
  // caller must keep the returned widget alive.
  [[nodiscard]] std::unique_ptr<Widget> DeactivateWidget(Widget* widget) {
#if !BUILDFLAG(IS_MAC)
    widget->Deactivate();
    return nullptr;
#else
    // macOS does not support `Widget::Deactivate()`. To force the widget to
    // lose activation, we must explicitly activate another widget to steal
    // focus.
    auto focus_stealer = CreateWidgetWithZOrder();
    ShowAndActivateWidget(*focus_stealer);
    return focus_stealer;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_FUCHSIA) && defined(ARCH_CPU_ARM64) && !defined(NDEBUG)
// TODO(crbug.com/464455929): Crash in llvm on Fuchsia arm64 in debug.
#define MAYBE_ParentInvisibleOnActivation_TriggersProtection \
  DISABLED_ParentInvisibleOnActivation_TriggersProtection
#else
#define MAYBE_ParentInvisibleOnActivation_TriggersProtection \
  ParentInvisibleOnActivation_TriggersProtection
#endif
TEST_F(WindowActivationInputProtectionPolicyTest,
       MAYBE_ParentInvisibleOnActivation_TriggersProtection) {
  // Create parent widget.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  ShowAndActivateWidget(*parent_widget);

  // Create child widget (bubble) anchored to parent.
  auto bubble_delegate = std::make_unique<TestBubbleDialogDelegate>(
      parent_widget->GetContentsView());
  std::unique_ptr<Widget> child_widget =
      BubbleDialogDelegate::CreateBubble(bubble_delegate.get());

  // Construct policy for child.
  auto policy = std::make_unique<WindowActivationInputProtectionPolicy>(
      child_widget.get());

  MockInputEventActivationProtector mock_protector;

  // Show child.
  ShowAndActivateWidget(*child_widget);

  // Since we just showed the child widget and the parent was visible, the
  // interaction should not be blocked.
  gfx::Point child_center =
      child_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  ui::MouseEvent initial_click_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      initial_click_allowed, child_widget->GetRootView(), mock_protector));

  // Hide parent.
  parent_widget->Hide();
  auto focus_stealer = DeactivateWidget(child_widget.get());
  WidgetVisibleWaiter(parent_widget.get()).WaitUntilInvisible();

  // Fast forward past the cooldown.
  FastForwardBy(mock_protector.cooldown_interval() + base::Milliseconds(1));

  // Now show parent and activate child.
  ShowAndActivateWidget(*parent_widget);
  ShowAndActivateWidget(*child_widget);

  // Click immediately. It should be blocked because parent was previously
  // invisible, so activation reset the protection cooldown.
  ui::MouseEvent click_after_restore_blocked =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      click_after_restore_blocked, child_widget->GetRootView(),
      mock_protector));

  // Fast forward past cooldown again.
  FastForwardBy(mock_protector.cooldown_interval() + base::Milliseconds(1));

  // Click should be allowed.
  ui::MouseEvent click_after_cooldown_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      click_after_cooldown_allowed, child_widget->GetRootView(),
      mock_protector));
}

TEST_F(WindowActivationInputProtectionPolicyTest,
       ChildHideThenShowWithParentVisible_DoesNotTriggerProtection) {
  // Create parent widget.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  ShowAndActivateWidget(*parent_widget);

  // Create child widget (bubble) anchored to parent.
  auto bubble_delegate = std::make_unique<TestBubbleDialogDelegate>(
      parent_widget->GetContentsView());
  std::unique_ptr<Widget> child_widget =
      BubbleDialogDelegate::CreateBubble(bubble_delegate.get());

  // Construct policy for child.
  auto policy = std::make_unique<WindowActivationInputProtectionPolicy>(
      child_widget.get());

  MockInputEventActivationProtector mock_protector;

  // Show child.
  ShowAndActivateWidget(*child_widget);

  // Verify click is allowed immediately. Since the parent window was visible
  // when the child was shown, no activation cooldown is triggered.
  const gfx::Point child_center =
      child_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  ui::MouseEvent initial_click_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      initial_click_allowed, child_widget->GetRootView(), mock_protector));

  // Explicitly hide child. Parent remains visible.
  child_widget->Hide();
  WidgetVisibleWaiter(child_widget.get()).WaitUntilInvisible();
  EXPECT_TRUE(parent_widget->IsVisible());

  // Show child again and activate.
  ShowAndActivateWidget(*child_widget);

  // Click immediately. It should not be blocked because parent was visible the
  // whole time, so activation should not reset the protection cooldown.
  ui::MouseEvent click_after_child_show_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      click_after_child_show_allowed, child_widget->GetRootView(),
      mock_protector));
}

TEST_F(WindowActivationInputProtectionPolicyTest,
       ParentVisibleAtPolicyCreation_DoesNotTriggerProtection) {
  // Create parent widget.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  ShowAndActivateWidget(*parent_widget);

  // Create child widget (bubble) anchored to parent.
  auto bubble_delegate = std::make_unique<TestBubbleDialogDelegate>(
      parent_widget->GetContentsView());
  std::unique_ptr<Widget> child_widget =
      BubbleDialogDelegate::CreateBubble(bubble_delegate.get());

  // Show child.
  ShowAndActivateWidget(*child_widget);

  // Now construct policy for child (after it is already visible).
  auto policy = std::make_unique<WindowActivationInputProtectionPolicy>(
      child_widget.get());

  // Activate child.
  SimulateNativeActivate(child_widget.get());

  // Click immediately. It should not be blocked because parent was visible when
  // the policy was created, so it should have initialized to visible and not
  // triggered protection on activation.
  const gfx::Point child_center =
      child_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  MockInputEventActivationProtector mock_protector;
  ui::MouseEvent click_after_activation_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      click_after_activation_allowed, child_widget->GetRootView(),
      mock_protector));
}

TEST_F(WindowActivationInputProtectionPolicyTest,
       ReactivationWithParentVisible_DoesNotTriggerProtection) {
  // Create parent widget.
  auto parent_widget = CreateWidgetWithZOrder();
  parent_widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  ShowAndActivateWidget(*parent_widget);

  // Create child widget (bubble) anchored to parent.
  auto bubble_delegate = std::make_unique<TestBubbleDialogDelegate>(
      parent_widget->GetContentsView());
  std::unique_ptr<Widget> child_widget =
      BubbleDialogDelegate::CreateBubble(bubble_delegate.get());

  // Construct policy for child.
  auto policy = std::make_unique<WindowActivationInputProtectionPolicy>(
      child_widget.get());

  MockInputEventActivationProtector mock_protector;

  // Show child.
  ShowAndActivateWidget(*child_widget);

  // Verify click is allowed.
  const gfx::Point child_center =
      child_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  ui::MouseEvent initial_click_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      initial_click_allowed, child_widget->GetRootView(), mock_protector));

  // Deactivate child (simulating popup taking focus). Parent remains visible.
  auto focus_stealer = DeactivateWidget(child_widget.get());
  EXPECT_TRUE(parent_widget->IsVisible());

  // Activate child again (simulating popup closing).
  SimulateNativeActivate(child_widget.get());

  // Click immediately. It should not be blocked because parent was visible.
  ui::MouseEvent click_after_reactivation_allowed =
      CreateMouseEvent(child_center, child_widget->GetRootView());
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      click_after_reactivation_allowed, child_widget->GetRootView(),
      mock_protector));
}

}  // namespace views::test
