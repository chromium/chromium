/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/loudness/background_level_detector.h"

#include "audio/dsp/decibels.h"
#include "audio/linear_filters/biquad_filter_design.h"

namespace audio_dsp {

using Eigen::ArrayXf;
using Eigen::ArrayXXf;
using linear_filters::FirstOrderCoefficientFromTimeConstant;
using linear_filters::LowpassBiquadFilterCoefficients;

void BackgroundLevelDetector::Init(const BackgroundLevelDetectorParams& params,
                                   int num_channels, float sample_rate_hz) {
  ABSL_CHECK_LT(params_.fast_smoother_cutoff_hz,
           params_.envelope_sample_rate_hz / 2);
  ABSL_CHECK_LT(params_.slow_smoother_cutoff_hz,
           params_.envelope_sample_rate_hz / 2);
  ABSL_CHECK_GT(params_.fast_smoother_cutoff_hz, 0);
  ABSL_CHECK_GT(params_.slow_smoother_cutoff_hz, 0);
  ABSL_CHECK_GT(params_.transient_sensitivity, 0);
  ABSL_CHECK_LE(params_.transient_sensitivity, 1);

  num_channels_ = num_channels;
  sample_rate_hz_ = sample_rate_hz;
  params_ = params;
  background_previous_samples_ = ArrayXf::Zero(num_channels_);
  transient_counters_ = ArrayXi::Zero(num_channels_);

  envelope_detector_.Init(
      num_channels_, sample_rate_hz, params_.fast_smoother_cutoff_hz,
      params_.envelope_sample_rate_hz,
      PerceptualLoudnessFilterCoefficients(params_.weighting, sample_rate_hz_));

  // Set the initial values of the smoothing filter.
  constexpr float kQualityFactor = 0.5f;  // Critically damped.
  auto slow_coeffs = LowpassBiquadFilterCoefficients(
      params_.envelope_sample_rate_hz, params_.slow_smoother_cutoff_hz,
      kQualityFactor);
  slow_smoother_.InitFromTransferFunction(num_channels, slow_coeffs.b,
                                          slow_coeffs.a);

  // Set the values of the other parameters.
  SetBackgroundSmootherTimeConstant(params_.background_smoother_time_constant);
  SetTransientSensitivity(params_.transient_sensitivity);
  SetTransientRejectionTime(params_.transient_rejection_time);

  Reset();
}

void BackgroundLevelDetector::Reset() {
  envelope_detector_.Reset();
  burn_in_stage_.assign(num_channels_, true);
  slow_smoother_.Reset();
  background_previous_samples_.setZero();
  fast_envelope_ = Eigen::ArrayXXf(num_channels_, 0);
  background_ = Eigen::ArrayXXf(num_channels_, 0);
  transient_counters_.setZero();
}

bool BackgroundLevelDetector::ProcessBlock(const ArrayXXf& input) {
  // Downsample and get the RMS envelope and compute the fast and slow smoothed
  // RMS envelope.
  if (!envelope_detector_.ProcessBlock(input, &fast_envelope_)) {
    return false;
  }

  slow_smoother_.ProcessBlock(fast_envelope_, &slow_envelope_);
  background_.resizeLike(fast_envelope_);

  // Algorithm overview:
  // First, decide if a transient is currently occurring or if one has ended.
  // A transient is occurring when the fast smoother's value is significantly
  // larger than the slow smoother's value. If the signal becomes small again,
  // the transient state is over.
  //
  // transient_counter_ counts how long the signal has been in a transient
  // state. The counter is turned off (set to zero) when the signal is no
  // longer transient.
  //
  // When the state is transient, we don't update the background estimate
  // because the measured level is not representative of background noise.
  //
  // If state has been transient for too long (i.e., the counter runs out),
  // it is decided that the signal is not a loud transient, but a loud
  // constant noise. This causes the background level estimate to increase
  // until it reaches the new, louder level.
  float sensitivity_factor = 1 / params_.transient_sensitivity;
  const int num_channels = background_.rows();
  const int num_samples = background_.cols();

  for (int i = 0; i < num_samples; ++i) {
    for (int channel = 0; channel < num_channels; ++channel) {
      float background_smoother_coeff = background_coeff_;
      // We use a small tolerance to make sure the difference is significant.
      bool background_exceeds_instantaneous =
          background_previous_samples_(channel) - 1e-4 >
          fast_envelope_(channel, i);
      // Sets the transient_counter_ equal to transient_rejection_time_samples_
      // only in channels where the signal is currently transient.
      if (burn_in_stage_[channel]) {
        // The burn-in stage allows the background smoother, which typically has
        // a very slow time constant to approximate the correct value much
        // faster.
        background_smoother_coeff = burn_in_background_coeff_;
        transient_counters_(channel) = 0;

        // If background exceeds instantaneous, we no longer need burn in
        // because the background is far from it's initial zero-state.
        if (background_exceeds_instantaneous) {
          burn_in_stage_[channel] = false;
        }
      } else if (fast_envelope_(channel, i) >
                 sensitivity_factor * slow_envelope_(channel, i)) {
        transient_counters_(channel) = transient_rejection_time_samples_;

        // If the background estimate exceeds the fast smoother, the transient
        // is over. Stop the counter in those channels.
      } else if (background_exceeds_instantaneous) {
        transient_counters_(channel) = 0;
      }

      // Updates background_ only if the signal in that channel is not currently
      // in a transient period. The signal always smooths towards the minimum
      // of the slow and fast smoother outputs. The rationale is that when the
      // fast signal is smaller, it is a better estimate of the room noise, but
      // when the fast signal is larger it is likely due to a transient.
      if (transient_counters_(channel) == 0) {
        background_previous_samples_(channel) +=
            background_smoother_coeff *
            (std::min(slow_envelope_(channel, i), fast_envelope_(channel, i)) -
             background_previous_samples_(channel));
      } else {
        // Tick the transient counter.
        --transient_counters_(channel);
      }
      background_(channel, i) = background_previous_samples_(channel);
    }
  }
  // Convert to decibels (avoid taking the log of zero).
  AmplitudeRatioToDecibels(fast_envelope_ + 1e-12f, &fast_envelope_);
  AmplitudeRatioToDecibels(background_ + 1e-12f, &background_);
  return true;
}

void BackgroundLevelDetector::SetSlowSmootherCutoffHz(float cutoff_hz) {
  ABSL_CHECK_LT(cutoff_hz, params_.envelope_sample_rate_hz / 2);
  params_.slow_smoother_cutoff_hz = cutoff_hz;
  constexpr float kQualityFactor = 0.5f;
  auto coeffs = LowpassBiquadFilterCoefficients(params_.envelope_sample_rate_hz,
                                                cutoff_hz, kQualityFactor);
  slow_smoother_.ChangeCoeffsFromTransferFunction(coeffs.b, coeffs.a);
}

void BackgroundLevelDetector::SetBackgroundSmootherTimeConstant(
    float time_constant_seconds) {
  ABSL_CHECK_GT(time_constant_seconds,
           1 / (2.0 * M_PI * params_.envelope_sample_rate_hz));
  params_.background_smoother_time_constant = time_constant_seconds;

  constexpr float kBurnInTimeConstantDecreaseFactor = 10.0;
  burn_in_background_coeff_ = FirstOrderCoefficientFromTimeConstant(
      time_constant_seconds / kBurnInTimeConstantDecreaseFactor,
      params_.envelope_sample_rate_hz);
  background_coeff_ = FirstOrderCoefficientFromTimeConstant(
      time_constant_seconds, params_.envelope_sample_rate_hz);
}

void BackgroundLevelDetector::SetTransientSensitivity(float sensitivity) {
  params_.transient_sensitivity = sensitivity;
}

void BackgroundLevelDetector::SetTransientRejectionTime(
    float transient_rejection_time_seconds) {
  params_.transient_rejection_time = transient_rejection_time_seconds;
  transient_rejection_time_samples_ =
      transient_rejection_time_seconds * params_.envelope_sample_rate_hz;
}

}  // namespace audio_dsp
