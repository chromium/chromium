// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/default_input_protection_policy.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "ui/events/event.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class DefaultInputProtectionPolicyTest : public WidgetTest {
 public:
  DefaultInputProtectionPolicyTest()
      : WidgetTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  ui::MouseEvent CreateClickEvent(base::TimeTicks timestamp) {
    return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                          gfx::Point(), timestamp, 0, 0);
  }

  ui::KeyEvent CreateKeyRepeatEvent(base::TimeTicks timestamp) {
    return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A,
                        ui::EF_IS_REPEAT, timestamp);
  }
};

TEST_F(DefaultInputProtectionPolicyTest, VisibilityCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto policy =
      std::make_unique<DefaultInputProtectionPolicy>(widget->GetRootView());
  MockInputEventActivationProtector mock_protector;

  // Initially not protected.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Blocked immediately after show.
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Fast forward past cooldown.
  task_environment()->FastForwardBy(mock_protector.cooldown_interval() +
                                    base::Milliseconds(1));

  // Allowed after cooldown.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Hiding and showing the widget should restart the protection cooldown.
  widget->Hide();
  WidgetVisibleWaiter(widget.get()).WaitUntilInvisible();
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Blocked again after show.
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));
}

TEST_F(DefaultInputProtectionPolicyTest, NoProtectionWithoutObservation) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  // Simulates legacy path (nullptr view). Without manual visibility
  // notifications, the policy must default to allowing all interactions.
  auto policy = std::make_unique<DefaultInputProtectionPolicy>(nullptr);
  MockInputEventActivationProtector mock_protector;

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Clicks are allowed.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Even key repeats are allowed.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateKeyRepeatEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));
}

TEST_F(DefaultInputProtectionPolicyTest, KeyRepeatBlocked) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto policy =
      std::make_unique<DefaultInputProtectionPolicy>(widget->GetRootView());
  MockInputEventActivationProtector mock_protector;

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Fast forward past cooldown.
  task_environment()->FastForwardBy(mock_protector.cooldown_interval() +
                                    base::Milliseconds(1));

  // Regular click is allowed.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Key repeat is blocked.
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      CreateKeyRepeatEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));
}

TEST_F(DefaultInputProtectionPolicyTest, RapidClicksBlocked) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto policy =
      std::make_unique<DefaultInputProtectionPolicy>(widget->GetRootView());
  MockInputEventActivationProtector mock_protector;

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Fast forward past cooldown.
  task_environment()->FastForwardBy(mock_protector.cooldown_interval() +
                                    base::Milliseconds(1));

  // Click 1: Allowed.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Click 2 (rapid): Blocked.
  task_environment()->FastForwardBy(mock_protector.cooldown_interval() -
                                    base::Milliseconds(1));
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Click 3 (after waiting): Allowed.
  task_environment()->FastForwardBy(mock_protector.cooldown_interval() +
                                    base::Milliseconds(1));
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));
}

TEST_F(DefaultInputProtectionPolicyTest, ResetCooldown) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);

  auto policy =
      std::make_unique<DefaultInputProtectionPolicy>(widget->GetRootView());
  MockInputEventActivationProtector mock_protector;

  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  const base::TimeDelta cooldown = mock_protector.cooldown_interval();

  // Wait almost until cooldown expires (1ms before).
  task_environment()->FastForwardBy(cooldown - base::Milliseconds(1));

  // Reset protection (simulating window movement).
  policy->OnProtectionReset();

  // Fast forward 2ms, which pushes past the original cooldown's expiry.
  // (Original cooldown would have expired in 1ms, but the new one needs
  // `cooldown` to expire).
  task_environment()->FastForwardBy(base::Milliseconds(2));

  // Should still be blocked because the new cooldown is active.
  // This is not blocked by rapid click protection as it is the first event.
  EXPECT_TRUE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));

  // Fast forward past the new cooldown (adding 1ms to avoid rapid click on the
  // next check).
  task_environment()->FastForwardBy(cooldown + base::Milliseconds(1));

  // Allowed now.
  EXPECT_FALSE(policy->IsPossiblyUnintendedInteraction(
      CreateClickEvent(base::TimeTicks::Now()), widget->GetRootView(),
      mock_protector));
}

TEST_F(DefaultInputProtectionPolicyTest, ObservedViewDeletedFirstDoesNotCrash) {
  std::unique_ptr<Widget> widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto policy =
      std::make_unique<DefaultInputProtectionPolicy>(widget->GetRootView());
  widget->Show();
  WidgetVisibleWaiter(widget.get()).Wait();

  // Deleting the observed `View` triggers `OnViewIsDeleting` to reset the
  // observation.
  widget.reset();

  // Deleting the policy should not crash.
  policy.reset();
}

}  // namespace views::test
