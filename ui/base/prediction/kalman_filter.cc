// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/kalman_filter.h"

#include "base/check_op.h"

namespace {
constexpr uint32_t kStableIterNum = 4;

constexpr double kSigmaProcess = 0.01;
constexpr double kSigmaMeasurement = 1.0;

gfx::Matrix3F GetStateTransition(double kDt) {
  gfx::Matrix3F state_transition = gfx::Matrix3F::Zeros();
  // State translation matrix is basic physics.
  // new_pos = pre_pos + v * dt + 1/2 * a * dt^2.
  // new_v = v + a * dt.
  // new_a = a .
  state_transition.set(1.0, kDt, 0.5 * kDt * kDt, 0.0, 1.0, kDt, 0.0, 0.0, 1.0);
  return state_transition;
}

gfx::Matrix3F GetProcessNoise(double kDt) {
  gfx::Vector3dF process_noise(0.5 * kDt * kDt, kDt, 1.0);
  // We model the system noise as noisy force on the pointer.
  // The following matrix describes the impact of that noise on each state.
  gfx::Matrix3F process_noise_covariance = gfx::Matrix3F::FromOuterProduct(
      process_noise, gfx::ScaleVector3d(process_noise, kSigmaProcess));
  return process_noise_covariance;
}

}  // namespace

namespace ui {

KalmanFilter::KalmanFilter()
    : state_estimation_(gfx::Vector3dF()),
      error_covariance_matrix_(gfx::Matrix3F::Identity()),
      state_transition_matrix_(GetStateTransition(1.0)),
      process_noise_covariance_matrix_(GetProcessNoise(1.0)),
      measurement_vector_(gfx::Vector3dF(1.0, 0.0, 0.0)),
      measurement_noise_variance_(kSigmaMeasurement),
      iteration_count_(0) {}

KalmanFilter::~KalmanFilter() = default;

const gfx::Vector3dF& KalmanFilter::GetStateEstimation() const {
  return state_estimation_;
}

bool KalmanFilter::Stable() const {
  return iteration_count_ >= kStableIterNum;
}

void KalmanFilter::Update(double observation, double dt) {
  if (iteration_count_++ == 0) {
    // We only update the state estimation in the first iteration.
    state_estimation_ = gfx::Vector3dF(observation, 0, 0);
    return;
  }
  Predict(dt);
  // Y = z - H * X
  double y =
      observation - gfx::DotProduct(measurement_vector_, state_estimation_);
  // S = H * P * H' + R
  double S = gfx::DotProduct(measurement_vector_,
                             gfx::MatrixProduct(error_covariance_matrix_,
                                                measurement_vector_)) +
             measurement_noise_variance_;
  // K = P * H' * inv(S)
  gfx::Vector3dF kalman_gain = gfx::ScaleVector3d(
      gfx::MatrixProduct(error_covariance_matrix_, measurement_vector_),
      1.0 / S);
  // X = X + K * Y
  state_estimation_ += gfx::ScaleVector3d(kalman_gain, y);
  // I_HK = eye(P) - K * H
  gfx::Matrix3F I_KH =
      gfx::Matrix3F::Identity() -
      gfx::Matrix3F::FromOuterProduct(kalman_gain, measurement_vector_);

  // P = I_KH * P * I_KH' + K * R * K'
  error_covariance_matrix_ =
      gfx::MatrixProduct(gfx::MatrixProduct(I_KH, error_covariance_matrix_),
                         I_KH.Transpose()) +
      gfx::Matrix3F::FromOuterProduct(
          gfx::ScaleVector3d(kalman_gain, measurement_noise_variance_),
          kalman_gain);
}

void KalmanFilter::Reset() {
  state_estimation_ = gfx::Vector3dF();
  error_covariance_matrix_ = gfx::Matrix3F::Identity();
  iteration_count_ = 0;
}

double KalmanFilter::GetPosition() const {
  return GetStateEstimation().x();
}

double KalmanFilter::GetVelocity() const {
  return GetStateEstimation().y();
}

double KalmanFilter::GetAcceleration() const {
  return GetStateEstimation().z();
}

void KalmanFilter::Predict(double dt) {
  DCHECK_GT(iteration_count_, 0u);
  state_transition_matrix_ = GetStateTransition(dt);
  // X = F * X
  state_estimation_ =
      gfx::MatrixProduct(state_transition_matrix_, state_estimation_);
  // P = F * P * F' + Q
  error_covariance_matrix_ =
      gfx::MatrixProduct(gfx::MatrixProduct(state_transition_matrix_,
                                            error_covariance_matrix_),
                         state_transition_matrix_.Transpose()) +
      GetProcessNoise(dt);
}

}  // namespace ui
