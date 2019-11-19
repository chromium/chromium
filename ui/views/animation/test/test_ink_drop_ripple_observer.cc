// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_ripple_observer.h"

#include "ui/views/animation/ink_drop_ripple.h"

namespace views {
namespace test {

TestInkDropRippleObserver::TestInkDropRippleObserver() = default;

TestInkDropRippleObserver::~TestInkDropRippleObserver() = default;

void TestInkDropRippleObserver::AnimationStarted(InkDropState ink_drop_state) {
  ObserverHelper::OnAnimationStarted(ink_drop_state);
  if (ink_drop_ripple_) {
    target_state_at_last_animation_started_ =
        ink_drop_ripple_->target_ink_drop_state();
  }
}

void TestInkDropRippleObserver::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  ObserverHelper::OnAnimationEnded(ink_drop_state, reason);
  if (ink_drop_ripple_) {
    target_state_at_last_animation_ended_ =
        ink_drop_ripple_->target_ink_drop_state();
  }
}

}  // namespace test
}  // namespace views
