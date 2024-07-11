// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/animation/test/test_ink_drop_host.h"
#include "ui/views/test/views_test_base.h"

namespace views {

// NOTE: The InkDropImpl class is also tested by the InkDropFactoryTest tests.
class InkDropImplTest : public ViewsTestBase {
 public:
  explicit InkDropImplTest(InkDropImpl::AutoHighlightMode auto_highlight_mode =
                               InkDropImpl::AutoHighlightMode::NONE)
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        auto_highlight_mode_(auto_highlight_mode) {}
  ~InkDropImplTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetContentsView(
        std::make_unique<TestInkDropHost>(auto_highlight_mode_));
    InkDrop::Get(ink_drop_host())->SetMode(views::InkDropHost::InkDropMode::ON);
    ink_drop_host()->set_disable_timers_for_test(true);
    widget_->Show();
  }

  void TearDown() override {
    DestroyWidget();
    ViewsTestBase::TearDown();
  }

 protected:
  TestInkDropHost* ink_drop_host() {
    return static_cast<TestInkDropHost*>(widget_->GetContentsView());
  }
  InkDropImpl* ink_drop() {
    return static_cast<InkDropImpl*>(
        InkDrop::Get(ink_drop_host())->GetInkDrop());
  }
  InkDropRipple* ink_drop_ripple() {
    return ink_drop()->ink_drop_ripple_.get();
  }
  InkDropHighlight* ink_drop_highlight() {
    return ink_drop()->highlight_.get();
  }
  test::InkDropImplTestApi test_api() {
    return test::InkDropImplTestApi(ink_drop());
  }
  Widget* widget() { return widget_.get(); }

  // Returns true if the ink drop layers have been added to `ink_drop_host()`.
  bool AreLayersAddedToHost() {
    return ink_drop_host()->num_ink_drop_layers() >= 1;
  }

  void DestroyInkDrop() {
    InkDrop::Get(ink_drop_host())
        ->SetMode(views::InkDropHost::InkDropMode::OFF);
  }

  void DestroyWidget() { widget_.reset(); }

 private:
  const InkDropImpl::AutoHighlightMode auto_highlight_mode_;
  std::unique_ptr<Widget> widget_;
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

// AutoHighlightMode parameterized test fixture.
class InkDropImplAutoHighlightTest
    : public InkDropImplTest,
      public testing::WithParamInterface<
          testing::tuple<InkDropImpl::AutoHighlightMode>> {
 public:
  InkDropImplAutoHighlightTest();

  InkDropImplAutoHighlightTest(const InkDropImplAutoHighlightTest&) = delete;
  InkDropImplAutoHighlightTest& operator=(const InkDropImplAutoHighlightTest&) =
      delete;

  ~InkDropImplAutoHighlightTest() override;
};

InkDropImplAutoHighlightTest::InkDropImplAutoHighlightTest()
    : InkDropImplTest(testing::get<0>(GetParam())) {}

InkDropImplAutoHighlightTest::~InkDropImplAutoHighlightTest() = default;

////////////////////////////////////////////////////////////////////////////////
//
// InkDropImpl tests
//

TEST_F(InkDropImplTest, ShouldHighlight) {
  ink_drop()->SetShowHighlightOnHover(false);
  ink_drop()->SetHovered(false);
  ink_drop()->SetShowHighlightOnFocus(false);
  ink_drop()->SetFocused(false);
  EXPECT_FALSE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(true);
  ink_drop()->SetHovered(false);
  ink_drop()->SetShowHighlightOnFocus(false);
  ink_drop()->SetFocused(false);
  EXPECT_FALSE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(false);
  ink_drop()->SetHovered(true);
  ink_drop()->SetShowHighlightOnFocus(false);
  ink_drop()->SetFocused(false);
  EXPECT_FALSE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(false);
  ink_drop()->SetHovered(false);
  ink_drop()->SetShowHighlightOnFocus(true);
  ink_drop()->SetFocused(false);
  EXPECT_FALSE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(false);
  ink_drop()->SetHovered(false);
  ink_drop()->SetShowHighlightOnFocus(false);
  ink_drop()->SetFocused(true);
  EXPECT_FALSE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(true);
  ink_drop()->SetHovered(true);
  ink_drop()->SetShowHighlightOnFocus(false);
  ink_drop()->SetFocused(false);
  EXPECT_TRUE(test_api().ShouldHighlight());

  ink_drop()->SetShowHighlightOnHover(false);
  ink_drop()->SetHovered(false);
  ink_drop()->SetShowHighlightOnFocus(true);
  ink_drop()->SetFocused(true);
  EXPECT_TRUE(test_api().ShouldHighlight());

  test_api().SetShouldHighlight(false);
  EXPECT_FALSE(test_api().ShouldHighlight());

  test_api().SetShouldHighlight(true);
  EXPECT_TRUE(test_api().ShouldHighlight());
}

TEST_F(InkDropImplTest,
       VerifyInkDropLayersRemovedWhenPresentDuringDestruction) {
  test_api().SetShouldHighlight(true);
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_TRUE(AreLayersAddedToHost());
  DestroyInkDrop();
  EXPECT_FALSE(AreLayersAddedToHost());
}

// Test that (re-)hiding or un-hovering a hidden ink drop doesn't add layers.
TEST_F(InkDropImplTest, AlwaysHiddenInkDropHasNoLayers) {
  EXPECT_FALSE(AreLayersAddedToHost());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  EXPECT_FALSE(AreLayersAddedToHost());

  ink_drop()->SetHovered(false);
  EXPECT_FALSE(AreLayersAddedToHost());
}

TEST_F(InkDropImplTest, LayersRemovedFromHostAfterHighlight) {
  EXPECT_FALSE(AreLayersAddedToHost());

  test_api().SetShouldHighlight(true);
  EXPECT_TRUE(AreLayersAddedToHost());

  test_api().CompleteAnimations();

  test_api().SetShouldHighlight(false);
  test_api().CompleteAnimations();
  EXPECT_FALSE(AreLayersAddedToHost());
}

TEST_F(InkDropImplTest, LayersRemovedFromHostAfterInkDrop) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  EXPECT_FALSE(AreLayersAddedToHost());

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  EXPECT_TRUE(AreLayersAddedToHost());

