// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/kalman_predictor.h"

#include <algorithm>
#include <cmath>

#include "base/numerics/math_constants.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace {

// Influence of acceleration during each prediction sample
constexpr float kAccelerationInfluence = 0.5f;
// Influence of velocity during each prediction sample
constexpr float kVelocityInfluence = 1.0f;

}  // namespace

namespace ui {

constexpr base::TimeDelta InputPredictor::kMaxTimeDelta;
constexpr base::TimeDelta InputPredictor::kMaxResampleTime;
constexpr base::TimeDelta InputPredictor::kMaxPredictionTime;
constexpr base::TimeDelta InputPredictor::kTimeInterval;
constexpr base::TimeDelta InputPredictor::kMinTimeInterval;
constexpr base::TimeDelta KalmanPredictor::kMaxTimeInQueue;

KalmanPredictor::KalmanPredictor(unsigned int prediction_options)
    : prediction_options_(prediction_options) {}

KalmanPredictor::~KalmanPredictor() = default;

const char* KalmanPredictor::GetName() const {
  return input_prediction::kScrollPredictorNameKalman;
}

void KalmanPredictor::Reset() {
  x_predictor_.Reset();
  y_predictor_.Reset();
  last_points_.clear();
  time_filter_.Reset();
}

void KalmanPredictor::Update(const InputData& cur_input) {
  base::TimeDelta dt;
  if (last_points_.size()) {
    // When last point is kMaxTimeDelta away, consider it is incontinuous.
    dt = cur_input.time_stamp - last_points_.back().time_stamp;
    if (dt > kMaxTimeDelta)
      Reset();
    else
      time_filter_.Update(dt.InMillisecondsF(), 0);
  }

  double dt_ms = time_filter_.GetPosition();
  last_points_.push_back(cur_input);
  x_predictor_.Update(cur_input.pos.x(), dt_ms);
  y_predictor_.Update(cur_input.pos.y(), dt_ms);

  while (last_points_.back().time_stamp - last_points_.front().time_stamp >
         kMaxTimeInQueue) {
    last_points_.pop_front();
  }
}

bool KalmanPredictor::HasPrediction() const {
  return x_predictor_.Stable() && y_predictor_.Stable();
}

std::unique_ptr<InputPredictor::InputData> KalmanPredictor::GeneratePrediction(
    base::TimeTicks predict_time) const {
  if (!HasPrediction())
    return nullptr;

  DCHECK(last_points_.size());
  float pred_dt =
      (predict_time - last_points_.back().time_stamp).InMillisecondsF();

  gfx::PointF position(last_points_.back().pos.x(),
                       last_points_.back().pos.y());
  gfx::Vector2dF velocity = PredictVelocity();
  gfx::Vector2dF acceleration = PredictAcceleration();

  if (prediction_options_ & kDirectionCutOffEnabled) {
    gfx::Vector2dF future_velocity =
        velocity + ScaleVector2d(acceleration, pred_dt);
    if (gfx::DotProduct(velocity, future_velocity) <= 0)
      return nullptr;
  }

  position += ScaleVector2d(velocity, kVelocityInfluence * pred_dt);

  if (prediction_options_ & kHeuristicsEnabled) {
    float points_angle = 0.0f;
    for (size_t i = 2; i < last_points_.size(); i++) {
      gfx::Vector2dF first_dir =
          last_points_[i - 1].pos - last_points_[i - 2].pos;
      gfx::Vector2dF second_dir = last_points_[i].pos - last_points_[i - 1].pos;
      if (first_dir.Length() && second_dir.Length()) {
        points_angle += atan2(first_dir.x(), first_dir.y()) -
                        atan2(second_dir.x(), second_dir.y());
      }
    }
    if (fabsf(points_angle) * 180 / base::kPiDouble > 15) {
      position += ScaleVector2d(acceleration,
                                kAccelerationInfluence * pred_dt * pred_dt);
    }
  } else {
    position +=
        ScaleVector2d(acceleration, kAccelerationInfluence * pred_dt * pred_dt);
  }

  return std::make_unique<InputData>(position, predict_time);
}

base::TimeDelta KalmanPredictor::TimeInterval() const {
  return time_filter_.GetPosition()
             ? std::max(kMinTimeInterval, base::TimeDelta::FromMilliseconds(
                                              time_filter_.GetPosition()))
             : kTimeInterval;
}

gfx::Vector2dF KalmanPredictor::PredictPosition() const {
  return gfx::Vector2dF(x_predictor_.GetPosition(), y_predictor_.GetPosition());
}

gfx::Vector2dF KalmanPredictor::PredictVelocity() const {
  return gfx::Vector2dF(x_predictor_.GetVelocity(), y_predictor_.GetVelocity());
}

gfx::Vector2dF KalmanPredictor::PredictAcceleration() const {
  return gfx::Vector2dF(x_predictor_.GetAcceleration(),
                        y_predictor_.GetAcceleration());
}

}  // namespace ui
