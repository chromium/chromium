// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_OBSERVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_OBSERVER_H_

#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace views {

// Observer to attach to an InkDrop.
class VIEWS_EXPORT InkDropObserver {
 public:
  InkDropObserver(const InkDropObserver&) = delete;
  InkDropObserver& operator=(const InkDropObserver&) = delete;

  // Called when the animation of the current InkDrop has started. This
  // includes the ripple or highlight animation. Note: this is not guaranteed to
  // be notified, as the notification is dependent on the subclass
  // implementation.
  virtual void InkDropAnimationStarted() = 0;

  // Called when the animation to the provided ink drop state has ended (both if
  // the animation ended successfully, and if the animation was aborted).
  // Includes ripple animation only.
  // NOTE: this is not guaranteed to be notified, as the notification is
  // dependent on the subclass implementation.
  // |ink_drop_state| - The state to which the ink drop ripple was animating.
  virtual void InkDropRippleAnimationEnded(InkDropState ink_drop_state) = 0;

 protected:
  InkDropObserver() = default;
  virtual ~InkDropObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_OBSERVER_H_
