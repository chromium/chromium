// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/slide_animation.h"

#include <math.h>

#include <algorithm>

#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {

SlideAnimation::SlideAnimation(AnimationDelegate* target)
    : LinearAnimation(target), target_(target) {}

SlideAnimation::~SlideAnimation() = default;

void SlideAnimation::Reset(double value) {
  direction_ = std::nullopt;
  value_current_ = value;
  Stop();
}

void SlideAnimation::Show() {
  BeginAnimating(Direction::kShowing);
}

void SlideAnimation::Hide() {
  BeginAnimating(Direction::kHiding);
}

void SlideAnimation::SetSlideDuration(base::TimeDelta duration) {
  slide_duration_ = duration;
}

void SlideAnimation::SetDampeningValue(double dampening_value) {
  dampening_value_ = dampening_value;
}

double SlideAnimation::GetCurrentValue() const {
  return value_current_;
}

base::TimeDelta SlideAnimation::GetDuration() {
  const double current_progress =
      direction_ == Direction::kShowing ? value_current_ : 1.0 - value_current_;

  return slide_duration_ * (1 - pow(current_progress, dampening_value_));
}

void SlideAnimation::BeginAnimating(Direction direction) {
  if (direction_ == direction)
    return;

  direction_ = direction;
  value_start_ = value_current_;
  value_end_ = (direction_ == Direction::kShowing) ? 1.0 : 0.0;

  // Make sure we actually have something to do.
  if (slide_duration_.is_zero()) {
    AnimateToState(1.0);  // Skip to the end of the animation.
    if (delegate()) {
      delegate()->AnimationProgressed(this);
      delegate()->AnimationEnded(this);
    }
  } else if (value_current_ != value_end_) {
    // This will also reset the currently-occurring animation.
    SetDuration(GetDuration());
    Start();
  }
}

void SlideAnimation::AnimateToState(double state) {
  state = Tween::CalculateValue(tween_type_, std::clamp(state, 0.0, 1.0));
  if (state == 1.0)
    direction_ = std::nullopt;

  value_current_ = value_start_ + (value_end_ - value_start_) * state;

  // Correct for any overshoot (while state may be capped at 1.0, let's not
  // take any rounding error chances.
  if ((value_end_ >= value_start_) ? (value_current_ > value_end_)
                                   : (value_current_ < value_end_)) {
    value_current_ = value_end_;
  }
}

}  // namespace gfx
