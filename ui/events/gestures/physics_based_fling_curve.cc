// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/physics_based_fling_curve.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"

namespace {

// These constants are defined based on UX experiment.
const float kDefaultP1X = 0.2f;
const float kDefaultP1Y = 1.0f;
const float kDefaultP2X = 0.55f;
const float kDefaultP2Y = 1.0f;
const float kDefaultPixelDeceleration = 2300.0f;
const float kMaxCurveDurationForFling = 2.0f;
const float kDefaultPhysicalDeceleration = 2.7559e-5f;  // inch/ms^2.

inline gfx::Vector2dF GetPositionAtValue(const gfx::Vector2dF& end_point,
                                         double progress) {
  return gfx::ScaleVector2d(end_point, progress);
}

inline gfx::Vector2dF GetVelocityAtTime(const gfx::Vector2dF& current_offset,
                                        const gfx::Vector2dF& prev_offset,
                                        base::TimeDelta delta) {
  return gfx::ScaleVector2d(current_offset - prev_offset, delta.ToHz());
}

float GetOffset(float velocity, float deceleration, float duration) {
  const float position =
      (std::abs(velocity) - deceleration * duration * 0.5) * duration;
  return std::copysign(position, velocity);
}

gfx::Vector2dF GetDecelerationInPixelsPerMs2(
    const gfx::Vector2dF& pixels_per_inch) {
  return gfx::ScaleVector2d(pixels_per_inch, kDefaultPhysicalDeceleration);
}

gfx::Vector2dF GetDuration(const gfx::Vector2dF& velocity,
                           const gfx::Vector2dF& deceleration) {
  return gfx::Vector2dF(fabs(velocity.x() / deceleration.x()),
                        fabs(velocity.y() / deceleration.y()));
}

// Calculates the |distance_| based on the fling velocity. It utilizes
// following equation for |distance_| calculation. d = v*t - 0.5*a*t^2 where d
// = distance, v = initial velocity, a = acceleration and time = duration before
// it reaches zero velocity. time = final velocity(0) - initial velocity(v) /
// acceleration (a) This |distance_| is later used by PhysicsBasedFlingCurve to
// generate fling animation curve.
gfx::Vector2dF CalculateEndPoint(const gfx::Vector2dF& pixels_per_inch,
                                 const gfx::Vector2dF& velocity_pixels_per_ms,
                                 const gfx::Size& bounding_size) {
  // deceleration is in pixels/ ms^2.
  gfx::Vector2dF deceleration = GetDecelerationInPixelsPerMs2(pixels_per_inch);

  // duration is in ms
  gfx::Vector2dF duration = GetDuration(velocity_pixels_per_ms, deceleration);

  // distance offset in screen coordinate
  gfx::Vector2dF offset_in_screen_coord_space = gfx::Vector2dF(
      GetOffset(velocity_pixels_per_ms.x(), deceleration.x(), duration.x()),
      GetOffset(velocity_pixels_per_ms.y(), deceleration.y(), duration.y()));

  if (std::abs(offset_in_screen_coord_space.x()) > bounding_size.width()) {
    float sign = offset_in_screen_coord_space.x() > 0 ? 1 : -1;
    offset_in_screen_coord_space.set_x(bounding_size.width() * sign);
  }

  if (std::abs(offset_in_screen_coord_space.y()) > bounding_size.height()) {
    float sign = offset_in_screen_coord_space.y() > 0 ? 1 : -1;
    offset_in_screen_coord_space.set_y(bounding_size.height() * sign);
  }

  return offset_in_screen_coord_space;
}

}  // namespace

namespace ui {

PhysicsBasedFlingCurve::PhysicsBasedFlingCurve(
    const gfx::Vector2dF& velocity,
    base::TimeTicks start_timestamp,
    const gfx::Vector2dF& pixels_per_inch,
    const float boost_multiplier,
    const gfx::Size& bounding_size)
    : start_timestamp_(start_timestamp),
      p1_(gfx::PointF(kDefaultP1X, kDefaultP1Y)),
      p2_(gfx::PointF(kDefaultP2X, kDefaultP2Y)),
      distance_(CalculateEndPoint(
          pixels_per_inch,
          gfx::ScaleVector2d(velocity, 1 / 1000.0f),
          ScaleToFlooredSize(bounding_size,
                             boost_multiplier * kDefaultBoundsMultiplier))),
      curve_duration_(CalculateDurationAndConfigureControlPoints(velocity)),
      bezier_(p1_.x(), p1_.y(), p2_.x(), p2_.y()),
      previous_time_delta_(base::TimeDelta()) {
  DCHECK(!velocity.IsZero());
  DCHECK(!std::isnan(velocity.x()));
  DCHECK(!std::isnan(velocity.y()));
}

PhysicsBasedFlingCurve::~PhysicsBasedFlingCurve() = default;

bool PhysicsBasedFlingCurve::ComputeScrollOffset(base::TimeTicks time,
                                                 gfx::Vector2dF* offset,
                                                 gfx::Vector2dF* velocity) {
  DCHECK(offset);
  DCHECK(velocity);

  const base::TimeDelta elapsed_time = time - start_timestamp_;
  if (elapsed_time.is_negative()) {
    *offset = gfx::Vector2dF();
    *velocity = gfx::Vector2dF();
    return true;
  }

  const double x = elapsed_time / curve_duration_;
  const bool still_active = x < 1.0f;
  if (still_active) {
    const double progress = bezier_.Solve(x);
    *offset = GetPositionAtValue(distance_, progress);
    *velocity = GetVelocityAtTime(*offset, prev_offset_,
                                  elapsed_time - previous_time_delta_);
    prev_offset_ = *offset;
    previous_time_delta_ = elapsed_time;
  } else {
    // At the end of animation, we should have traveled a distance equal to
    // |distance_|.
    *offset = distance_;
    *velocity = gfx::Vector2dF();
  }

  return still_active;
}

base::TimeDelta
PhysicsBasedFlingCurve::CalculateDurationAndConfigureControlPoints(
    const gfx::Vector2dF& velocity) {
  float fling_velocity = std::max(fabs(velocity.x()), fabs(velocity.y()));
  float duration = std::min(kMaxCurveDurationForFling,
                            (fling_velocity / kDefaultPixelDeceleration));
  float slope = fabs(velocity.y()) == fling_velocity
                    ? fabs(velocity.y() * duration / distance_.y())
                    : fabs(velocity.x() * duration / distance_.x());

  // Configure cubic bezier control points based on initial fling velocity.
  // When matching the initial fling velocity for a Cubic Bezier Curve, scale
  // |p1_.y| up until it reaches 1 After it reaches 1, move |p1_.x| to the left.
  if (slope * p1_.x() < 1.0f) {
    p1_.set_y(p1_.x() * slope);
  } else {
    p1_.set_x(p1_.y() / slope);
  }

  return base::Seconds(duration);
}
}  // namespace ui
