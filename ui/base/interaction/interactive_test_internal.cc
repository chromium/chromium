// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interactive_test_internal.h"

#include "base/check.h"
#include "base/strings/string_piece_forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui::test::internal {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kInteractiveTestPivotElementId);

InteractiveTestPrivate::InteractiveTestPrivate(
    std::unique_ptr<InteractionTestUtil> test_util)
    : test_util_(std::move(test_util)) {}
InteractiveTestPrivate::~InteractiveTestPrivate() = default;

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
    InteractionSequence::AbortedReason aborted_reason) {
  if (aborted_callback_for_testing_) {
    std::move(aborted_callback_for_testing_)
        .Run(active_step, last_element, last_id, last_step_type,
             aborted_reason);
    return;
  }
  GTEST_FAIL() << "Interactive test failed on step " << active_step
               << " for reason " << aborted_reason << ". Step type was "
               << last_step_type << " with element " << last_id;
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

}  // namespace ui::test::internal
