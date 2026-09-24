// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/input_protection/input_protection_interactive_test.h"
#include "ui/views/input_protection/input_protection_specification.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/test/mock_activation_controller.h"
#endif

namespace views::test {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryButtonId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondaryButtonId);

constexpr gfx::Rect kInitialWidgetBounds(100, 100, 400, 400);
constexpr gfx::Point kPrimaryButtonOrigin(20, 20);
constexpr gfx::Size kButtonSize(120, 40);
constexpr int kButtonSpacing = 20;

}  // namespace

class InputProtectionInteractiveUiTest : public InputProtectionInteractiveTest {
 public:
  InputProtectionInteractiveUiTest()
      : InputProtectionInteractiveTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kEnableInputProtection);
  }

  ~InputProtectionInteractiveUiTest() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    InputProtectionInteractiveTest::SetUp();

    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(kInitialWidgetBounds);

    auto contents = std::make_unique<View>();
    auto primary_button = std::make_unique<LabelButton>(
        base::BindRepeating(
            &InputProtectionInteractiveUiTest::OnPrimaryButtonClicked,
            base::Unretained(this)),
        u"Primary Button");
    primary_button->SetProperty(kElementIdentifierKey, kPrimaryButtonId);
    primary_button->SetIsDefault(true);
    gfx::Rect primary_button_bounds(kPrimaryButtonOrigin, kButtonSize);
    primary_button->SetBoundsRect(primary_button_bounds);
    contents->AddChildView(std::move(primary_button));

    auto secondary_button = std::make_unique<LabelButton>(
        base::BindRepeating(
            &InputProtectionInteractiveUiTest::OnSecondaryButtonClicked,
            base::Unretained(this)),
        u"Secondary Button");
    secondary_button->SetProperty(kElementIdentifierKey, kSecondaryButtonId);
    gfx::Rect secondary_button_bounds(
        gfx::Point(primary_button_bounds.x(),
                   primary_button_bounds.bottom() + kButtonSpacing),
        kButtonSize);
    secondary_button->SetBoundsRect(secondary_button_bounds);
    contents->AddChildView(std::move(secondary_button));

    widget_->SetContentsView(std::move(contents));
    WidgetVisibleWaiter waiter(widget_.get());
    widget_->Show();
    waiter.Wait();
    widget_->Activate();
    WaitForWidgetActive(widget_.get(), true);

    SetContextWidget(widget_.get());
  }

  void TearDown() override {
    SetContextWidget(nullptr);
    widget_.reset();
    OccludedWidgetInputProtector::GetInstance()->ClearForTesting();
    InputProtectionInteractiveTest::TearDown();
  }

  void OnPrimaryButtonClicked() { primary_click_count_++; }
  void OnSecondaryButtonClicked() { secondary_click_count_++; }

  const int& primary_click_count() const { return primary_click_count_; }
  const int& secondary_click_count() const { return secondary_click_count_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<Widget> widget_;
  int primary_click_count_ = 0;
  int secondary_click_count_ = 0;

#if BUILDFLAG(IS_MAC)
  // Use synchronous activation to prevent native activation timeouts on macOS.
  views::test::MockActivationController activation_controller_{
      /*allow_in_interactive_ui_tests=*/true};
#endif
};

// Verifies that input event activation protection enforces the initial show
// cooldown.
TEST_F(InputProtectionInteractiveUiTest, InitialShowCooldown) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      TriggerShowCooldown(kPrimaryButtonId),
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvancePastInputProtectionInterval(),
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that rapid successive clicks (within the double click interval)
// are dropped by `DefaultInputProtectionPolicy` to prevent click spam
// hijacking.
TEST_F(InputProtectionInteractiveUiTest, RapidSuccessiveClicksBlocked) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      AdvancePastInputProtectionInterval(),
      // Initial click is allowed after show cooldown.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()),
      // Rapid second click immediately following the first is blocked.
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvancePastInputProtectionInterval(),
      // After the protection cooldown expires, subsequent click is allowed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that when a mouse press is blocked during input protection,
// `RootView::ResetEventHandlers()` discards the subsequent mouse release even
// after input protection expires, preventing unintended interactions.
TEST_F(InputProtectionInteractiveUiTest,
       MouseReleaseAfterInputProtectionDiscarded) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      TriggerShowCooldown(kPrimaryButtonId), MousePress(kPrimaryButtonId),
      AdvancePastInputProtectionInterval(), MouseRelease(kPrimaryButtonId),
      CheckVariable(primary_click_count(), 0, "primary_click_count"));
}

// Verifies that the forward focus navigation key (Tab) is not blocked during
// input protection.
TEST_F(InputProtectionInteractiveUiTest,
       TabKeyTraversalAllowedDuringInputProtection) {
  RunTestSequence(EnableInputEventActivationProtection(),
                  TriggerShowCooldown(kPrimaryButtonId),
                  FocusElement(kPrimaryButtonId),
                  KeyPressAndRelease(kPrimaryButtonId, ui::VKEY_TAB),
                  CheckViewProperty(kSecondaryButtonId, &View::HasFocus, true));
}

