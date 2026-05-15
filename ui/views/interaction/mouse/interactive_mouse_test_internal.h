// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_INTERNAL_H_
#define UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_INTERNAL_H_

#include <memory>

#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"

namespace views::test {

class InteractiveMouseTestApi;

namespace internal {

// Provides functionality required by InteractiveMouseTestApi but which needs to
// be hidden from tests inheriting from the API class.
class InteractiveMouseTestPrivate
    : public ui::test::internal::InteractiveTestPrivateFrameworkBase {
 public:
  DECLARE_SAFE_CAST_TARGET()

  explicit InteractiveMouseTestPrivate(
      ui::test::internal::InteractiveTestPrivate& test_impl);
  ~InteractiveMouseTestPrivate() override;

  // base::test::internal::InteractiveTestPrivateFrameworkBase:
  void OnSequenceComplete() override;
  void OnSequenceAborted(
      const ui::InteractionSequence::AbortedData& data) override;
  void OnDefaultContextSet() override;

  InteractionTestUtilMouse& mouse_util() { return *mouse_util_; }

  InteractionTestUtilMouse::GestureParams GetGestureParamsForStep(
      ui::TrackedElement* el,
      const ui::InteractionSequence* seq);

 private:
  friend class views::test::InteractiveMouseTestApi;

  // Provides mouse input simulation.
  std::unique_ptr<InteractionTestUtilMouse> mouse_util_;
};

}  // namespace internal

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_MOUSE_INTERACTIVE_MOUSE_TEST_INTERNAL_H_
