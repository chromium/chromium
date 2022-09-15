// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_OBSERVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_OBSERVER_H_

#include "ui/views/animation/ink_drop_animation_ended_reason.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace views {

// Observer to attach to an InkDropRipple.
class VIEWS_EXPORT InkDropRippleObserver {
 public:
  InkDropRippleObserver(const InkDropRippleObserver&) = delete;
  InkDropRippleObserver& operator=(const InkDropRippleObserver&) = delete;

  // An animation for the given |ink_drop_state| has started.
  virtual void AnimationStarted(InkDropState ink_drop_state) = 0;

  // Notifies the observer that an animation for the given |ink_drop_state| has
  // finished and the reason for completion is given by |reason|. If |reason| is
  // SUCCESS then the animation has progressed to its final frame however if
  // |reason| is |PRE_EMPTED| then the animation was stopped before its final
  // frame.
  virtual void AnimationEnded(InkDropState ink_drop_state,
                              InkDropAnimationEndedReason reason) = 0;

 protected:
  InkDropRippleObserver() = default;
  virtual ~InkDropRippleObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_OBSERVER_H_
