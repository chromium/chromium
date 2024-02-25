// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_RIPPLE_OBSERVER_H_
#define UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_RIPPLE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/test_ink_drop_animation_observer_helper.h"

namespace views {
class InkDropRipple;

namespace test {

// Simple InkDropRippleObserver test double that tracks if InkDropRippleObserver
// methods are invoked and the parameters used for the last invocation.
class TestInkDropRippleObserver
    : public InkDropRippleObserver,
      public TestInkDropAnimationObserverHelper<InkDropState> {
 public:
  TestInkDropRippleObserver();

  TestInkDropRippleObserver(const TestInkDropRippleObserver&) = delete;
  TestInkDropRippleObserver& operator=(const TestInkDropRippleObserver&) =
      delete;

  ~TestInkDropRippleObserver() override;

  void set_ink_drop_ripple(InkDropRipple* ink_drop_ripple) {
    ink_drop_ripple_ = ink_drop_ripple;
  }

  InkDropState target_state_at_last_animation_started() const {
    return target_state_at_last_animation_started_;
  }

  InkDropState target_state_at_last_animation_ended() const {
    return target_state_at_last_animation_ended_;
  }

  // InkDropRippleObserver:
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // The type this inherits from. Reduces verbosity in .cc file.
  using ObserverHelper = TestInkDropAnimationObserverHelper<InkDropState>;

  // The value of InkDropRipple::GetTargetInkDropState() the last time an
  // AnimationStarted() event was handled. This is only valid if
  // |ink_drop_ripple_| is not null.
  InkDropState target_state_at_last_animation_started_ = InkDropState::HIDDEN;

  // The value of InkDropRipple::GetTargetInkDropState() the last time an
  // AnimationEnded() event was handled. This is only valid if
  // |ink_drop_ripple_| is not null.
  InkDropState target_state_at_last_animation_ended_ = InkDropState::HIDDEN;

  // An InkDropRipple to spy info from when notifications are handled.
  raw_ptr<InkDropRipple> ink_drop_ripple_ = nullptr;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_TEST_INK_DROP_RIPPLE_OBSERVER_H_
