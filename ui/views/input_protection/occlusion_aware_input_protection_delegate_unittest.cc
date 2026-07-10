// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/occlusion_aware_input_protection_delegate.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/metrics.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

class OcclusionAwareInputProtectionDelegateTest : public WidgetTest {
 public:
  OcclusionAwareInputProtectionDelegateTest()
      : WidgetTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
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

  ui::MouseEvent CreateMouseEvent(const gfx::Point& screen_point,
                                  View* target_view) {
    gfx::Point local_point = screen_point;
    View::ConvertPointFromScreen(target_view, &local_point);
    return ui::MouseEvent(ui::EventType::kMousePressed, local_point,
                          local_point, ui::EventTimeForNow(), 0, 0);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OcclusionAwareInputProtectionDelegateTest, CurrentOcclusionBlocks) {
  const gfx::Rect kWidgetBounds(100, 100, 200, 200);

  // Create AOT widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kWidgetBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create the protected widget.
  auto protected_widget = CreateWidgetWithZOrder();
  protected_widget->SetBounds(kWidgetBounds);

  // Instantiate the delegate directly.
  auto delegate = std::make_unique<OcclusionAwareInputProtectionDelegate>(
      protected_widget.get());

  // Show the protected widget.
  protected_widget->Show();
  WidgetVisibleWaiter(protected_widget.get()).Wait();

  // Click at the center of the protected widget (which is occluded by the AOT
  // widget).
  gfx::Point protected_center =
      protected_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  ui::MouseEvent event =
      CreateMouseEvent(protected_center, protected_widget->GetRootView());

  MockInputEventActivationProtector mock_protector;

  // Verify that the interaction is blocked because the widget is currently
  // occluded.
  EXPECT_TRUE(delegate->IsPossiblyUnintendedInteraction(
      event, protected_widget->GetRootView(), &mock_protector));
}

TEST_F(OcclusionAwareInputProtectionDelegateTest, RecentOcclusionBlocks) {
  const gfx::Rect kWidgetBounds(100, 100, 200, 200);

  // Create AOT widget.
  auto aot_widget = CreateWidgetWithZOrder(ui::ZOrderLevel::kFloatingWindow);
  aot_widget->SetBounds(kWidgetBounds);
  aot_widget->Show();
  WidgetVisibleWaiter(aot_widget.get()).Wait();

  // Create the protected widget.
  auto protected_widget = CreateWidgetWithZOrder();
  protected_widget->SetBounds(kWidgetBounds);

  // Instantiate the delegate directly.
  auto delegate = std::make_unique<OcclusionAwareInputProtectionDelegate>(
      protected_widget.get());

  // Show the protected widget.
  protected_widget->Show();
  WidgetVisibleWaiter(protected_widget.get()).Wait();

  // Hide the AOT widget, revealing the protected widget.
  aot_widget->Hide();
  WidgetVisibleWaiter(aot_widget.get()).WaitUntilInvisible();

  // Click at the center of the protected widget (which was recently occluded).
  gfx::Point protected_center =
      protected_widget->GetNonDecoratedClientAreaBoundsInScreen().CenterPoint();
  ui::MouseEvent event =
      CreateMouseEvent(protected_center, protected_widget->GetRootView());

  MockInputEventActivationProtector mock_protector;

  // Verify that the interaction is blocked because the widget was recently
  // occluded.
  EXPECT_TRUE(delegate->IsPossiblyUnintendedInteraction(
      event, protected_widget->GetRootView(), &mock_protector));

  // Fast forward past the reveal cooldown.
  FastForwardBy(GetDoubleClickInterval() + base::Milliseconds(1));

  ui::MouseEvent event_after =
      CreateMouseEvent(protected_center, protected_widget->GetRootView());

  // Verify that the interaction is no longer blocked after the cooldown
  // expires.
  EXPECT_FALSE(delegate->IsPossiblyUnintendedInteraction(
      event_after, protected_widget->GetRootView(), &mock_protector));
}

TEST_F(OcclusionAwareInputProtectionDelegateTest, NullTargetViewCrashes) {
  auto protected_widget = CreateWidgetWithZOrder();
  auto delegate = std::make_unique<OcclusionAwareInputProtectionDelegate>(
      protected_widget.get());

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  MockInputEventActivationProtector mock_protector;

  EXPECT_DEATH_IF_SUPPORTED(delegate->IsPossiblyUnintendedInteraction(
                                event, nullptr, &mock_protector),
                            "");
}

}  // namespace views::test
