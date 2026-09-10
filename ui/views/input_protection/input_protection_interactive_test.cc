// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_interactive_test.h"

#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace {

// Dispatches a simulated left mouse click (press followed by release) to
// `view`. Uses `ui::EventTimeForNow()` so the event timestamp is generated
// from the same mock clock that tracks input protection cooldowns.
//
// When `click_point` is specified, the click targets that local coordinate,
// otherwise the click defaults to the center point of the local bounds of
// `view`. Converts coordinates to root view space before dispatching to the
// widget.
void DispatchClickAtPointOrDefaultCenter(
    View* view,
    const std::optional<gfx::Point>& click_point) {
  gfx::Point point = click_point.value_or(view->GetLocalBounds().CenterPoint());
  View::ConvertPointToTarget(view, view->GetWidget()->GetRootView(), &point);

  ui::MouseEvent press(ui::EventType::kMousePressed, point, point,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&press);

  ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  view->GetWidget()->OnMouseEvent(&release);
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

InputProtectionTestApi::MultiStep InputProtectionTestApi::ClickExpectingBlocked(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    int expected_count,
    std::optional<gfx::Point> click_point) {
  auto steps =
      Steps(WithView(element_id,
                     [click_point](View* view) {
                       DispatchClickAtPointOrDefaultCenter(view, click_point);
                     }),
            CheckVariable(action_counter, expected_count));
  AddDescriptionPrefix(steps, "ClickExpectingBlocked()");
  return steps;
}

InputProtectionTestApi::MultiStep InputProtectionTestApi::ClickExpectingAllowed(
    ui::ElementIdentifier element_id,
    const int& action_counter,
    int expected_count,
    std::optional<gfx::Point> click_point) {
  auto steps =
      Steps(WithView(element_id,
                     [click_point](View* view) {
                       DispatchClickAtPointOrDefaultCenter(view, click_point);
                     }),
            CheckVariable(action_counter, expected_count));
  AddDescriptionPrefix(steps, "ClickExpectingAllowed()");
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
