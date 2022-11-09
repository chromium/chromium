// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"

namespace ui::test {

namespace {
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kEnsureNotPresentCheckEvent);
}

using internal::kInteractiveTestPivotElementId;

InteractiveTestApi::InteractiveTestApi(
    std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl)
    : private_test_impl_(std::move(private_test_impl)) {}
InteractiveTestApi::~InteractiveTestApi() = default;

InteractionSequence::StepBuilder InteractiveTestApi::PressButton(
    ElementSpecifier button,
    InputType input_type) {
  StepBuilder builder;
  internal::SpecifyElement(builder, button);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test, InteractionSequence*,
         TrackedElement* el) { test->test_util().PressButton(el, input_type); },
      input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectMenuItem(
    ElementSpecifier menu_item,
    InputType input_type) {
  StepBuilder builder;
  internal::SpecifyElement(builder, menu_item);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test, InteractionSequence*,
         TrackedElement* el) {
        test->test_util().SelectMenuItem(el, input_type);
      },
      input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::DoDefaultAction(
    ElementSpecifier element,
    InputType input_type) {
  StepBuilder builder;
  internal::SpecifyElement(builder, element);
  builder.SetMustRemainVisible(false);
  builder.SetStartCallback(base::BindOnce(
      [](InputType input_type, InteractiveTestApi* test, InteractionSequence*,
         TrackedElement* el) {
        test->test_util().DoDefaultAction(el, input_type);
      },
      input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectTab(
    ElementSpecifier tab_collection,
    size_t tab_index,
    InputType input_type) {
  StepBuilder builder;
  internal::SpecifyElement(builder, tab_collection);
  builder.SetStartCallback(base::BindOnce(
      [](size_t index, InputType input_type, InteractiveTestApi* test,
         InteractionSequence*, TrackedElement* el) {
        test->test_util().SelectTab(el, index, input_type);
      },
      tab_index, input_type, base::Unretained(this)));
  return builder;
}

InteractionSequence::StepBuilder InteractiveTestApi::SelectDropdownItem(
    ElementSpecifier collection,
    size_t item,
    InputType input_type) {
  StepBuilder builder;
  internal::SpecifyElement(builder, collection);
  builder.SetStartCallback(base::BindOnce(
      [](size_t item, InputType input_type, InteractiveTestApi* test,
         InteractionSequence*, TrackedElement* el) {
        test->test_util().SelectDropdownItem(el, item, input_type);
      },
      item, input_type, base::Unretained(this)));
  return builder;
}

InteractiveTestApi::StepBuilder InteractiveTestApi::Check(
    CheckCallback check_callback) {
  StepBuilder builder;
  builder.SetElementID(kInteractiveTestPivotElementId);
  builder.SetStartCallback(base::BindOnce(
      [](CheckCallback check_callback, InteractionSequence* seq,
         TrackedElement*) {
        const bool result = std::move(check_callback).Run();
        if (!result)
          seq->FailForTesting();
      },
      std::move(check_callback)));
  return builder;
}

// static
InteractiveTestApi::StepBuilder InteractiveTestApi::Do(
    base::OnceClosure action) {
  StepBuilder builder;
  builder.SetElementID(kInteractiveTestPivotElementId);
  builder.SetStartCallback(std::move(action));
  return builder;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    base::OnceCallback<bool(TrackedElement* el)> check) {
  return CheckElement(element, std::move(check), true);
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForShow(
    ElementSpecifier element,
    bool transition_only_on_event) {
  StepBuilder step;
  internal::SpecifyElement(step, element);
  step.SetTransitionOnlyOnEvent(transition_only_on_event);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForHide(
    ElementSpecifier element,
    bool transition_only_on_event) {
  StepBuilder step;
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kHidden);
  step.SetTransitionOnlyOnEvent(transition_only_on_event);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForActivate(
    ElementSpecifier element) {
  StepBuilder step;
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kActivated);
  return step;
}

// static
InteractionSequence::StepBuilder InteractiveTestApi::WaitForEvent(
    ElementSpecifier element,
    CustomElementEventType event) {
  StepBuilder step;
  internal::SpecifyElement(step, element);
  step.SetType(InteractionSequence::StepType::kCustomEvent, event);
  return step;
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::EnsureNotPresent(
    ElementIdentifier element_to_check,
    bool in_any_context) {
  MultiStep steps;
  steps.emplace_back(WithElement(
      kInteractiveTestPivotElementId,
      base::BindOnce([](TrackedElement* element) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](ElementIdentifier id, ElementContext context) {
                  auto* const element =
                      ElementTracker::GetElementTracker()
                          ->GetFirstMatchingElement(id, context);
                  if (element) {
                    ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                        element, kEnsureNotPresentCheckEvent);
                  }
                  // Note: if the element is no longer present, the sequence was
                  // already aborted; there is no need to send further errors.
                },
                element->identifier(), element->context()));
      })));
  steps.emplace_back(AfterEvent(
      kInteractiveTestPivotElementId, kEnsureNotPresentCheckEvent,
      base::BindOnce(
          [](ElementIdentifier element_to_check, bool in_any_context,
             InteractionSequence* seq, TrackedElement* reference) {
            auto* const element =
                in_any_context
                    ? ElementTracker::GetElementTracker()
                          ->GetElementInAnyContext(element_to_check)
                    : ElementTracker::GetElementTracker()
                          ->GetFirstMatchingElement(element_to_check,
                                                    reference->context());
            if (element) {
              LOG(ERROR) << "Expected element " << element_to_check
                         << " not to be present but it was present.";
              seq->FailForTesting();
            }
          },
          element_to_check, in_any_context)));
  return steps;
}