// Verifies that reverse focus navigation (Shift+Tab) is not blocked during
// input protection.
TEST_F(InputProtectionInteractiveUiTest,
       ShiftTabKeyTraversalAllowedDuringInputProtection) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      TriggerShowCooldown(kPrimaryButtonId), FocusElement(kSecondaryButtonId),
      KeyPressAndRelease(kSecondaryButtonId, ui::VKEY_TAB, ui::EF_SHIFT_DOWN),
      CheckViewProperty(kPrimaryButtonId, &View::HasFocus, true));
}

// Verifies that pressing the Space key on a focused button is blocked during
// input protection.
TEST_F(InputProtectionInteractiveUiTest, SpaceKeyBlockedDuringInputProtection) {
  RunTestSequence(EnableInputEventActivationProtection(),
                  TriggerShowCooldown(kPrimaryButtonId),
                  FocusElement(kPrimaryButtonId),
                  KeyPressAndReleaseExpectingBlocked(
                      kPrimaryButtonId, ui::VKEY_SPACE, primary_click_count()),
                  AdvancePastInputProtectionInterval(),
                  KeyPressAndReleaseExpectingAllowed(
                      kPrimaryButtonId, ui::VKEY_SPACE, primary_click_count()));
}

// Verifies that pressing the Return key on a focused button is blocked during
// input protection.
TEST_F(InputProtectionInteractiveUiTest,
       ReturnKeyBlockedDuringInputProtection) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      TriggerShowCooldown(kPrimaryButtonId), FocusElement(kPrimaryButtonId),
      KeyPressAndReleaseExpectingBlocked(kPrimaryButtonId, ui::VKEY_RETURN,
                                         primary_click_count()),
      AdvancePastInputProtectionInterval(),
      KeyPressAndReleaseExpectingAllowed(kPrimaryButtonId, ui::VKEY_RETURN,
                                         primary_click_count()));
}

