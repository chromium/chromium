// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_INTERACTIVE_TEST_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_INTERACTIVE_TEST_H_

#include <concepts>
#include <optional>
#include <utility>

#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/views_test_base.h"

namespace views::test {

// Provides interactive test functionality for Views input protection.
//
// This class is not a test fixture; it is an API that can be added to existing
// test classes using `InputProtectionInteractiveTestMixin<T>`.
class InputProtectionTestApi
    : virtual public views::test::InteractiveViewsTestApi {
 public:
  InputProtectionTestApi();
  ~InputProtectionTestApi() override;

  // Enables input protection on the widget containing `element_id`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  EnableInputEventActivationProtection(ui::ElementIdentifier element_id);

  // Hides and shows the widget containing `element_id` to trigger the initial
  // show cooldown via visibility change. Also activates the widget so it is
  // ready to receive input.
  [[nodiscard]] MultiStep TriggerShowCooldown(ui::ElementIdentifier element_id);

  // Clicks `element_id` at `click_point` (or the center point if `click_point`
  // is omitted) and verifies the click was blocked by input protection.
  [[nodiscard]] MultiStep ClickExpectingBlocked(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      int expected_count = 0,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Clicks `element_id` at `click_point` (or the center point if `click_point`
  // is omitted) and verifies the click was processed.
  [[nodiscard]] MultiStep ClickExpectingAllowed(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      int expected_count = 1,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Advances mock clock by the specified `delta`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder AdvanceClockBy(
      base::TimeDelta delta);

  // Advances mock clock past the default protection interval.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  AdvancePastInputProtectionInterval();

 protected:
  // Fast-forwards mock time by `delta`. Subclasses or mixins (such as
  // `InputProtectionInteractiveTestMixin`) must override this method.
  virtual void FastForwardMockClock(base::TimeDelta delta);
};

// Template for adding `InputProtectionTestApi` to any test fixture which is
// derived from `ViewsTestBase`.
template <typename T>
  requires std::derived_from<T, ViewsTestBase>
class InputProtectionInteractiveTestMixin : public T,
                                            public InputProtectionTestApi {
 public:
  template <typename... Args>
  explicit InputProtectionInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InputProtectionInteractiveTestMixin() override = default;

 protected:
  void SetUp() override {
    T::SetUp();
    if (!base::test::ScopedRunLoopTimeout::ExistsForCurrentThread()) {
      run_loop_timeout_.emplace(FROM_HERE, TestTimeouts::action_timeout());
    }
    private_test_impl().DoTestSetUp();
  }

  void TearDown() override {
    private_test_impl().DoTestTearDown();
    run_loop_timeout_.reset();
    T::TearDown();
  }

  // Fast-forwards mock time on `task_environment()` of `ViewsTestBase`.
  void FastForwardMockClock(base::TimeDelta delta) override {
    this->task_environment()->FastForwardBy(delta);
  }

 private:
  // Provides a baseline run loop timeout when running under mock time, where
  // `TaskEnvironment` omits installing a default timeout.
  std::optional<base::test::ScopedRunLoopTimeout> run_loop_timeout_;
};

// Convenience test fixture for input protection interactive tests. This is the
// preferred base class for tests unless you specifically need something else.
using InputProtectionInteractiveTest =
    InputProtectionInteractiveTestMixin<ViewsTestBase>;

}  // namespace views::test

#endif  // UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_INTERACTIVE_TEST_H_
