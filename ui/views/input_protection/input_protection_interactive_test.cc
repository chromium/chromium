// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_interactive_test.h"

#include <optional>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

// Returns `click_point` (or the center point of `view` if omitted) converted to
// the `RootView` coordinate space of the target widget.
gfx::Point GetPointInRootView(View* view,
                              const std::optional<gfx::Point>& click_point) {
  gfx::Point point = click_point.value_or(view->GetLocalBounds().CenterPoint());
  View::ConvertPointToTarget(view, view->GetWidget()->GetRootView(), &point);
  return point;
}

// Event dispatch helpers below use `ui::EventTimeForNow()` so the event
// timestamp is generated from the same mock clock that tracks input protection
// cooldowns.
void DispatchMousePressOnly(
    View* view,
    const std::optional<gfx::Point>& click_point = std::nullopt) {
  gfx::Point point = GetPointInRootView(view, click_point);
  ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&press);
}

void DispatchMouseReleaseOnly(
    View* view,
    const std::optional<gfx::Point>& click_point = std::nullopt) {
  gfx::Point point = GetPointInRootView(view, click_point);
  ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&release);
}

void DispatchClick(View* view, const std::optional<gfx::Point>& click_point) {
  DispatchMousePressOnly(view, click_point);
  DispatchMouseReleaseOnly(view, click_point);
}

void DispatchKeyPressOnly(View* view, ui::KeyboardCode key, int flags) {
  ui::KeyEvent press(ui::EventType::kKeyPressed, key, flags,
                     ui::EventTimeForNow());
  view->GetWidget()->OnKeyEvent(&press);
}

void DispatchKeyReleaseOnly(View* view, ui::KeyboardCode key, int flags) {
  ui::KeyEvent release(ui::EventType::kKeyReleased, key, flags,
                       ui::EventTimeForNow());
  view->GetWidget()->OnKeyEvent(&release);
}

void DispatchKeyPressAndRelease(View* view, ui::KeyboardCode key, int flags) {
  DispatchKeyPressOnly(view, key, flags);
  DispatchKeyReleaseOnly(view, key, flags);
}

}  // namespace

InputProtectionTestApi::InputProtectionTestApi() = default;
InputProtectionTestApi::~InputProtectionTestApi() = default;

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::EnableInputEventActivationProtection(
    ui::ElementIdentifier element_id) {
  auto step = WithView(element_id, [](View* view) {
    if (view->GetWidget()) {
      view->GetWidget()->EnableInputEventActivationProtection();
    }
  });
  step.SetDescription("EnableInputEventActivationProtection()");
  return step;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::TriggerShowCooldown(
    ui::ElementIdentifier element_id) {
  auto steps = Steps(WithView(element_id,
                              [](View* view) {
                                if (view->GetWidget()) {
                                  view->GetWidget()->Hide();
                                  view->GetWidget()->Show();
                                }
                              })
                         .SetMustRemainVisible(false),
                     WaitForShow(element_id), ActivateSurface(element_id));
  AddDescriptionPrefix(steps, "TriggerShowCooldown()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::MousePress(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchMousePressOnly(view, click_point);
  }));
  AddDescriptionPrefix(steps, "MousePress()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::MouseRelease(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchMouseReleaseOnly(view, click_point);
  }));
  AddDescriptionPrefix(steps, "MouseRelease()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::Click(
    ui::ElementIdentifier element_id,
    std::optional<gfx::Point> click_point) {
  auto steps = Steps(WithView(element_id, [click_point](View* view) {
    DispatchClick(view, click_point);
  }));
  AddDescriptionPrefix(steps, "Click()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::ClickExpectingBlocked(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    int expected_count,
    std::optional<gfx::Point> click_point) {
  auto steps =
      Steps(Click(element_id, click_point),
            CheckVariable(action_counter, expected_count, "action_counter"));
  AddDescriptionPrefix(steps, "ClickExpectingBlocked()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::ClickExpectingAllowed(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    int expected_count,
    std::optional<gfx::Point> click_point) {
  auto steps =
      Steps(Click(element_id, click_point),
            CheckVariable(action_counter, expected_count, "action_counter"));
  AddDescriptionPrefix(steps, "ClickExpectingAllowed()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyPress(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyPressOnly(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyPress()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyRelease(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyReleaseOnly(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyRelease()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::KeyPressAndRelease(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    int flags) {
  auto steps = Steps(WithView(element_id, [key, flags](View* view) {
    DispatchKeyPressAndRelease(view, key, flags);
  }));
  AddDescriptionPrefix(steps, "KeyPressAndRelease()");
  return steps;
}

InputProtectionTestApi::MultiStep
InputProtectionTestApi::KeyPressAndReleaseExpectingBlocked(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    const int& action_counter,
    int expected_count,
    int flags) {
  auto steps =
      Steps(KeyPressAndRelease(element_id, key, flags),
            CheckVariable(action_counter, expected_count, "action_counter"));
  AddDescriptionPrefix(steps, "KeyPressAndReleaseExpectingBlocked()");
  return steps;
}

InputProtectionTestApi::MultiStep
InputProtectionTestApi::KeyPressAndReleaseExpectingAllowed(
    ui::ElementIdentifier element_id,
    ui::KeyboardCode key,
    const int& action_counter,
    int expected_count,
    int flags) {
  auto steps =
      Steps(KeyPressAndRelease(element_id, key, flags),
            CheckVariable(action_counter, expected_count, "action_counter"));
  AddDescriptionPrefix(steps, "KeyPressAndReleaseExpectingAllowed()");
  return steps;
}

void InputProtectionTestApi::FastForwardMockClock(base::TimeDelta delta) {
  NOTREACHED()
      << "`FastForwardMockClock` must be overridden by a derived test fixture "
         "or mixin (such as `InputProtectionInteractiveTestMixin`) to advance "
         "mock time.";
}

ui::InteractionSequence::StepBuilder InputProtectionTestApi::AdvanceClockBy(
    base::TimeDelta delta) {
  auto step = Do([this, delta]() { FastForwardMockClock(delta); });
  step.SetDescription(
      base::StrCat({"AdvanceClockBy(",
                    base::NumberToString(delta.InMilliseconds()), " ms)"}));
  return step;
}

ui::InteractionSequence::StepBuilder
InputProtectionTestApi::AdvancePastInputProtectionInterval() {
  auto step =
      AdvanceClockBy(views::GetDoubleClickInterval() + base::Milliseconds(50));
  step.SetDescription("AdvancePastInputProtectionInterval()");
  return step;
}

}  // namespace views::test