// Verifies that an Always-On-Top window actively occluding an element blocks
// clicks, and that clicks succeed after the AOT window is removed and cooldown
// expires.
TEST_F(InputProtectionInteractiveUiTest, LiveAotWindowBlocksClicks) {
  RunTestSequence(
      OccludeElementWithAotWindow(kPrimaryButtonId),
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      HideAotWindow(), AdvancePastInputProtectionInterval(),
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that an Always-On-Top window occluding one element does not block
// clicks on an unoccluded element.
TEST_F(InputProtectionInteractiveUiTest, AotWindowAllowsUnoccludedClicks) {
  RunTestSequence(
      OccludeElementWithAotWindow(kSecondaryButtonId),
      // Clicks on the unoccluded primary button succeed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()),
      // Clicks on the occluded secondary button are blocked.
      ClickExpectingBlocked(kSecondaryButtonId, secondary_click_count()));
}

// Verifies that dismissing an active Always-On-Top window triggers historical
// occlusion, enforcing a cooldown during which clicks remain blocked.
TEST_F(InputProtectionInteractiveUiTest, AotWindowDismissalEnforcesCooldown) {
  RunTestSequence(
      OccludeElementWithAotWindow(kPrimaryButtonId), HideAotWindow(),
      // Immediately after dismissal, historical occlusion blocks clicks.
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvancePastInputProtectionInterval(),
      // After cooldown expires, clicks succeed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that a pop-away attack (AOT window shown and immediately hidden)
// enforces cooldown during which clicks are blocked.
TEST_F(InputProtectionInteractiveUiTest, PopAwayAttackEnforcesCooldown) {
  RunTestSequence(
      TriggerAotPopAwayAttack(kPrimaryButtonId),
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvanceHalfwayThroughInputProtectionInterval(),
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvancePastInputProtectionInterval(),
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that a moved Always-On-Top window records historical occlusion at
// the vacated position, blocking clicks during the post-move cooldown.
TEST_F(InputProtectionInteractiveUiTest, MovedAotWindowEnforcesCooldown) {
  RunTestSequence(
      OccludeElementWithAotWindow(kPrimaryButtonId),
      MoveAotWindowToUnocclude(kPrimaryButtonId),
      // Vacated area is still protected by historical occlusion.
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count()),
      AdvancePastInputProtectionInterval(),
      // After cooldown expires, clicks at the vacated position succeed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count()));
}

// Verifies that view defined protected bounds via
// `InputProtectionSpecification` are respected by occlusion detection.
TEST_F(InputProtectionInteractiveUiTest, CustomProtectedBoundsEnforced) {
  // Designate the left half of the element as protected.
  constexpr int kProtectedWidth = kButtonSize.width() / 2;
  const gfx::Rect protected_region(0, 0, kProtectedWidth, kButtonSize.height());

  // Point inside the protected region (center of protected bounds).
  const gfx::Point inside_point = protected_region.CenterPoint();

  // Point outside the protected region (center of remaining unprotected area).
  const gfx::Point outside_point((kProtectedWidth + kButtonSize.width()) / 2,
                                 kButtonSize.height() / 2);

  RunTestSequence(
      InstallInputProtectionSpecification(
          kPrimaryButtonId, base::BindRepeating(
                                [](const gfx::Rect& region,
                                   const View* view) -> std::vector<gfx::Rect> {
                                  return {region};
                                },
                                protected_region)),
      OccludeElementWithAotWindow(kPrimaryButtonId), HideAotWindow(),
      // Clicks within the protected region are blocked during cooldown.
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count(),
                            inside_point),
      // Clicks outside the protected region are permitted.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count(),
                            outside_point),
      AdvancePastInputProtectionInterval(),
      // After cooldown expires, clicks within the protected region succeed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count(),
                            inside_point));
}

// Verifies that when an element is fully occluded by an Always-On-Top window,
// key presses are blocked by default (without requiring a specification), and
// remain blocked during the post-dismissal cooldown.
TEST_F(InputProtectionInteractiveUiTest, FullyOccludedKeyEventsBlocked) {
  RunTestSequence(
      EnableInputEventActivationProtection(),
      AdvancePastInputProtectionInterval(),
      OccludeElementWithAotWindow(kPrimaryButtonId),
      // Reactivate target surface so it can receive focus (required on macOS).
      ActivateSurface(kPrimaryButtonId), FocusElement(kPrimaryButtonId),
      // Fully occluded element blocks Space key without a specification.
      KeyPressAndReleaseExpectingBlocked(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()),
      HideAotWindow(),
      // Immediately after dismissal, historical occlusion blocks Space key.
      KeyPressAndReleaseExpectingBlocked(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()),
      AdvancePastInputProtectionInterval(),
      // After cooldown expires, Space key succeeds.
      KeyPressAndReleaseExpectingAllowed(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()));
}

// Verifies that partially occluding an element allows key presses by default,
// but blocks them if an `InputProtectionSpecification` is installed and
// intersects the occluded region.
TEST_F(InputProtectionInteractiveUiTest,
       PartiallyOccludedKeyEventsRespectCustomBounds) {
  // Designate the right half of the element as protected.
  constexpr int kProtectedWidth = kButtonSize.width() / 2;
  const gfx::Rect protected_region(kButtonSize.width() - kProtectedWidth, 0,
                                   kProtectedWidth, kButtonSize.height());

  // Occlude only the right quarter of the element (partially occluding the
  // protected region).
  constexpr int kOccludedWidth = kButtonSize.width() / 4;
  const gfx::Rect occluded_region(kButtonSize.width() - kOccludedWidth, 0,
                                  kOccludedWidth, kButtonSize.height());

  RunTestSequence(
      EnableInputEventActivationProtection(),
      AdvancePastInputProtectionInterval(),
      // Partially occlude only a slice of the element.
      OccludeRectWithAotWindow(kPrimaryButtonId, occluded_region),
      // Reactivate target surface so it can receive focus (required on macOS).
      ActivateSurface(kPrimaryButtonId), FocusElement(kPrimaryButtonId),
      // Without spec: Check allows Space key because view is not fully
      // occluded.
      KeyPressAndReleaseExpectingAllowed(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()),
      // Install spec designating the right half as protected.
      InstallInputProtectionSpecification(
          kPrimaryButtonId, base::BindRepeating(
                                [](const gfx::Rect& region,
                                   const View* view) -> std::vector<gfx::Rect> {
                                  return {region};
                                },
                                protected_region)),
      // With spec: Check blocks Space key because the occluding window
      // intersects the protected region.
      KeyPressAndReleaseExpectingBlocked(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()),
      HideAotWindow(), AdvancePastInputProtectionInterval(),
      // After cooldown expires, Space key succeeds.
      KeyPressAndReleaseExpectingAllowed(kPrimaryButtonId, ui::VKEY_SPACE,
                                         primary_click_count()));
}

// Verifies that the forward focus navigation key (Tab) is not blocked even
// when an element is actively occluded by an Always-On-Top window.
TEST_F(InputProtectionInteractiveUiTest, TabKeyTraversalAllowedWhileOccluded) {
  RunTestSequence(
      OccludeElementWithAotWindow(kPrimaryButtonId),
      // Reactivate target surface so it can receive focus (required on macOS).
      ActivateSurface(kPrimaryButtonId), FocusElement(kPrimaryButtonId),
      KeyPressAndRelease(kPrimaryButtonId, ui::VKEY_TAB),
      CheckViewProperty(kSecondaryButtonId, &View::HasFocus, true));
}

}  // namespace views::test