  test_api().CompleteAnimations();

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  EXPECT_TRUE(AreLayersAddedToHost());

  test_api().CompleteAnimations();
  EXPECT_FALSE(AreLayersAddedToHost());
}

TEST_F(InkDropImplTest, LayersArentRemovedWhenPreemptingFadeOut) {
  EXPECT_FALSE(AreLayersAddedToHost());

  test_api().SetShouldHighlight(true);
  EXPECT_TRUE(AreLayersAddedToHost());

  test_api().CompleteAnimations();

  ink_drop()->SetHovered(false);
  EXPECT_TRUE(AreLayersAddedToHost());

  ink_drop()->SetHovered(true);
  EXPECT_TRUE(AreLayersAddedToHost());
}

TEST_F(InkDropImplTest,
       SettingHighlightStateDuringStateExitIsntAllowedDeathTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  test::InkDropImplTestApi::SetStateOnExitHighlightState::Install(
      test_api().state_factory());
  EXPECT_DCHECK_DEATH(
      test::InkDropImplTestApi::AccessFactoryOnExitHighlightState::Install(
          test_api().state_factory()));
  // Set `highlight_state_` directly because `SetStateOnExitHighlightState` will
  // recursively try to set it during tear down and cause a stack overflow.
  test_api().SetHighlightState(nullptr);
  DestroyInkDrop();
}

// Verifies there are no use after free errors.
TEST_F(InkDropImplTest,
       TearingDownHighlightStateThatAccessesTheStateFactoryIsSafe) {
  test::InkDropImplTestApi::AccessFactoryOnExitHighlightState::Install(
      test_api().state_factory());
  test::InkDropImplTestApi::AccessFactoryOnExitHighlightState::Install(
      test_api().state_factory());
}

// Tests that if during destruction, a ripple animation is successfully ended,
// no crash happens (see https://crbug.com/663579).
TEST_F(InkDropImplTest, SuccessfulAnimationEndedDuringDestruction) {
  // Start a ripple animation with non-zero duration.
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  {
    // Start another ripple animation with zero duration that would be queued
    // until the previous one is finished/aborted.
    ui::ScopedAnimationDurationScaleMode duration_mode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    ink_drop()->AnimateToState(InkDropState::ACTION_TRIGGERED);
  }
  // Abort the first animation, so that the queued animation is started (and
  // finished immediately since it has zero duration). No crash should happen.
  DestroyInkDrop();
}

