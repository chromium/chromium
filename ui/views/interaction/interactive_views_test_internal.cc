// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test_internal.h"

#include <memory>
#include <utility>

#include "ui/base/interaction/interaction_test_util.h"

namespace views::test::internal {

InteractiveViewsTestPrivate::InteractiveViewsTestPrivate(
    std::unique_ptr<ui::test::InteractionTestUtil> test_util)
    : InteractiveTestPrivate(std::move(test_util)) {}
InteractiveViewsTestPrivate::~InteractiveViewsTestPrivate() = default;

void InteractiveViewsTestPrivate::DoTestTearDown() {
  mouse_util_.reset();
  InteractiveTestPrivate::DoTestTearDown();
}

void InteractiveViewsTestPrivate::OnSequenceComplete() {
  mouse_util_->CancelAllGestures();
  InteractiveTestPrivate::OnSequenceComplete();
}

void InteractiveViewsTestPrivate::OnSequenceAborted(
    int active_step,
    ui::TrackedElement* last_element,
    ui::ElementIdentifier last_id,
    ui::InteractionSequence::StepType last_step_type,
    ui::InteractionSequence::AbortedReason aborted_reason) {
  mouse_util_->CancelAllGestures();
  InteractiveTestPrivate::OnSequenceAborted(active_step, last_element, last_id,
                                            last_step_type, aborted_reason);
}

}  // namespace views::test::internal
