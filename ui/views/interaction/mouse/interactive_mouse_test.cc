// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/mouse/interactive_mouse_test.h"

#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/mouse/interactive_mouse_test_internal.h"

namespace views::test {

using GestureParams = InteractionTestUtilMouse::GestureParams;

using ui::test::internal::kInteractiveTestPivotElementId;

InteractiveMouseTestApi::InteractiveMouseTestApi()
    : test_impl_(private_test_impl()
                     .MaybeRegisterFrameworkImpl<
                         internal::InteractiveMouseTestPrivate>()) {}

InteractiveMouseTestApi::~InteractiveMouseTestApi() = default;

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::MoveMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("MoveMouseTo()");
  step.SetElement(reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveMouseTestApi* test, RelativePositionCallback pos_callback,
         ui::InteractionSequence* seq, ui::TrackedElement* el) {
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl_->GetGestureParamsForStep(el, seq),
                InteractionTestUtilMouse::MoveTo(
                    std::move(pos_callback).Run(el)))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), GetPositionCallback(std::move(position))));
  return step;
}

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::MoveMouseTo(
    AbsolutePositionSpecifier position) {
  return MoveMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)));
}

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::ClickMouse(
    ui_controls::MouseButton button,
    bool release,
    int modifier_keys) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("ClickMouse()");
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveMouseTestApi* test, ui_controls::MouseButton button,
         bool release, int modifier_keys, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl_->GetGestureParamsForStep(el, seq),
                release ? InteractionTestUtilMouse::Click(button, modifier_keys)
                        : InteractionTestUtilMouse::MouseGestures{
                              InteractionTestUtilMouse::MouseDown(
                                  button, modifier_keys)})) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), button, release, modifier_keys));
  step.SetMustRemainVisible(false);
  return step;
}

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::DragMouseTo(
    ElementSpecifier reference,
    RelativePositionSpecifier position,
    bool release) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("DragMouseTo()");
  step.SetElement(reference);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveMouseTestApi* test, RelativePositionCallback pos_callback,
         bool release, ui::InteractionSequence* seq, ui::TrackedElement* el) {
        const gfx::Point target = std::move(pos_callback).Run(el);
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl_->GetGestureParamsForStep(el, seq),
                release ? InteractionTestUtilMouse::DragAndRelease(target)
                        : InteractionTestUtilMouse::DragAndHold(target))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), GetPositionCallback(std::move(position)),
      release));
  return step;
}

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::DragMouseTo(
    AbsolutePositionSpecifier position,
    bool release) {
  return DragMouseTo(kInteractiveTestPivotElementId,
                     GetPositionCallback(std::move(position)), release);
}

InteractiveMouseTestApi::StepBuilder InteractiveMouseTestApi::ReleaseMouse(
    ui_controls::MouseButton button,
    int modifier_keys) {
  RequireInteractiveTest();
  StepBuilder step;
  step.SetDescription("ReleaseMouse()");
  step.SetElementID(kInteractiveTestPivotElementId);
  step.SetStartCallback(base::BindOnce(
      [](InteractiveMouseTestApi* test, ui_controls::MouseButton button,
         int modifier_keys, ui::InteractionSequence* seq,
         ui::TrackedElement* el) {
        const auto weak_seq = seq->AsWeakPtr();
        if (!test->mouse_util().PerformGestures(
                test->test_impl_->GetGestureParamsForStep(el, seq),
                InteractionTestUtilMouse::MouseUp(button, modifier_keys))) {
          if (weak_seq) {
            weak_seq->FailForTesting();
          }
        }
      },
      base::Unretained(this), button, modifier_keys));
  step.SetMustRemainVisible(false);
  return step;
}

// static
InteractiveMouseTestApi::RelativePositionCallback
InteractiveMouseTestApi::GetPositionCallback(AbsolutePositionSpecifier spec) {
  return std::visit(
      absl::Overload{
          [](const gfx::Point& point) {
            return base::BindOnce(
                [](gfx::Point p, ui::TrackedElement*) { return p; }, point);
          },
          [](std::reference_wrapper<gfx::Point> point) {
            return base::BindOnce([](std::reference_wrapper<gfx::Point> p,
                                     ui::TrackedElement*) { return p.get(); },
                                  point);
          },
          [](AbsolutePositionCallback& callback) {
            return base::RectifyCallback<RelativePositionCallback>(
                std::move(callback));
          }},
      spec);
}

// static
InteractiveMouseTestApi::RelativePositionCallback
InteractiveMouseTestApi::GetPositionCallback(RelativePositionSpecifier spec) {
  return std::visit(
      absl::Overload{[](RelativePositionCallback& callback) {
                       return std::move(callback);
                     },
                     [](CenterPoint) {
                       return base::BindOnce([](ui::TrackedElement* el) {
                         return el->GetScreenBounds().CenterPoint();
                       });
                     }},
      spec);
}

}  // namespace views::test