// Make sure the InkDropRipple and InkDropHighlight get recreated when the host
// size changes (https:://crbug.com/899104).
TEST_F(InkDropImplTest, RippleAndHighlightRecreatedOnSizeChange) {
  test_api().SetShouldHighlight(true);
  ink_drop()->AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(1, ink_drop_host()->num_ink_drop_ripples_created());
  EXPECT_EQ(1, ink_drop_host()->num_ink_drop_highlights_created());

  const gfx::Rect bounds(5, 6, 7, 8);
  ink_drop_host()->SetBoundsRect(bounds);
  EXPECT_EQ(2, ink_drop_host()->num_ink_drop_ripples_created());
  EXPECT_EQ(2, ink_drop_host()->num_ink_drop_highlights_created());
  EXPECT_EQ(bounds.size(), ink_drop_ripple()->GetRootLayer()->size());
  EXPECT_EQ(bounds.size(), ink_drop_highlight()->layer()->size());
}

// Make sure the InkDropRipple and InkDropHighlight get recreated when the host
// theme changes.
TEST_F(InkDropImplTest, RippleAndHighlightRecreatedOnHostThemeChange) {
  test_api().SetShouldHighlight(true);
  ink_drop()->AnimateToState(InkDropState::ACTIVATED);
  EXPECT_EQ(1, ink_drop_host()->num_ink_drop_ripples_created());
  EXPECT_EQ(1, ink_drop_host()->num_ink_drop_highlights_created());

  ui::TestNativeTheme native_theme;
  native_theme.SetDarkMode(true);
  widget()->SetNativeThemeForTest(&native_theme);
  EXPECT_EQ(2, ink_drop_host()->num_ink_drop_ripples_created());
  EXPECT_EQ(2, ink_drop_host()->num_ink_drop_highlights_created());
  DestroyWidget();
}

// Verifies that the host's GetHighlighted() method reflects the ink drop's
// highlight state, and when the state changes the ink drop notifies the host.
TEST_F(InkDropImplTest, HostTracksHighlightState) {
  bool callback_called = false;
  auto subscription =
      InkDrop::Get(ink_drop_host())
          ->AddHighlightedChangedCallback(base::BindRepeating(
              [](bool* called) { *called = true; }, &callback_called));
  EXPECT_FALSE(InkDrop::Get(ink_drop_host())->GetHighlighted());

  test_api().SetShouldHighlight(true);
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(InkDrop::Get(ink_drop_host())->GetHighlighted());
  callback_called = false;

  test_api().SetShouldHighlight(false);
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(InkDrop::Get(ink_drop_host())->GetHighlighted());
}

////////////////////////////////////////////////////////////////////////////////
//
// Common AutoHighlightMode tests
//

using InkDropImplCommonAutoHighlightTest = InkDropImplAutoHighlightTest;
// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(
    All,
    InkDropImplCommonAutoHighlightTest,
    testing::Values(InkDropImpl::AutoHighlightMode::NONE,
                    InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE,
                    InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE));

// Verifies InkDropImplTestApi::SetShouldHighlight() works as expected.
TEST_P(InkDropImplCommonAutoHighlightTest,
       ShouldHighlightCausesHighlightToBeVisible) {
  test_api().SetShouldHighlight(true);
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  test_api().SetShouldHighlight(false);
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplCommonAutoHighlightTest,
       HighlightVisibilityForFocusAndHoverStates) {
  ink_drop()->SetShowHighlightOnHover(true);
  ink_drop()->SetShowHighlightOnFocus(true);

  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetFocused(true);
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetHovered(false);
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetHovered(true);
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetFocused(false);
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetHovered(false);
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

////////////////////////////////////////////////////////////////////////////////
//
// InkDropImpl::AutoHighlightMode::NONE specific tests
//

using InkDropImplNoAutoHighlightTest = InkDropImplAutoHighlightTest;
// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(All,
                         InkDropImplNoAutoHighlightTest,
                         testing::Values(InkDropImpl::AutoHighlightMode::NONE));

