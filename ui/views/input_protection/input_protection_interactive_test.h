// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_INTERACTIVE_TEST_H_
#define UI_VIEWS_INPUT_PROTECTION_INPUT_PROTECTION_INTERACTIVE_TEST_H_

#include <concepts>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/input_protection/input_protection_specification.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/test/views_test_base.h"

namespace views {
class InputProtectionPolicy;
class Widget;
}

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

  using PolicyFactory =
      base::RepeatingCallback<std::unique_ptr<InputProtectionPolicy>(Widget*)>;

  // Factory helper that creates a `PolicyFactory` for any
  // `InputProtectionPolicy` type, forwarding any constructor arguments and
  // passing the target `Widget*` if accepted by the constructor.
  template <typename Policy, typename... Args>
  static PolicyFactory MakePolicy(Args&&... args) {
    return base::BindRepeating(
        [](std::decay_t<Args>... bound_args,
           Widget* widget) -> std::unique_ptr<InputProtectionPolicy> {
          if constexpr (std::is_constructible_v<Policy, Widget*,
                                                std::decay_t<Args>...>) {
            return std::make_unique<Policy>(widget, bound_args...);
          } else {
            return std::make_unique<Policy>(bound_args...);
          }
        },
        std::forward<Args>(args)...);
  }

  // Enables input protection on the widget containing `element_id`. If
  // `element_id` is omitted, input protection is enabled on `context_widget()`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  EnableInputEventActivationProtection(ui::ElementIdentifier element_id = {});

  // Hides and shows the widget containing `element_id` to trigger the initial
  // show cooldown via visibility change. Also activates the widget so it is
  // ready to receive input.
  [[nodiscard]] MultiStep TriggerShowCooldown(ui::ElementIdentifier element_id);

  // Dispatches a simulated left mouse press to `element_id` at `click_point`
  // (or the center point if `click_point` is omitted).
  [[nodiscard]] MultiStep MousePress(
      ui::ElementIdentifier element_id,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Dispatches a simulated left mouse release to `element_id` at `click_point`
  // (or the center point if `click_point` is omitted).
  [[nodiscard]] MultiStep MouseRelease(
      ui::ElementIdentifier element_id,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Dispatches a simulated left mouse click (press and release) to `element_id`
  // at `click_point` (or the center point if `click_point` is omitted).
  [[nodiscard]] MultiStep Click(
      ui::ElementIdentifier element_id,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Clicks `element_id` and verifies the click was blocked by input protection.
  [[nodiscard]] ui::InteractionSequence::StepBuilder ClickExpectingBlocked(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Clicks `element_id` and verifies the click was processed.
  [[nodiscard]] ui::InteractionSequence::StepBuilder ClickExpectingAllowed(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      std::optional<gfx::Point> click_point = std::nullopt);

  // Dispatches a simulated key press to the widget containing `element_id`.
  [[nodiscard]] MultiStep KeyPress(ui::ElementIdentifier element_id,
                                   ui::KeyboardCode key,
                                   int flags = ui::EF_NONE);

  // Dispatches a simulated key release to the widget containing `element_id`.
  [[nodiscard]] MultiStep KeyRelease(ui::ElementIdentifier element_id,
                                     ui::KeyboardCode key,
                                     int flags = ui::EF_NONE);

  // Dispatches a keyboard event (press and release) to the widget containing
  // `element_id` with optional event flags (e.g. ui::EF_SHIFT_DOWN).
  [[nodiscard]] MultiStep KeyPressAndRelease(ui::ElementIdentifier element_id,
                                             ui::KeyboardCode key,
                                             int flags = ui::EF_NONE);

  // Dispatches an action key (press and release) and verifies it was blocked
  // by input protection.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  KeyPressAndReleaseExpectingBlocked(ui::ElementIdentifier element_id,
                                     ui::KeyboardCode key,
                                     const int& action_counter,
                                     int flags = ui::EF_NONE);

  // Dispatches an action key (press and release) and verifies it was
  // processed.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  KeyPressAndReleaseExpectingAllowed(ui::ElementIdentifier element_id,
                                     ui::KeyboardCode key,
                                     const int& action_counter,
                                     int flags = ui::EF_NONE);

  // Dispatches a simulated gesture tap to `element_id` at `tap_point` (or the
  // center point if `tap_point` is omitted).
  [[nodiscard]] ui::InteractionSequence::StepBuilder TouchTap(
      ui::ElementIdentifier element_id,
      std::optional<gfx::Point> tap_point = std::nullopt);

  // Dispatches a gesture tap and verifies the tap was blocked by input
  // protection.
  [[nodiscard]] ui::InteractionSequence::StepBuilder TouchTapExpectingBlocked(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      std::optional<gfx::Point> tap_point = std::nullopt);

  // Dispatches a gesture tap and verifies the tap was processed.
  [[nodiscard]] ui::InteractionSequence::StepBuilder TouchTapExpectingAllowed(
      ui::ElementIdentifier element_id,
      const int& action_counter,
      std::optional<gfx::Point> tap_point = std::nullopt);

  // Creates and shows an Always-On-Top floating window completely occluding the
  // element with `element_id`. If `button_id` is provided, the window hosts a
  // button tagged with `button_id` that runs `on_button_clicked` when clicked.
  // `button_id` also serves as an identifier to address this specific AOT
  // window (for example, passing `button_id` to `HideAotWindow(button_id)`).
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  OccludeElementWithAotWindow(ui::ElementIdentifier element_id,
                              ui::ElementIdentifier button_id = {},
                              base::RepeatingClosure on_button_clicked = {});

  // Creates and shows an Always-On-Top floating window occluding `local_bounds`
  // relative to the element with `element_id`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder OccludeRectWithAotWindow(
      ui::ElementIdentifier element_id,
      const gfx::Rect& local_bounds);

  // Hides all tracked Always-On-Top windows.
  [[nodiscard]] ui::InteractionSequence::StepBuilder HideAotWindows();

  // Hides the Always-On-Top window containing `element_id`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder HideAotWindow(
      ui::ElementIdentifier element_id);

  // Moves all tracked Always-On-Top windows away from `element_id` so that
  // `element_id` is no longer occluded.
  [[nodiscard]] ui::InteractionSequence::StepBuilder MoveAotWindowsToUnocclude(
      ui::ElementIdentifier element_id);

  // Creates and shows an Always-On-Top floating window completely occluding
  // `element_id` and immediately hides it to simulate a pop-away attack.
  [[nodiscard]] MultiStep TriggerAotPopAwayAttack(
      ui::ElementIdentifier element_id);

  // Anchors a bubble dialog containing a button tagged with `button_id` to
  // `anchor_element_id` and shows it with input event activation protection
  // enabled.
  [[nodiscard]] ui::InteractionSequence::StepBuilder ShowBubbleDialog(
      ui::ElementIdentifier anchor_element_id,
      ui::ElementIdentifier button_id,
      base::RepeatingClosure on_button_clicked,
      std::vector<PolicyFactory> policy_factories = {});

  // Hides the widget window containing `element_id` and waits for it to hide.
  [[nodiscard]] MultiStep HideWindow(ui::ElementIdentifier element_id);

  // Shows the widget window previously hidden by `HideWindow` and waits for
  // it to show.
  [[nodiscard]] MultiStep ShowWindow(ui::ElementIdentifier element_id);

  // Shows an auxiliary window hosting a button tagged with `button_id`.
  // Useful for transferring surface activation.
  [[nodiscard]] ui::InteractionSequence::StepBuilder ShowAuxiliaryWindow(
      ui::ElementIdentifier button_id);

  // Installs custom protected bounds on the view identified by `element_id`.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  InstallInputProtectionSpecification(
      ui::ElementIdentifier element_id,
      InputProtectionSpecification::GetBoundsCallback<View> callback);

  // Advances mock clock halfway through the default protection interval.
  [[nodiscard]] ui::InteractionSequence::StepBuilder
  AdvanceHalfwayThroughInputProtectionInterval();

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

  std::vector<std::unique_ptr<views::Widget>> aot_widgets_;
  std::vector<std::unique_ptr<views::Widget>> auxiliary_widgets_;
  std::unique_ptr<views::BubbleDialogDelegate> bubble_delegate_;
  std::unique_ptr<views::Widget> bubble_widget_;
  std::map<ui::ElementIdentifier, raw_ptr<views::Widget>> hidden_widgets_;
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
    hidden_widgets_.clear();
    bubble_widget_.reset();
    bubble_delegate_.reset();
    aot_widgets_.clear();
    auxiliary_widgets_.clear();
    run_loop_timeout_.reset();
    private_test_impl().DoTestTearDown();
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
