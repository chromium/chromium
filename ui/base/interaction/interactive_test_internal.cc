// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test_internal.h"

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace ui::test::internal {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kInteractiveTestPivotEventType);

InteractiveTestPrivate::InteractiveTestPrivate(
    std::unique_ptr<InteractionTestUtil> test_util)
    : test_util_(std::move(test_util)) {}
InteractiveTestPrivate::~InteractiveTestPrivate() = default;

void InteractiveTestPrivate::Init(ElementContext initial_context) {
  success_ = false;
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

TrackedElement* InteractiveTestPrivate::GetPivotElement(
    ElementContext context) const {
  const auto it = pivot_elements_.find(context);
  CHECK(it != pivot_elements_.end())
      << "Tried to reference non-existent context.";
  return it->second.get();
}

void InteractiveTestPrivate::DoTestSetUp() {}
void InteractiveTestPrivate::DoTestTearDown() {}

void InteractiveTestPrivate::OnSequenceComplete() {
  success_ = true;
}

void InteractiveTestPrivate::OnSequenceAborted(
    int active_step,
    TrackedElement* last_element,
    ElementIdentifier last_id,
    InteractionSequence::StepType last_step_type,
    InteractionSequence::AbortedReason aborted_reason,
    std::string description) {
  if (aborted_callback_for_testing_) {
    std::move(aborted_callback_for_testing_)
        .Run(active_step, last_element, last_id, last_step_type, aborted_reason,
             description);
    return;
  }
  GTEST_FAIL() << "Interactive test failed on step " << active_step
               << " for reason " << aborted_reason << ". Step type was "
               << last_step_type << " with element " << last_id
               << " description: " << description;
}

void SpecifyElement(ui::InteractionSequence::StepBuilder& builder,
                    ElementSpecifier element) {
  if (auto* id = absl::get_if<ElementIdentifier>(&element)) {
    builder.SetElementID(*id);
  } else {
    CHECK(absl::holds_alternative<base::StringPiece>(element));
    builder.SetElementName(absl::get<base::StringPiece>(element));
  }
}

std::string DescribeElement(ElementSpecifier element) {
  if (auto* id = absl::get_if<ElementIdentifier>(&element)) {
    return id->GetName();
  }
  CHECK(absl::holds_alternative<base::StringPiece>(element));
  return base::StringPrintf("\"%s\"",
                            absl::get<base::StringPiece>(element).data());
}

}  // namespace ui::test::internal
