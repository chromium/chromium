// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/input_protection/input_protection_interactive_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryButtonId);

constexpr gfx::Rect kInitialWidgetBounds(100, 100, 400, 400);
constexpr gfx::Size kButtonSize(120, 40);
constexpr int kButtonMargin = 20;

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
    auto button = std::make_unique<LabelButton>(
        base::BindRepeating(
            &InputProtectionInteractiveUiTest::OnPrimaryButtonClicked,
            base::Unretained(this)),
        u"Primary Button");
    button->SetProperty(kElementIdentifierKey, kPrimaryButtonId);
    button->SetBoundsRect(
        gfx::Rect(gfx::Point(kButtonMargin, kButtonMargin), kButtonSize));
    contents->AddChildView(std::move(button));

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
    InputProtectionInteractiveTest::TearDown();
  }

  void OnPrimaryButtonClicked() { primary_click_count_++; }

  const int& primary_click_count() const { return primary_click_count_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<Widget> widget_;
  int primary_click_count_ = 0;
};

// Verifies that input event activation protection enforces the initial show
// cooldown.
TEST_F(InputProtectionInteractiveUiTest, InitialShowCooldown) {
  RunTestSequence(
      EnableInputEventActivationProtection(kPrimaryButtonId),
      TriggerShowCooldown(kPrimaryButtonId),
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count(), 0),
      AdvancePastInputProtectionInterval(),
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count(), 1));
}

// Verifies that rapid successive clicks (within the double click interval)
// are dropped by `DefaultInputProtectionPolicy` to prevent click spam
// hijacking.
TEST_F(InputProtectionInteractiveUiTest, RapidSuccessiveClicksBlocked) {
  RunTestSequence(
      EnableInputEventActivationProtection(kPrimaryButtonId),
      AdvancePastInputProtectionInterval(),
      // Initial click is allowed after show cooldown.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count(), 1),
      // Rapid second click immediately following the first is blocked.
      ClickExpectingBlocked(kPrimaryButtonId, primary_click_count(), 1),
      AdvancePastInputProtectionInterval(),
      // After the protection cooldown expires, subsequent click is allowed.
      ClickExpectingAllowed(kPrimaryButtonId, primary_click_count(), 2));
}

}  // namespace views::test