// static
InteractiveTestApi::MultiStep InteractiveTestApi::InAnyContext(
    MultiStep steps) {
  for (auto& step : steps)
    step.SetFindElementInAnyContext(true);
  return steps;
}

bool InteractiveTestApi::RunTestSequenceImpl(
    ElementContext context,
    InteractionSequence::Builder builder) {
  builder.SetContext(context);

  // Pivot element also serves as a re-entrancy guard.
  CHECK(!private_test_impl_->pivot_element_);
  auto pivot_element =
      std::make_unique<TestElement>(kInteractiveTestPivotElementId, context);
  pivot_element->Show();
  base::AutoReset<std::unique_ptr<TrackedElement>> pivot_element_reset(
      &private_test_impl_->pivot_element_, std::move(pivot_element));

  private_test_impl_->success_ = false;
  builder.SetCompletedCallback(
      base::BindOnce(&internal::InteractiveTestPrivate::OnSequenceComplete,
                     base::Unretained(private_test_impl_.get())));
  builder.SetAbortedCallback(
      base::BindOnce(&internal::InteractiveTestPrivate::OnSequenceAborted,
                     base::Unretained(private_test_impl_.get())));
  auto sequence = builder.Build();
  sequence->RunSynchronouslyForTesting();
  return private_test_impl_->success_;
}

// static
void InteractiveTestApi::AddStep(InteractionSequence::Builder& builder,
                                 MultiStep multi_step) {
  for (auto& step : multi_step)
    builder.AddStep(step);
}

// static
void InteractiveTestApi::AddStep(MultiStep& dest, StepBuilder src) {
  dest.emplace_back(std::move(src));
}

// static
void InteractiveTestApi::AddStep(MultiStep& dest, MultiStep src) {
  for (auto& step : src)
    dest.emplace_back(std::move(step));
}

InteractiveTest::InteractiveTest()
    : InteractiveTestApi(std::make_unique<internal::InteractiveTestPrivate>(
          std::make_unique<InteractionTestUtil>())) {}

InteractiveTest::~InteractiveTest() = default;

void InteractiveTest::SetUp() {
  Test::SetUp();
  private_test_impl().DoTestSetUp();
}

void InteractiveTest::TearDown() {
  private_test_impl().DoTestTearDown();
  Test::TearDown();
}

}  // namespace ui::test
