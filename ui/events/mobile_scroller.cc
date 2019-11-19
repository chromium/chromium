// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mobile_scroller.h"

#include <cmath>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/numerics/math_constants.h"

namespace ui {
namespace {

// Default scroll duration from android.widget.Scroller.
const int kDefaultDurationMs = 250;

// Default friction constant in android.view.ViewConfiguration.
const float kDefaultFriction = 0.015f;

// == std::log(0.78f) / std::log(0.9f)
const float kDecelerationRate = 2.3582018f;

// Tension lines cross at (kInflexion, 1).
const float kInflexion = 0.35f;

const float kEpsilon = 1e-5f;

// Fling scroll is stopped when the scroll position is |kThresholdForFlingEnd|
// pixels or closer from the end.
const float kThresholdForFlingEnd = 0.1;

bool ApproxEquals(float a, float b) {
  return std::abs(a - b) < kEpsilon;
}

struct ViscosityConstants {
  ViscosityConstants()
      : viscous_fluid_scale_(8.f), viscous_fluid_normalize_(1.f) {
    viscous_fluid_normalize_ = 1.0f / ApplyViscosity(1.0f);
  }

  float ApplyViscosity(float x) {
    x *= viscous_fluid_scale_;
    if (x < 1.0f) {
      x -= (1.0f - std::exp(-x));
    } else {
      float start = 0.36787944117f;  // 1/e == exp(-1)
      x = 1.0f - std::exp(1.0f - x);
      x = start + x * (1.0f - start);
    }
    x *= viscous_fluid_normalize_;
    return x;
  }

 private:
  // This controls the intensity of the viscous fluid effect.
  float viscous_fluid_scale_;
  float viscous_fluid_normalize_;

  DISALLOW_COPY_AND_ASSIGN(ViscosityConstants);
};

struct SplineConstants {
  SplineConstants() {
    const float kStartTension = 0.5f;
    const float kEndTension = 1.0f;
    const float kP1 = kStartTension * kInflexion;
    const float kP2 = 1.0f - kEndTension * (1.0f - kInflexion);

    float x_min = 0.0f;
    float y_min = 0.0f;
    for (int i = 0; i < NUM_SAMPLES; i++) {
      const float alpha = static_cast<float>(i) / NUM_SAMPLES;

      float x_max = 1.0f;
      float x, tx, coef;
      while (true) {
        x = x_min + (x_max - x_min) / 2.0f;
        coef = 3.0f * x * (1.0f - x);
        tx = coef * ((1.0f - x) * kP1 + x * kP2) + x * x * x;
        if (ApproxEquals(tx, alpha))
          break;
        if (tx > alpha)
          x_max = x;
        else
          x_min = x;
      }
      spline_position_[i] = coef * ((1.0f - x) * kStartTension + x) + x * x * x;

      float y_max = 1.0f;
      float y, dy;
      while (true) {
        y = y_min + (y_max - y_min) / 2.0f;
        coef = 3.0f * y * (1.0f - y);
        dy = coef * ((1.0f - y) * kStartTension + y) + y * y * y;
        if (ApproxEquals(dy, alpha))
          break;
        if (dy > alpha)
          y_max = y;
        else
          y_min = y;
      }
      spline_time_[i] = coef * ((1.0f - y) * kP1 + y * kP2) + y * y * y;
    }
    spline_position_[NUM_SAMPLES] = spline_time_[NUM_SAMPLES] = 1.0f;
  }

  void CalculateCoefficients(float t,
                             float* distance_coef,
                             float* velocity_coef) {
    *distance_coef = 1.f;
    *velocity_coef = 0.f;
    const int index = static_cast<int>(NUM_SAMPLES * t);
    if (index < NUM_SAMPLES) {
      const float t_inf = static_cast<float>(index) / NUM_SAMPLES;
      const float t_sup = static_cast<float>(index + 1) / NUM_SAMPLES;
      const float d_inf = spline_position_[index];
      const float d_sup = spline_position_[index + 1];
      *velocity_coef = (d_sup - d_inf) / (t_sup - t_inf);
      *distance_coef = d_inf + (t - t_inf) * *velocity_coef;
    }
  }

 private:
  enum { NUM_SAMPLES = 100 };

  float spline_position_[NUM_SAMPLES + 1];
  float spline_time_[NUM_SAMPLES + 1];

