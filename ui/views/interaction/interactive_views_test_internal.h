// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_
#define UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_

#include <memory>
#include <string>

#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/views/interaction/interaction_test_util_mouse.h"

namespace views::test {

class InteractiveViewsTestApi;

namespace internal {

// Provides functionality required by InteractiveViewsTestApi but which needs to
// be hidden from tests inheriting from the API class.
class InteractiveViewsTestPrivate
    : public ui::test::internal::InteractiveTestPrivate {
 public:
  explicit InteractiveViewsTestPrivate(
      std::unique_ptr<ui::test::InteractionTestUtil> test_util);
  ~InteractiveViewsTestPrivate() override;

  // base::test::internal::InteractiveTestPrivate:
  void DoTestTearDown() override;
  void OnSequenceComplete() override;
  void OnSequenceAborted(
      int active_step,
      ui::TrackedElement* last_element,
      ui::ElementIdentifier last_id,
      ui::InteractionSequence::StepType last_step_type,
      ui::InteractionSequence::AbortedReason aborted_reason) override;

  InteractionTestUtilMouse& mouse_util() { return *mouse_util_; }

 private:
  friend class views::test::InteractiveViewsTestApi;

  // Provides mouse input simulation.
  std::unique_ptr<InteractionTestUtilMouse> mouse_util_;

  // Tracks failures when a mouse operation fails.
  std::string mouse_error_message_;
};

}  // namespace internal

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTIVE_VIEWS_TEST_INTERNAL_H_
