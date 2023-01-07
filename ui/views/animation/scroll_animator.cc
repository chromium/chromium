// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/scroll_animator.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "ui/gfx/animation/slide_animation.h"

namespace {
constexpr float kDefaultAcceleration = -1500.0f;  // in pixels per second^2

// Assumes that d0 == 0.0f
float GetPosition(float v0, float a, float t) {
  float max_t = -v0 / a;
  if (t > max_t)
    t = max_t;
  return t * (v0 + 0.5f * a * t);
}

float GetDelta(float v0, float a, float t1, float t2) {
  return GetPosition(v0, a, t2) - GetPosition(v0, a, t1);
}

}  // namespace

namespace views {

ScrollAnimator::ScrollAnimator(ScrollDelegate* delegate)
    : delegate_(delegate), acceleration_(kDefaultAcceleration) {
  DCHECK(delegate);
}

ScrollAnimator::~ScrollAnimator() {
  Stop();
}

void ScrollAnimator::Start(float velocity_x, float velocity_y) {
  if (acceleration_ >= 0.0f)
    acceleration_ = kDefaultAcceleration;
  float v = std::max(fabs(velocity_x), fabs(velocity_y));
  last_t_ = 0.0f;
  velocity_x_ = velocity_x * velocity_multiplier_;
  velocity_y_ = velocity_y * velocity_multiplier_;
  duration_ = -v / acceleration_;  // in seconds
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(base::Seconds(duration_));
  animation_->Show();
}

void ScrollAnimator::Stop() {
  velocity_x_ = velocity_y_ = last_t_ = duration_ = 0.0f;
  animation_.reset();
}

void ScrollAnimator::AnimationEnded(const gfx::Animation* animation) {
  Stop();
  delegate_->OnFlingScrollEnded();
}

void ScrollAnimator::AnimationProgressed(const gfx::Animation* animation) {
  float t = static_cast<float>(animation->GetCurrentValue()) * duration_;
  float a_x = velocity_x_ > 0 ? acceleration_ : -acceleration_;
  float a_y = velocity_y_ > 0 ? acceleration_ : -acceleration_;
  float dx = GetDelta(velocity_x_, a_x, last_t_, t);
  float dy = GetDelta(velocity_y_, a_y, last_t_, t);
  last_t_ = t;
  delegate_->OnScroll(dx, dy);
}

void ScrollAnimator::AnimationCanceled(const gfx::Animation* animation) {
  Stop();
}

}  // namespace views