  DISALLOW_COPY_AND_ASSIGN(SplineConstants);
};

float ComputeDeceleration(float friction) {
  return base::kMeanGravityFloat  // g (m/s^2)
         * 39.37f                 // inch/meter
         * 160.f                  // pixels/inch
         * friction;
}

template <typename T>
int Signum(T t) {
  return (T(0) < t) - (t < T(0));
}

template <typename T>
T Clamped(T t, T a, T b) {
  return t < a ? a : (t > b ? b : t);
}

// Leaky to allow access from the impl thread.
base::LazyInstance<ViscosityConstants>::Leaky g_viscosity_constants =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<SplineConstants>::Leaky g_spline_constants =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

MobileScroller::Config::Config()
    : fling_friction(kDefaultFriction),
      flywheel_enabled(false),
      chromecast_optimized(false) {}

MobileScroller::MobileScroller(const Config& config)
    : mode_(UNDEFINED),
      start_x_(0),
      start_y_(0),
      final_x_(0),
      final_y_(0),
      min_x_(0),
      max_x_(0),
      min_y_(0),
      max_y_(0),
      curr_x_(0),
      curr_y_(0),
      duration_seconds_reciprocal_(1),
      delta_x_(0),
      delta_x_norm_(1),
      delta_y_(0),
      delta_y_norm_(1),
      finished_(true),
      flywheel_enabled_(config.flywheel_enabled),
      velocity_(0),
      curr_velocity_(0),
      distance_(0),
      fling_friction_(config.fling_friction),
      deceleration_(ComputeDeceleration(fling_friction_)),
      tuning_coeff_(
          ComputeDeceleration(config.chromecast_optimized ? 0.9f : 0.84f)) {}

MobileScroller::~MobileScroller() {}

bool MobileScroller::ComputeScrollOffset(base::TimeTicks time,
                                         gfx::Vector2dF* offset,
                                         gfx::Vector2dF* velocity) {
  DCHECK(offset);
  DCHECK(velocity);
  if (!ComputeScrollOffsetInternal(time)) {
    *offset = gfx::Vector2dF(GetFinalX(), GetFinalY());
    *velocity = gfx::Vector2dF();
    return false;
  }

  *offset = gfx::Vector2dF(GetCurrX(), GetCurrY());
  *velocity = gfx::Vector2dF(GetCurrVelocityX(), GetCurrVelocityY());
  return true;
}

void MobileScroller::StartScroll(float start_x,
                                 float start_y,
                                 float dx,
                                 float dy,
                                 base::TimeTicks start_time) {
  StartScroll(start_x, start_y, dx, dy, start_time,
              base::TimeDelta::FromMilliseconds(kDefaultDurationMs));
}

void MobileScroller::StartScroll(float start_x,
                                 float start_y,
                                 float dx,
                                 float dy,
                                 base::TimeTicks start_time,
                                 base::TimeDelta duration) {
  DCHECK_GT(duration, base::TimeDelta());
  mode_ = SCROLL_MODE;
  finished_ = false;
  duration_ = duration;
  duration_seconds_reciprocal_ = 1.0 / duration_.InSecondsF();
  start_time_ = start_time;
  curr_x_ = start_x_ = start_x;
  curr_y_ = start_y_ = start_y;
  final_x_ = start_x + dx;
  final_y_ = start_y + dy;
  RecomputeDeltas();
  curr_time_ = start_time_;
}

void MobileScroller::Fling(float start_x,
                           float start_y,
                           float velocity_x,
                           float velocity_y,
                           float min_x,
                           float max_x,
                           float min_y,
                           float max_y,
                           base::TimeTicks start_time) {
  DCHECK(velocity_x || velocity_y);

  // Continue a scroll or fling in progress.
  if (flywheel_enabled_ && !finished_) {
    float old_velocity_x = GetCurrVelocityX();
    float old_velocity_y = GetCurrVelocityY();
    if (Signum(velocity_x) == Signum(old_velocity_x) &&
        Signum(velocity_y) == Signum(old_velocity_y)) {
      velocity_x += old_velocity_x;
      velocity_y += old_velocity_y;
    }
  }

  mode_ = FLING_MODE;
  finished_ = false;

  float velocity = std::sqrt(velocity_x * velocity_x + velocity_y * velocity_y);

  velocity_ = velocity;
  duration_ = GetSplineFlingDuration(velocity);
  DCHECK_GT(duration_, base::TimeDelta());
  duration_seconds_reciprocal_ = 1.0 / duration_.InSecondsF();
  start_time_ = start_time;
  curr_time_ = start_time_;
  curr_x_ = start_x_ = start_x;
  curr_y_ = start_y_ = start_y;

  float coeff_x = velocity == 0 ? 1.0f : velocity_x / velocity;
  float coeff_y = velocity == 0 ? 1.0f : velocity_y / velocity;

  double total_distance = GetSplineFlingDistance(velocity);
  distance_ = total_distance * Signum(velocity);

  min_x_ = min_x;
  max_x_ = max_x;
  min_y_ = min_y;
  max_y_ = max_y;

  final_x_ = start_x + total_distance * coeff_x;
  final_x_ = Clamped(final_x_, min_x_, max_x_);

  final_y_ = start_y + total_distance * coeff_y;
  final_y_ = Clamped(final_y_, min_y_, max_y_);

  RecomputeDeltas();
}

void MobileScroller::ExtendDuration(base::TimeDelta extend) {
  base::TimeDelta passed = GetTimePassed();
  duration_ = passed + extend;
  duration_seconds_reciprocal_ = 1.0 / duration_.InSecondsF();
  finished_ = false;
}

void MobileScroller::SetFinalX(float new_x) {
  final_x_ = new_x;
  finished_ = false;
  RecomputeDeltas();
}

void MobileScroller::SetFinalY(float new_y) {
  final_y_ = new_y;
  finished_ = false;
  RecomputeDeltas();
}

void MobileScroller::AbortAnimation() {
  curr_x_ = final_x_;
  curr_y_ = final_y_;
  curr_velocity_ = 0;
  curr_time_ = start_time_ + duration_;
  finished_ = true;
}

void MobileScroller::ForceFinished(bool finished) {
  finished_ = finished;
}

bool MobileScroller::IsFinished() const {
  return finished_;
}

base::TimeDelta MobileScroller::GetTimePassed() const {
  return curr_time_ - start_time_;
}

base::TimeDelta MobileScroller::GetDuration() const {
  return duration_;
}

float MobileScroller::GetCurrX() const {
  return curr_x_;
}

float MobileScroller::GetCurrY() const {
  return curr_y_;
}

float MobileScroller::GetCurrVelocity() const {
  if (finished_)
    return 0;
  if (mode_ == FLING_MODE)
    return curr_velocity_;
  return velocity_ - deceleration_ * GetTimePassed().InSecondsF() * 0.5f;
}

float MobileScroller::GetCurrVelocityX() const {
  return delta_x_norm_ * GetCurrVelocity();
}

float MobileScroller::GetCurrVelocityY() const {
  return delta_y_norm_ * GetCurrVelocity();
}

float MobileScroller::GetStartX() const {
  return start_x_;
}

float MobileScroller::GetStartY() const {
  return start_y_;
}

float MobileScroller::GetFinalX() const {
  return final_x_;
}

float MobileScroller::GetFinalY() const {
  return final_y_;
}

bool MobileScroller::IsScrollingInDirection(float xvel, float yvel) const {
  return !finished_ && Signum(xvel) == Signum(delta_x_) &&
         Signum(yvel) == Signum(delta_y_);
}

bool MobileScroller::ComputeScrollOffsetInternal(base::TimeTicks time) {
  if (finished_)
    return false;

  if (time <= start_time_)
    return true;

  if (time == curr_time_)
    return true;

  base::TimeDelta time_passed = time - start_time_;
  if (time_passed >= duration_) {
    AbortAnimation();
    return false;
  }

  curr_time_ = time;

  const float u = time_passed.InSecondsF() * duration_seconds_reciprocal_;
  switch (mode_) {
    case UNDEFINED:
      NOTREACHED() << "|StartScroll()| or |Fling()| must be called prior to "
                      "scroll offset computation.";
      return false;

    case SCROLL_MODE: {
      float x = g_viscosity_constants.Get().ApplyViscosity(u);

      curr_x_ = start_x_ + x * delta_x_;
      curr_y_ = start_y_ + x * delta_y_;
    } break;

    case FLING_MODE: {
      float distance_coef = 1.f;
      float velocity_coef = 0.f;
      g_spline_constants.Get().CalculateCoefficients(u, &distance_coef,
                                                     &velocity_coef);

      curr_velocity_ = velocity_coef * distance_ * duration_seconds_reciprocal_;

      curr_x_ = start_x_ + distance_coef * delta_x_;
      curr_x_ = Clamped(curr_x_, min_x_, max_x_);

      curr_y_ = start_y_ + distance_coef * delta_y_;
      curr_y_ = Clamped(curr_y_, min_y_, max_y_);

      float diff_x = std::abs(curr_x_ - final_x_);
      float diff_y = std::abs(curr_y_ - final_y_);
      if (diff_x < kThresholdForFlingEnd && diff_y < kThresholdForFlingEnd)
        AbortAnimation();
    } break;
  }

  return !finished_;
}

void MobileScroller::RecomputeDeltas() {
  delta_x_ = final_x_ - start_x_;
  delta_y_ = final_y_ - start_y_;

  const float hyp = std::sqrt(delta_x_ * delta_x_ + delta_y_ * delta_y_);
  if (hyp > kEpsilon) {
    delta_x_norm_ = delta_x_ / hyp;
    delta_y_norm_ = delta_y_ / hyp;
  } else {
    delta_x_norm_ = delta_y_norm_ = 1;
  }
}

double MobileScroller::GetSplineDeceleration(float velocity) const {
  return std::log(kInflexion * std::abs(velocity) /
                  (fling_friction_ * tuning_coeff_));
}

base::TimeDelta MobileScroller::GetSplineFlingDuration(float velocity) const {
  const double l = GetSplineDeceleration(velocity);
  const double decel_minus_one = kDecelerationRate - 1.0;
  const double time_seconds = std::exp(l / decel_minus_one);
  return base::TimeDelta::FromMicroseconds(time_seconds *
                                           base::Time::kMicrosecondsPerSecond);
}

double MobileScroller::GetSplineFlingDistance(float velocity) const {
  const double l = GetSplineDeceleration(velocity);
  const double decel_minus_one = kDecelerationRate - 1.0;
  return fling_friction_ * tuning_coeff_ *
         std::exp(kDecelerationRate / decel_minus_one * l);
}

}  // namespace ui
