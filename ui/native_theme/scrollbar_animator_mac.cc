// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/scrollbar_animator_mac.h"

#include <algorithm>

#include "base/task/single_thread_task_runner.h"

namespace ui {

///////////////////////////////////////////////////////////////////////////////
// ScrollbarAnimationTimerMac

ScrollbarAnimationTimerMac::ScrollbarAnimationTimerMac(
    base::RepeatingCallback<void(double)> callback,
    double duration,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : start_time_(0.0), duration_(duration), callback_(std::move(callback)) {
  timing_function_ = gfx::CubicBezierTimingFunction::CreatePreset(
      gfx::CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
}

ScrollbarAnimationTimerMac::~ScrollbarAnimationTimerMac() {}

void ScrollbarAnimationTimerMac::Start() {
  start_time_ = base::Time::Now().InSecondsFSinceUnixEpoch();
  // Set the framerate of the animation. NSAnimation uses a default
  // framerate of 60 Hz, so use that here.
  timer_.Start(FROM_HERE, base::Seconds(1.0 / 60.0), this,
               &ScrollbarAnimationTimerMac::TimerFired);
}

void ScrollbarAnimationTimerMac::Stop() {
  timer_.Stop();
}

void ScrollbarAnimationTimerMac::SetDuration(double duration) {
  duration_ = duration;
}

void ScrollbarAnimationTimerMac::TimerFired() {
  double current_time = base::Time::Now().InSecondsFSinceUnixEpoch();
  double delta = current_time - start_time_;

  if (delta >= duration_)
    timer_.Stop();

  double fraction = delta / duration_;
  fraction = std::clamp(fraction, 0.0, 1.0);
  double progress = timing_function_->GetValue(fraction);
  // Note that `this` may be destroyed from within `callback_`, so it is not
  // safe to call any other code after it.
  callback_.Run(progress);
}

///////////////////////////////////////////////////////////////////////////////
// OverlayScrollbarAnimatorMac

OverlayScrollbarAnimatorMac::OverlayScrollbarAnimatorMac(
    Client* client,
    int thumb_width_expanded,
    int thumb_width_unexpanded,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : client_(client),
      thumb_width_expanded_(thumb_width_expanded),
      thumb_width_unexpanded_(thumb_width_unexpanded),
      thumb_width_(thumb_width_unexpanded),
      task_runner_(task_runner),
      weak_factory_(this) {}

OverlayScrollbarAnimatorMac::~OverlayScrollbarAnimatorMac() = default;

void OverlayScrollbarAnimatorMac::MouseDidEnter() {
  // If the scrollbar is completely hidden, ignore this. We will initialize
  // the `mouse_in_track_` state if there's a scroll.
  if (thumb_alpha_ == 0.f)
    return;

  if (mouse_in_track_)
    return;
  mouse_in_track_ = true;

  // Cancel any in-progress fade-out, and ensure that the fade-out timer be
  // disabled.
  if (fade_out_animation_)
    FadeOutAnimationCancel();
  FadeOutTimerUpdate();

  // Start the fade-in animation (unless it is in progress or has already
  // completed).
  if (!fade_in_track_animation_ && track_alpha_ != 1.f)
    FadeInTrackAnimationStart();

  // Start the expand-thumb animation (unless it is in progress or has already
  // completed).
  if (!expand_thumb_animation_ && thumb_width_ != thumb_width_expanded_)
    ExpandThumbAnimationStart();
}

void OverlayScrollbarAnimatorMac::MouseDidExit() {
  // Ensure that the fade-out timer be re-enabled.
  mouse_in_track_ = false;
  FadeOutTimerUpdate();
}

void OverlayScrollbarAnimatorMac::DidScroll() {
  // If we were fading out, then immediately return to being fully opaque for
  // both the thumb and track.
  if (fade_out_animation_) {
    FadeOutAnimationCancel();
    FadeOutTimerUpdate();
    return;
  }

  // If the scrollbar was already fully visible, then just update the fade-out
  // timer.
  if (thumb_alpha_ == 1.f) {
    FadeOutTimerUpdate();
    return;
  }

  // This is an initial scroll causing the thumb to appear.
  DCHECK_EQ(thumb_width_, thumb_width_unexpanded_);
  DCHECK_EQ(thumb_alpha_, 0.f);
  DCHECK(!fade_in_track_animation_);
  thumb_width_ = thumb_width_unexpanded_;
  thumb_alpha_ = 1;
  client_->SetThumbNeedsDisplay();
  client_->SetHidden(false);

  // If the initial scroll is done inside the scrollbar's area, then also
  // cause the track to appear and the thumb to enlarge.
  if (client_->IsMouseInScrollbarFrameRect()) {
    mouse_in_track_ = true;
    thumb_width_ = thumb_width_expanded_;
    track_alpha_ = 1;
    client_->SetTrackNeedsDisplay();
  }

  // Update the fade-out timer (now that we know whether or not the mouse
  // is in the track).
  FadeOutTimerUpdate();
}

void OverlayScrollbarAnimatorMac::ExpandThumbAnimationStart() {
  DCHECK(!expand_thumb_animation_);
  DCHECK_NE(thumb_width_, thumb_width_expanded_);
  expand_thumb_animation_ = std::make_unique<ScrollbarAnimationTimerMac>(
      base::BindRepeating(
          &OverlayScrollbarAnimatorMac::ExpandThumbAnimationTicked,
          weak_factory_.GetWeakPtr()),
      kAnimationDurationSeconds, task_runner_);
  expand_thumb_animation_->Start();
}

void OverlayScrollbarAnimatorMac::ExpandThumbAnimationTicked(double progress) {
  thumb_width_ = (1 - progress) * thumb_width_unexpanded_ +
                 progress * thumb_width_expanded_;
  client_->SetThumbNeedsDisplay();
  if (progress == 1)
    expand_thumb_animation_.reset();
}

void OverlayScrollbarAnimatorMac::FadeInTrackAnimationStart() {
  DCHECK(!fade_in_track_animation_);
  DCHECK(!fade_out_animation_);
  fade_in_track_animation_ = std::make_unique<ScrollbarAnimationTimerMac>(
      base::BindRepeating(
          &OverlayScrollbarAnimatorMac::FadeInTrackAnimationTicked,
          weak_factory_.GetWeakPtr()),
      kAnimationDurationSeconds, task_runner_);
  fade_in_track_animation_->Start();
}

void OverlayScrollbarAnimatorMac::FadeInTrackAnimationTicked(double progress) {
  // Fade-in and fade-out are mutually exclusive.
  DCHECK(!fade_out_animation_);

  track_alpha_ = progress;
  client_->SetTrackNeedsDisplay();
  if (progress == 1) {
    fade_in_track_animation_.reset();
  }
}

void OverlayScrollbarAnimatorMac::FadeOutTimerUpdate() {
  if (mouse_in_track_) {
    start_scrollbar_fade_out_timer_.reset();
    return;
  }
  if (!start_scrollbar_fade_out_timer_) {
    start_scrollbar_fade_out_timer_ =
        std::make_unique<base::RetainingOneShotTimer>(
            FROM_HERE, kFadeOutDelay,
            base::BindRepeating(
                &OverlayScrollbarAnimatorMac::FadeOutAnimationStart,
                weak_factory_.GetWeakPtr()));
    start_scrollbar_fade_out_timer_->SetTaskRunner(task_runner_);
  }
  start_scrollbar_fade_out_timer_->Reset();
}

void OverlayScrollbarAnimatorMac::FadeOutAnimationStart() {
  start_scrollbar_fade_out_timer_.reset();
  fade_in_track_animation_.reset();
  fade_out_animation_.reset();

  fade_out_animation_ = std::make_unique<ScrollbarAnimationTimerMac>(
      base::BindRepeating(&OverlayScrollbarAnimatorMac::FadeOutAnimationTicked,
                          weak_factory_.GetWeakPtr()),
      kAnimationDurationSeconds, task_runner_);
  fade_out_animation_->Start();
}

void OverlayScrollbarAnimatorMac::FadeOutAnimationTicked(double progress) {
  DCHECK(!fade_in_track_animation_);

  // Fade out the thumb.
  thumb_alpha_ = 1 - progress;
  client_->SetThumbNeedsDisplay();

  // If the track is not already invisible, fade it out.
  if (track_alpha_ != 0) {
    track_alpha_ = 1 - progress;
    client_->SetTrackNeedsDisplay();
  }

  // Once completely faded out, reset all state.
  if (progress == 1) {
    expand_thumb_animation_.reset();
    fade_out_animation_.reset();

    thumb_width_ = thumb_width_unexpanded_;
    DCHECK_EQ(thumb_alpha_, 0.f);
    DCHECK_EQ(track_alpha_, 0.f);

    // Mark that the scrollbars were hidden.
    client_->SetHidden(true);
  }
}

void OverlayScrollbarAnimatorMac::FadeOutAnimationCancel() {
  DCHECK(fade_out_animation_);
  fade_out_animation_.reset();
  thumb_alpha_ = 1;
  client_->SetThumbNeedsDisplay();
  if (track_alpha_ > 0) {
    track_alpha_ = 1;
    client_->SetTrackNeedsDisplay();
  }
}

const float OverlayScrollbarAnimatorMac::kAnimationDurationSeconds = 0.25f;
const base::TimeDelta OverlayScrollbarAnimatorMac::kFadeOutDelay =
    base::Milliseconds(500);

}  // namespace ui
