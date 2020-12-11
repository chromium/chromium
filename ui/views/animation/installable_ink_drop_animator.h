// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace gfx {
class AnimationContainer;
}

namespace views {

// Manages animating the ink drop's visual state. This class is essentially a
// state machine, using the current and target InkDropStates to affect the
// InstallableInkDropPainter passed in. The animations are currently minimal.
class VIEWS_EXPORT InstallableInkDropAnimator : public gfx::AnimationDelegate {
 public:
  // Placeholder duration used for highlight animation. TODO(crbug.com/933384):
  // remove this and make highlight animation duration controllable, like in
  // InkDropHighlight.
  static constexpr base::TimeDelta kHighlightAnimationDuration =
      base::TimeDelta::FromMilliseconds(500);

  // We use a shared gfx::AnimationContainer for our animations to allow them to
  // update in sync. It is passed in at construction for two reasons: it allows
  // |views::CompositorAnimationRunner| to be used for more efficient and less
  // janky animations, and it enables easier unit testing.
  explicit InstallableInkDropAnimator(
      gfx::Size size,
      InstallableInkDropPainter::State* visual_state,
      gfx::AnimationContainer* animation_container,
      base::RepeatingClosure repaint_callback);
  ~InstallableInkDropAnimator() override;

  void SetSize(gfx::Size size);
  void SetLastEventLocation(gfx::Point last_event_location);

  // Set the target state and animate to it.
  void AnimateToState(InkDropState target_state);

  // Animates the hover highlight in or out. Animates in if |fade_in| is true,
  // and out otherwise.
  void AnimateHighlight(bool fade_in);

  InkDropState target_state() const { return target_state_; }

  // The sub-animations used when animating to an |InkDropState|. These are used
  // to look up the animation durations. This is mainly meant for internal use
  // but is public for tests.
  enum class SubAnimation {
    kHiddenFadeOut,
    kActionPendingFloodFill,
    kActionTriggeredFadeOut,
    kActivatedFloodFill,
    kDeactivatedFadeOut,
  };

  static base::TimeDelta GetSubAnimationDurationForTesting(
      SubAnimation sub_animation) {
    return GetSubAnimationDuration(sub_animation);
  }

 private:
  static base::TimeDelta GetSubAnimationDuration(SubAnimation sub_animation);

  void StartSubAnimation(SubAnimation sub_animation);

  // Checks that the states of our animations make sense given
  // |target_state_|. DCHECKs if something is wrong.
  void VerifyAnimationState() const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  gfx::Size size_;

  // The visual state we are controlling.
  InstallableInkDropPainter::State* const visual_state_;

  // Called when |visual_state_| changes so the user can repaint.
  base::RepeatingClosure repaint_callback_;

  InkDropState target_state_ = InkDropState::HIDDEN;

  // Used to animate the painter's highlight value in and out.
  gfx::SlideAnimation highlight_animation_;

  gfx::LinearAnimation flood_fill_animation_;
  gfx::LinearAnimation fade_out_animation_;

  gfx::Point last_event_location_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INSTALLABLE_INK_DROP_ANIMATOR_H_
