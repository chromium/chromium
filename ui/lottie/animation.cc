// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/lottie/animation.h"

#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/lottie/animation_observer.h"

namespace lottie {

Animation::TimerControl::TimerControl(const base::TimeDelta& offset,
                                      const base::TimeDelta& cycle_duration,
                                      const base::TimeDelta& total_duration,
                                      const base::TimeTicks& start_timestamp,
                                      bool should_reverse)
    : start_offset_(offset),
      end_offset_((offset + cycle_duration)),
      cycle_duration_(end_offset_ - start_offset_),
      total_duration_(total_duration),
      previous_tick_(start_timestamp),
      progress_(base::TimeDelta::FromMilliseconds(0)),
      current_cycle_progress_(start_offset_),
      should_reverse_(should_reverse) {}

void Animation::TimerControl::Step(const base::TimeTicks& timestamp) {
  progress_ += timestamp - previous_tick_;
  previous_tick_ = timestamp;

  base::TimeDelta completed_cycles_duration =
      completed_cycles_ * cycle_duration_;
  if (progress_ >= completed_cycles_duration + cycle_duration_) {
    completed_cycles_++;
    completed_cycles_duration += cycle_duration_;
  }

  current_cycle_progress_ =
      start_offset_ + progress_ - completed_cycles_duration;
  if (should_reverse_ && completed_cycles_ % 2) {
    current_cycle_progress_ =
        end_offset_ - (current_cycle_progress_ - start_offset_);
  }
}

void Animation::TimerControl::Resume(const base::TimeTicks& timestamp) {
  previous_tick_ = timestamp;
}

double Animation::TimerControl::GetNormalizedCurrentCycleProgress() const {
  return current_cycle_progress_ / total_duration_;
}

double Animation::TimerControl::GetNormalizedStartOffset() const {
  return start_offset_ / total_duration_;
}

double Animation::TimerControl::GetNormalizedEndOffset() const {
  return end_offset_ / total_duration_;
}

Animation::Animation(scoped_refptr<cc::SkottieWrapper> skottie)
    : skottie_(skottie) {}

Animation::~Animation() = default;

void Animation::SetAnimationObserver(AnimationObserver* observer) {
  DCHECK(!observer_ || !observer);
  observer_ = observer;
}

base::TimeDelta Animation::GetAnimationDuration() const {
  return base::TimeDelta::FromSecondsD(skottie_->duration());
}

gfx::Size Animation::GetOriginalSize() const {
#if DCHECK_IS_ON()
  // The size should have no fractional component.
  gfx::SizeF float_size = gfx::SkSizeToSizeF(skottie_->size());
  gfx::Size rounded_size = gfx::ToRoundedSize(float_size);

  float height_diff = std::abs(float_size.height() - rounded_size.height());
  float width_diff = std::abs(float_size.width() - rounded_size.width());

  DCHECK_LE(height_diff, std::numeric_limits<float>::epsilon());
  DCHECK_LE(width_diff, std::numeric_limits<float>::epsilon());
#endif
  return gfx::ToRoundedSize(gfx::SkSizeToSizeF(skottie_->size()));
}

void Animation::Start(Style style) {
  DCHECK_NE(state_, PlayState::kPaused);
  DCHECK_NE(state_, PlayState::kPlaying);
  StartSubsection(base::TimeDelta(), GetAnimationDuration(), style);
}

void Animation::StartSubsection(base::TimeDelta start_offset,
                                base::TimeDelta duration,
                                Style style) {
  DCHECK(state_ == PlayState::kStopped || state_ == PlayState::kEnded);
  DCHECK_LE(start_offset + duration, GetAnimationDuration());

  style_ = style;

  // Reset the |timer_control_| object for a new animation play.
  timer_control_.reset(nullptr);

  // Schedule a play for the animation and store the necessary information
  // needed to start playing.
  state_ = PlayState::kSchedulePlay;
  scheduled_start_offset_ = start_offset;
  scheduled_duration_ = duration;
}

void Animation::Pause() {
  DCHECK(state_ == PlayState::kPlaying || state_ == PlayState::kSchedulePlay);
  state_ = PlayState::kPaused;
}

void Animation::ResumePlaying() {
  DCHECK(state_ == PlayState::kPaused);
  state_ = PlayState::kScheduleResume;
}

void Animation::Stop() {
  state_ = PlayState::kStopped;
  timer_control_.reset(nullptr);
}

float Animation::GetCurrentProgress() const {
  switch (state_) {
    case PlayState::kStopped:
      return 0;
    case PlayState::kEnded:
      DCHECK(timer_control_);
      return timer_control_->GetNormalizedEndOffset();
    case PlayState::kPaused:
      // It may be that the timer hasn't been initialized, which may happen if
      // the animation was paused while it was in the kSchedulePlay state.
      return timer_control_
                 ? timer_control_->GetNormalizedCurrentCycleProgress()
                 : (scheduled_start_offset_ / GetAnimationDuration());
    case PlayState::kSchedulePlay:
    case PlayState::kPlaying:
    case PlayState::kScheduleResume:
      // The timer control needs to be initialized before making this call. It
      // may not have been initialized if OnAnimationStep has not been called
      // yet
      DCHECK(timer_control_);
      return timer_control_->GetNormalizedCurrentCycleProgress();
  }
}

void Animation::Paint(gfx::Canvas* canvas,
                      const base::TimeTicks& timestamp,
                      const gfx::Size& size) {
  switch (state_) {
    case PlayState::kStopped:
      return;
    case PlayState::kSchedulePlay:
      InitTimer(timestamp);
      state_ = PlayState::kPlaying;
      if (observer_)
        observer_->AnimationWillStartPlaying(this);
      break;
    case PlayState::kPlaying:
      UpdateState(timestamp);
      break;
    case PlayState::kPaused:
      break;
    case PlayState::kScheduleResume:
      state_ = PlayState::kPlaying;
      if (timer_control_) {
        timer_control_->Resume(timestamp);
      } else {
        // The animation may have been paused after a play was scheduled but
        // before it started playing.
        InitTimer(timestamp);
      }
      if (observer_)
        observer_->AnimationResuming(this);
      break;
    case PlayState::kEnded:
      break;
  }
  PaintFrame(canvas, GetCurrentProgress(), size);
}

void Animation::PaintFrame(gfx::Canvas* canvas,
                           float t,
                           const gfx::Size& size) {
  DCHECK_GE(t, 0.f);
  DCHECK_LE(t, 1.f);
  canvas->DrawSkottie(skottie(), gfx::Rect(size), t);
}

void Animation::InitTimer(const base::TimeTicks& timestamp) {
  DCHECK(!timer_control_);
  timer_control_ = std::make_unique<TimerControl>(
      scheduled_start_offset_, scheduled_duration_, GetAnimationDuration(),
      timestamp, style_ == Style::kThrobbing);
}

void Animation::UpdateState(const base::TimeTicks& timestamp) {
  DCHECK(timer_control_);
  int cycles = timer_control_->completed_cycles();
  timer_control_->Step(timestamp);

  if (cycles == timer_control_->completed_cycles())
    return;

  bool inform_observer = true;
  switch (style_) {
    case Style::kLoop:
      break;
    case Style::kThrobbing:
      // For a throbbing animation, the animation cycle ends when the timer
      // goes from 0 to 1 and then back to 0. So the number of timer cycles
      // must be even at the end of one throbbing animation cycle.
      if (timer_control_->completed_cycles() % 2 != 0)
        inform_observer = false;
      break;
    case Style::kLinear:
      state_ = PlayState::kEnded;
      break;
  }

  // Inform observer if the cycle has ended.
  if (observer_ && inform_observer) {
    observer_->AnimationCycleEnded(this);
  }
}

}  // namespace lottie
