// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/installable_ink_drop_animator.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/animation/installable_ink_drop_painter.h"

namespace views {

// static
constexpr base::TimeDelta
    InstallableInkDropAnimator::kHighlightAnimationDuration;

InstallableInkDropAnimator::InstallableInkDropAnimator(
    gfx::Size size,
    InstallableInkDropPainter::State* visual_state,
    gfx::AnimationContainer* animation_container,
    base::RepeatingClosure repaint_callback)
    : size_(size),
      visual_state_(visual_state),
      repaint_callback_(repaint_callback),
      highlight_animation_(this),
      flood_fill_animation_(this),
      fade_out_animation_(this) {
  highlight_animation_.SetContainer(animation_container);
  flood_fill_animation_.SetContainer(animation_container);
  fade_out_animation_.SetContainer(animation_container);

  highlight_animation_.SetSlideDuration(kHighlightAnimationDuration);
}

InstallableInkDropAnimator::~InstallableInkDropAnimator() = default;

void InstallableInkDropAnimator::SetSize(gfx::Size size) {
  size_ = size;
}

void InstallableInkDropAnimator::SetLastEventLocation(
    gfx::Point last_event_location) {
  last_event_location_ = last_event_location;
}

void InstallableInkDropAnimator::AnimateToState(InkDropState target_state) {
  VerifyAnimationState();

  const InkDropState last_state = target_state_;

  TRACE_EVENT2("views", "InstallableInkDropAnimator::AnimateToState",
               "target_state", target_state, "last_state", last_state);

  switch (target_state) {
    case InkDropState::HIDDEN:
    case InkDropState::DEACTIVATED:
      // If we weren't animating to a visible state, we shouldn't do anything.
      if (last_state == InkDropState::HIDDEN ||
          last_state == InkDropState::DEACTIVATED) {
        break;
      }
      // If we were flood-filling, jump to the end of that animation.
      if (flood_fill_animation_.is_animating()) {
        flood_fill_animation_.End();
      }
      // If we weren't already fading out, start fading out. Otherwise, just
      // continue fading out.
      if (!fade_out_animation_.is_animating()) {
        const SubAnimation sub_animation =
            target_state == InkDropState::HIDDEN
                ? SubAnimation::kHiddenFadeOut
                : SubAnimation::kDeactivatedFadeOut;
        StartSubAnimation(sub_animation);
      }
      break;

    case InkDropState::ACTION_PENDING:
      if (last_state != InkDropState::HIDDEN) {
        // Snap to hidden state by stopping all animations.
        flood_fill_animation_.Stop();
        fade_out_animation_.Stop();
      }
      StartSubAnimation(SubAnimation::kActionPendingFloodFill);
      break;

    case InkDropState::ACTION_TRIGGERED:
      if (last_state == InkDropState::HIDDEN) {
        // Start the flood fill. On this animation's end, we will start the fade
        // out animation in AnimationEnded().
        StartSubAnimation(SubAnimation::kActionPendingFloodFill);
      } else if (last_state == InkDropState::ACTION_PENDING &&
                 !flood_fill_animation_.is_animating()) {
        // If we were done animating to ACTION_PENDING, we must start the fade
        // out animation here.
        StartSubAnimation(SubAnimation::kActionTriggeredFadeOut);
        // If we were in ACTION_PENDING but weren't done animating, we will
        // start the fade out animation in AnimationEnded().
      } else if (last_state != InkDropState::ACTION_PENDING) {
        // Any other state transitions are invalid.
        NOTREACHED() << "Transition from " << ToString(last_state)
                     << " to ACTION_TRIGGERED is invalid.";
      }
      break;

    case InkDropState::ALTERNATE_ACTION_PENDING:
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      // TODO(crbug.com/933384): handle these.
      NOTREACHED() << "target_state = " << ToString(target_state);
      break;

    case InkDropState::ACTIVATED:
      if (fade_out_animation_.is_animating()) {
        // If we were fading out of ACTIVATED or ACTION_TRIGGERED, finish the
        // animation to reset to HIDDEN state.
        fade_out_animation_.End();
      }

      // Now simply start the flood fill animation again.
      StartSubAnimation(SubAnimation::kActivatedFloodFill);
      break;
  }

  target_state_ = target_state;
  VerifyAnimationState();
  repaint_callback_.Run();
}

void InstallableInkDropAnimator::AnimateHighlight(bool fade_in) {
  if (fade_in) {
    highlight_animation_.Show();
  } else {
    highlight_animation_.Hide();
  }
}

base::TimeDelta InstallableInkDropAnimator::GetSubAnimationDuration(
    SubAnimation sub_animation) {
  switch (sub_animation) {
    case SubAnimation::kHiddenFadeOut:
      return base::TimeDelta::FromMilliseconds(200);
    case SubAnimation::kActionPendingFloodFill:
      return base::TimeDelta::FromMilliseconds(240);
    case SubAnimation::kActionTriggeredFadeOut:
      return base::TimeDelta::FromMilliseconds(300);
    case SubAnimation::kActivatedFloodFill:
      return base::TimeDelta::FromMilliseconds(200);
    case SubAnimation::kDeactivatedFadeOut:
      return base::TimeDelta::FromMilliseconds(300);
  }
}

void InstallableInkDropAnimator::StartSubAnimation(SubAnimation sub_animation) {
  const base::TimeDelta duration = GetSubAnimationDuration(sub_animation);
  switch (sub_animation) {
    case SubAnimation::kHiddenFadeOut:
    case SubAnimation::kActionTriggeredFadeOut:
    case SubAnimation::kDeactivatedFadeOut:
      if (!fade_out_animation_.is_animating()) {
        fade_out_animation_.SetDuration(duration);
        fade_out_animation_.Start();
      }
      break;
    case SubAnimation::kActionPendingFloodFill:
    case SubAnimation::kActivatedFloodFill:
      if (!flood_fill_animation_.is_animating()) {
        flood_fill_animation_.SetDuration(duration);
        flood_fill_animation_.Start();
      }
      break;
  }
}

void InstallableInkDropAnimator::VerifyAnimationState() const {
#if DCHECK_IS_ON()
  switch (target_state_) {
    case InkDropState::HIDDEN:
    case InkDropState::DEACTIVATED:
      // We can only be fading out or completely invisible. So, we cannot be
      // flood-filling.
      DCHECK(!flood_fill_animation_.is_animating());
      break;
    case InkDropState::ACTION_PENDING:
    case InkDropState::ACTIVATED:
      // These states flood-fill then stay visible.
      DCHECK(!fade_out_animation_.is_animating());
      if (!flood_fill_animation_.is_animating())
        DCHECK_EQ(1.0f, visual_state_->flood_fill_progress);
      break;
    case InkDropState::ACTION_TRIGGERED:
      // We should always be animating during this state. Once animations are
      // done, we automatically transition to HIDDEN. Make sure exactly one of
      // our animations are running.
      DCHECK_NE(flood_fill_animation_.is_animating(),
                fade_out_animation_.is_animating());
      break;
    case InkDropState::ALTERNATE_ACTION_PENDING:
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      // TODO(crbug.com/933384): handle these.
      NOTREACHED() << "target_state = " << ToString(target_state_);
      break;
  }
#endif  // DCHECK_IS_ON()
}

void InstallableInkDropAnimator::AnimationEnded(
    const gfx::Animation* animation) {
  TRACE_EVENT0("views", "InstallableInkDropAnimator::AnimationEnded");

  if (animation == &flood_fill_animation_) {
    switch (target_state_) {
      case InkDropState::ACTION_PENDING:
      case InkDropState::ACTIVATED:
        // The ink drop stays filled in these states so nothing needs to be
        // done.
        break;

      case InkDropState::ACTION_TRIGGERED:
        // After filling, the ink drop should fade out.
        StartSubAnimation(SubAnimation::kActionTriggeredFadeOut);
        break;

      case InkDropState::ALTERNATE_ACTION_PENDING:
      case InkDropState::ALTERNATE_ACTION_TRIGGERED:
        // TODO(crbug.com/933384): handle these.
        NOTREACHED() << "target_state_ = " << ToString(target_state_);
        break;

      case InkDropState::HIDDEN:
      case InkDropState::DEACTIVATED:
        // The flood fill animation should never run in these states.
        NOTREACHED() << "target_state_ = " << ToString(target_state_);
        break;
    }

    // The ink drop is now fully activated.
    visual_state_->flood_fill_progress = 1.0f;
    repaint_callback_.Run();
  } else if (animation == &fade_out_animation_) {
    switch (target_state_) {
      case InkDropState::HIDDEN:
        // Nothing to do.
        break;

      case InkDropState::ACTION_TRIGGERED:
      case InkDropState::DEACTIVATED:
        // These states transition to HIDDEN when they're done animating.
        target_state_ = InkDropState::HIDDEN;
        break;

      case InkDropState::ACTION_PENDING:
      case InkDropState::ACTIVATED:
        // The fade out animation should never run in these states.
        NOTREACHED() << "target_state_ = " << ToString(target_state_);
        break;

      case InkDropState::ALTERNATE_ACTION_PENDING:
      case InkDropState::ALTERNATE_ACTION_TRIGGERED:
        // TODO(crbug.com/933384): handle these.
        NOTREACHED() << "target_state_ = " << ToString(target_state_);
        break;
    }

    // The ink drop fill is fully invisible.
    visual_state_->flood_fill_progress = 0.0f;
    repaint_callback_.Run();
  }
}

void InstallableInkDropAnimator::AnimationProgressed(
    const gfx::Animation* animation) {
  TRACE_EVENT0("views", "InstallableInkDropAnimator::AnimationProgressed");

  if (animation == &highlight_animation_) {
    visual_state_->highlighted_ratio = highlight_animation_.GetCurrentValue();
  } else if (animation == &flood_fill_animation_) {
    visual_state_->flood_fill_center = gfx::PointF(last_event_location_);
    visual_state_->flood_fill_progress = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_SLOW_IN, flood_fill_animation_.GetCurrentValue());
  } else if (animation == &fade_out_animation_) {
    // Do nothing for now.
  } else {
    NOTREACHED();
  }

  repaint_callback_.Run();
}

}  // namespace views
