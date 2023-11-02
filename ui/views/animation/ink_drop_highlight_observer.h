// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_OBSERVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_OBSERVER_H_

#include "ui/views/animation/ink_drop_animation_ended_reason.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/views_export.h"

namespace views {

// Observer to attach to an InkDropHighlight animation.
class VIEWS_EXPORT InkDropHighlightObserver {
 public:
  InkDropHighlightObserver(const InkDropHighlightObserver&) = delete;
  InkDropHighlightObserver& operator=(const InkDropHighlightObserver&) = delete;

  // An animation for the given |animation_type| has started.
  virtual void AnimationStarted(
      InkDropHighlight::AnimationType animation_type) = 0;

  // Notifies the observer that an animation for the given |animation_type| has
  // finished and the reason for completion is given by |reason|. If |reason| is
  // SUCCESS then the animation has progressed to its final frame however if
  // |reason| is |PRE_EMPTED| then the animation was stopped before its final
  // frame.
  virtual void AnimationEnded(InkDropHighlight::AnimationType animation_type,
                              InkDropAnimationEndedReason reason) = 0;

 protected:
  InkDropHighlightObserver() = default;
  virtual ~InkDropHighlightObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_OBSERVER_H_
