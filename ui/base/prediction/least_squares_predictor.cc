// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/least_squares_predictor.h"

#include <algorithm>

#include "ui/base/ui_base_features.h"

namespace ui {

namespace {

// Solve XB = y.
static bool SolveLeastSquares(const gfx::Matrix3F& x,
                              const std::deque<double>& y,
                              gfx::Vector3dF& result) {
  constexpr double kEpsilon = std::numeric_limits<double>::epsilon();

  // return last point if y didn't change.
  if (std::abs(y[0] - y[1]) < kEpsilon && std::abs(y[1] - y[2]) < kEpsilon) {
    result = gfx::Vector3dF(y[2], 0, 0);
    return true;
  }

  gfx::Matrix3F x_transpose = x.Transpose();
  gfx::Matrix3F temp = gfx::MatrixProduct(x_transpose, x).Inverse();

  // Return false if x is singular.
  if (temp == gfx::Matrix3F::Zeros())
    return false;

  result = gfx::MatrixProduct(gfx::MatrixProduct(temp, x_transpose),
                              gfx::Vector3dF(y[0], y[1], y[2]));
  return true;
}

}  // namespace

LeastSquaresPredictor::LeastSquaresPredictor() {}

LeastSquaresPredictor::~LeastSquaresPredictor() {}

const char* LeastSquaresPredictor::GetName() const {
  return features::kPredictorNameLsq;
}

void LeastSquaresPredictor::Reset() {
  x_queue_.clear();
  y_queue_.clear();
  time_.clear();
}

void LeastSquaresPredictor::Update(const InputData& cur_input) {
  if (!time_.empty()) {
    // When last point is kMaxTimeDelta away, consider it is incontinuous.
    if (cur_input.time_stamp - time_.back() > kMaxTimeDelta)
      Reset();
  }

  x_queue_.push_back(cur_input.pos.x());
  y_queue_.push_back(cur_input.pos.y());
  time_.push_back(cur_input.time_stamp);
  if (time_.size() > kSize) {
    x_queue_.pop_front();
    y_queue_.pop_front();
    time_.pop_front();
  }
}

bool LeastSquaresPredictor::HasPrediction() const {
  return time_.size() >= kSize;
}

gfx::Matrix3F LeastSquaresPredictor::GetXMatrix() const {
  gfx::Matrix3F x = gfx::Matrix3F::Zeros();
  double t1 = (time_[1] - time_[0]).InMillisecondsF();
  double t2 = (time_[2] - time_[0]).InMillisecondsF();
  x.set(1, 0, 0, 1, t1, t1 * t1, 1, t2, t2 * t2);
  return x;
}

std::unique_ptr<InputPredictor::InputData>
LeastSquaresPredictor::GeneratePrediction(base::TimeTicks predict_time,
                                          base::TimeDelta frame_interval) {
  if (!HasPrediction())
    return nullptr;

  float pred_dt = (predict_time - time_[0]).InMillisecondsF();

  gfx::Vector3dF b1, b2;
  gfx::Matrix3F time_matrix = GetXMatrix();
  if (SolveLeastSquares(time_matrix, x_queue_, b1) &&
      SolveLeastSquares(time_matrix, y_queue_, b2)) {
    gfx::Vector3dF prediction_time(1, pred_dt, pred_dt * pred_dt);

    return std::make_unique<InputData>(
        gfx::PointF(gfx::DotProduct(prediction_time, b1),
                    gfx::DotProduct(prediction_time, b2)),
        predict_time);
  }
  return nullptr;
}

base::TimeDelta LeastSquaresPredictor::TimeInterval() const {
  if (time_.size() > 1) {
    return std::max(kMinTimeInterval,
                    (time_.back() - time_.front()) / (time_.size() - 1));
  }
  return kTimeInterval;
}

}  // namespace ui
