// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test_internal.h"

#include <memory>
#include <string_view>
#include <variant>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui::test::internal {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

const char kInteractiveTestFailedMessagePrefix[] = "Interactive test failed ";
const char kNoCheckDescriptionSpecified[] = "[no description specified]";

StateObserverElement::StateObserverElement(ElementIdentifier id,
                                           ElementContext context)
    : TestElementBase(id, context) {}

StateObserverElement::~StateObserverElement() = default;

DEFINE_FRAMEWORK_SPECIFIC_METADATA(StateObserverElement)

// static
bool InteractiveTestPrivate::allow_interactive_test_verbs_ = false;

InteractiveTestPrivate::InteractiveTestPrivate(
    std::unique_ptr<InteractionTestUtil> test_util)
    : test_util_(std::move(test_util)) {}
InteractiveTestPrivate::~InteractiveTestPrivate() = default;

void InteractiveTestPrivate::Init(ElementContext initial_context) {
  success_ = false;
  sequence_skipped_ = false;
  MaybeAddPivotElement(initial_context);
  for (ElementContext context :
       ElementTracker::GetElementTracker()->GetAllContextsForTesting()) {
    MaybeAddPivotElement(context);
  }
  context_subscription_ =
      ElementTracker::GetElementTracker()->AddAnyElementShownCallbackForTesting(
          base::BindRepeating(&InteractiveTestPrivate::OnElementAdded,
                              base::Unretained(this)));
}

void InteractiveTestPrivate::Cleanup() {
  context_subscription_ = base::CallbackListSubscription();
  pivot_elements_.clear();
}

void InteractiveTestPrivate::OnElementAdded(TrackedElement* el) {
  if (el->identifier() == kInteractiveTestPivotElementId)
    return;
  MaybeAddPivotElement(el->context());
}

void InteractiveTestPrivate::MaybeAddPivotElement(ElementContext context) {
  if (!base::Contains(pivot_elements_, context)) {
    auto pivot =
        std::make_unique<TestElement>(kInteractiveTestPivotElementId, context);
    auto* const el = pivot.get();
    pivot_elements_.emplace(context, std::move(pivot));
    el->Show();
  }
}

void InteractiveTestPrivate::HandleActionResult(
    InteractionSequence* seq,
    const TrackedElement* el,
    const std::string& operation_name,
    ActionResult result) {
  switch (result) {
    case ActionResult::kSucceeded:
      break;
    case ActionResult::kFailed:
      LOG(ERROR) << operation_name << " failed for " << *el;
      seq->FailForTesting();
      break;
    case ActionResult::kNotAttempted:
      LOG(ERROR) << operation_name << " could not be applied to " << *el;
      seq->FailForTesting();
      break;
    case ActionResult::kKnownIncompatible:
      LOG(WARNING) << operation_name
                   << " failed because it is unsupported on this platform for "
                   << *el;
      if (!on_incompatible_action_reason_.empty()) {
        LOG(WARNING) << "Unsupported action was expected: "
                     << on_incompatible_action_reason_;
      } else {
        LOG(ERROR) << "Unsupported action was unexpected. "
                      "Did you forget to call SetOnIncompatibleAction()?";
      }
      switch (on_incompatible_action_) {
        case OnIncompatibleAction::kFailTest:
          seq->FailForTesting();
          break;
        case OnIncompatibleAction::kSkipTest:
        case OnIncompatibleAction::kHaltTest:
          sequence_skipped_ = true;
          seq->FailForTesting();
          break;
        case OnIncompatibleAction::kIgnoreAndContinue:
          break;
      }
      break;
  }
}

TrackedElement* InteractiveTestPrivate::GetPivotElement(
    ElementContext context) const {
  const auto it = pivot_elements_.find(context);
  CHECK(it != pivot_elements_.end())
      << "Tried to reference non-existent context.";
  return it->second.get();
}

bool InteractiveTestPrivate::RemoveStateObserver(ElementIdentifier id,
                                                 ElementContext context) {
  using It = decltype(state_observer_elements_.begin());
  It found = state_observer_elements_.end();
  for (It it = state_observer_elements_.begin();
       it != state_observer_elements_.end(); ++it) {
    auto& entry = **it;
    if (entry.identifier() == id && (!context || entry.context() == context)) {
      CHECK(found == state_observer_elements_.end())
          << "RemoveStateObserver: Duplicate entries found for " << id;
      found = it;
    }
  }
  if (found == state_observer_elements_.end()) {
    LOG(ERROR) << "RemoveStateObserver: Entry not found for " << id;
    return false;
  }

  state_observer_elements_.erase(found);
  return true;
}

void InteractiveTestPrivate::DoTestSetUp() {}
void InteractiveTestPrivate::DoTestTearDown() {
  state_observer_elements_.clear();
}

void InteractiveTestPrivate::OnSequenceComplete() {
  success_ = true;
}

void InteractiveTestPrivate::OnSequenceAborted(
    const InteractionSequence::AbortedData& data) {
  if (aborted_callback_for_testing_) {
    std::move(aborted_callback_for_testing_).Run(data);
    return;
  }
  if (sequence_skipped_) {
    LOG(WARNING) << kInteractiveTestFailedMessagePrefix << data;
    if (on_incompatible_action_ == OnIncompatibleAction::kSkipTest) {
      GTEST_SKIP();
    } else {
      DCHECK_EQ(OnIncompatibleAction::kHaltTest, on_incompatible_action_);
    }
  } else {
    std::ostringstream additional_message;
    if (data.aborted_reason == InteractionSequence::AbortedReason::
                                   kElementHiddenBetweenTriggerAndStepStart) {
      additional_message
          << "\nNOTE: Please check for one of the following common mistakes:\n"
             " - A RunLoop whose type is not set to kNestableTasksAllowed. "
             "Change the type and try again.\n"
             " - A check being performed on an element that has been hidden. "
             "Wrap waiting for the hide and subsequent checks in a "
             "WithoutDelay() to avoid possible access-after-delete.";
    }
    GTEST_FAIL() << "Interactive test failed " << data
                 << additional_message.str();
  }
}

void SpecifyElement(ui::InteractionSequence::StepBuilder& builder,
                    ElementSpecifier element) {
  std::visit(
      base::Overloaded{
          [&builder](ElementIdentifier id) { builder.SetElementID(id); },
          [&builder](std::string_view name) { builder.SetElementName(name); }},
      element);
}

std::string DescribeElement(ElementSpecifier element) {
  return std::visit(
      base::Overloaded{[](ElementIdentifier id) { return id.GetName(); },
                       [](std::string_view name) {
                         return base::StringPrintf("\"%s\"", name.data());
                       }},
      element);
}

InteractionSequence::Builder BuildSubsequence(
    InteractiveTestPrivate::MultiStep steps) {
  InteractionSequence::Builder builder;
  for (auto& step : steps) {
    builder.AddStep(std::move(step));
  }
  return builder;
}

}  // namespace ui::test::internal