TEST_P(InkDropImplNoAutoHighlightTest, VisibleHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplNoAutoHighlightTest, HiddenHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(false);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

////////////////////////////////////////////////////////////////////////////////
//
// InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE specific tests
//

using InkDropImplHideAutoHighlightTest = InkDropImplAutoHighlightTest;
// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(
    All,
    InkDropImplHideAutoHighlightTest,
    testing::Values(InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE));

TEST_P(InkDropImplHideAutoHighlightTest,
       VisibleHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       HiddenHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(false);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest, HighlightIsHiddenOnSnapToActivated) {
  test_api().SetShouldHighlight(true);

  ink_drop()->SnapToActivated();
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       HighlightDoesntFadeInAfterAnimationIfHighlightNotSet) {
  test_api().SetShouldHighlight(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api().CompleteAnimations();
  test_api().SetShouldHighlight(false);
  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       HighlightFadesInAfterAnimationIfHovered) {
  ink_drop()->SetShowHighlightOnHover(true);
  ink_drop()->SetHovered(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api().CompleteAnimations();
  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();

  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
  task_environment()->FastForwardBy(base::Seconds(1));
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       HighlightSnapsInAfterAnimationWhenHostIsFocused) {
  ink_drop()->SetShowHighlightOnFocus(true);
  ink_drop()->SetFocused(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_TRIGGERED);
  test_api().CompleteAnimations();
  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();

  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplHideAutoHighlightTest, DeactivatedAnimatesWhenNotFocused) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  test_api().SetShouldHighlight(false);

  ink_drop()->AnimateToState(InkDropState::ACTIVATED);
  test_api().CompleteAnimations();

  ink_drop()->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
  EXPECT_TRUE(test_api().HasActiveAnimations());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       DeactivatedAnimationSkippedWhenFocused) {
  ink_drop()->SetShowHighlightOnFocus(true);
  ink_drop()->SetFocused(true);

  ink_drop()->AnimateToState(InkDropState::ACTIVATED);
  test_api().CompleteAnimations();

  ink_drop()->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_TRUE(AreLayersAddedToHost());

  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());
}

TEST_P(InkDropImplHideAutoHighlightTest,
       FocusAndHoverChangesDontShowHighlightWhenRippleIsVisible) {
  test_api().SetShouldHighlight(true);
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetHovered(false);
  ink_drop()->SetFocused(false);

  ink_drop()->SetHovered(true);
  ink_drop()->SetFocused(true);

  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
  EXPECT_TRUE(test_api().ShouldHighlight());
}

// Verifies there is no crash when animations are started during the destruction
// of the InkDropRipple. See https://crbug.com/663335.
TEST_P(InkDropImplHideAutoHighlightTest, NoCrashDuringRippleTearDown) {
  ink_drop()->SetShowHighlightOnFocus(true);
  ink_drop()->SetFocused(true);
  ink_drop()->AnimateToState(InkDropState::ACTIVATED);
  ink_drop()->AnimateToState(InkDropState::DEACTIVATED);
  ink_drop()->AnimateToState(InkDropState::DEACTIVATED);
  DestroyInkDrop();
}

////////////////////////////////////////////////////////////////////////////////
//
// InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE specific tests
//

using InkDropImplShowAutoHighlightTest = InkDropImplAutoHighlightTest;
// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(
    All,
    InkDropImplShowAutoHighlightTest,
    testing::Values(InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE));

TEST_P(InkDropImplShowAutoHighlightTest,
       VisibleHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(true);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplShowAutoHighlightTest,
       HiddenHighlightDuringRippleAnimations) {
  test_api().SetShouldHighlight(false);

  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->AnimateToState(InkDropState::HIDDEN);
  test_api().CompleteAnimations();
  EXPECT_FALSE(test_api().IsHighlightFadingInOrVisible());
}

TEST_P(InkDropImplShowAutoHighlightTest,
       FocusAndHoverChangesDontHideHighlightWhenRippleIsVisible) {
  test_api().SetShouldHighlight(true);
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  test_api().CompleteAnimations();
  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());

  ink_drop()->SetHovered(false);
  ink_drop()->SetFocused(false);

  EXPECT_TRUE(test_api().IsHighlightFadingInOrVisible());
  EXPECT_FALSE(test_api().ShouldHighlight());
}

}  // namespace views
