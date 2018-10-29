// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/kalman_predictor.h"

namespace {

// Influence of acceleration during each prediction sample
constexpr float kAccelerationInfluence = 0.5f;
// Influence of velocity during each prediction sample
constexpr float kVelocityInfluence = 1.0f;

}  // namespace

namespace ui {

constexpr base::TimeDelta InputPredictor::kMaxTimeDelta;
constexpr base::TimeDelta InputPredictor::kMaxResampleTime;

KalmanPredictor::KalmanPredictor() = default;

KalmanPredictor::~KalmanPredictor() = default;

const char* KalmanPredictor::GetName() const {
  return "Kalman";
}

void KalmanPredictor::Reset() {
  x_predictor_.Reset();
  y_predictor_.Reset();
  last_point_.time_stamp = base::TimeTicks();
}

void KalmanPredictor::Update(const InputData& cur_input) {
  base::TimeDelta dt;
  if (!last_point_.time_stamp.is_null()) {
    // When last point is kMaxTimeDelta away, consider it is incontinuous.
    dt = cur_input.time_stamp - last_point_.time_stamp;
    if (dt > kMaxTimeDelta)
      Reset();
  }

  double dt_ms = dt.InMillisecondsF();
  last_point_ = cur_input;
  x_predictor_.Update(cur_input.pos.x(), dt_ms);
  y_predictor_.Update(cur_input.pos.y(), dt_ms);
}

bool KalmanPredictor::HasPrediction() const {
  return x_predictor_.Stable() && y_predictor_.Stable();
}

bool KalmanPredictor::GeneratePrediction(base::TimeTicks predict_time,
                                         InputData* result) const {
  std::vector<InputData> pred_points;

  base::TimeDelta dt = predict_time - last_point_.time_stamp;
  // Kalman filter is not very good when predicting backwards. Besides,
  // predicting backwards means increasing latency. Thus disable prediction when
  // dt < 0.
  if (!HasPrediction() || dt < base::TimeDelta() || dt > kMaxResampleTime)
    return false;

  gfx::Vector2dF position(last_point_.pos.x(), last_point_.pos.y());
  // gfx::Vector2dF position = PredictPosition();
  gfx::Vector2dF velocity = PredictVelocity();
  gfx::Vector2dF acceleration = PredictAcceleration();

  float dt_ms = dt.InMillisecondsF();
  position +=
      ScaleVector2d(velocity, kVelocityInfluence * dt_ms) +
      ScaleVector2d(acceleration, kAccelerationInfluence * dt_ms * dt_ms);

  result->pos.set_x(position.x());
  result->pos.set_y(position.y());
  return true;
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
