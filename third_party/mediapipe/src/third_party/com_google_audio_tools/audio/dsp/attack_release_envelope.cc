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

#include "audio/dsp/attack_release_envelope.h"

#include <cmath>

#include "audio/linear_filters/discretization.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "audio/linear_filters/biquad_filter_design.h"

namespace audio_dsp {

using ::linear_filters::FirstOrderCoefficientFromTimeConstant;
using ::linear_filters::LowpassBiquadFilterCoefficients;

AttackReleaseEnvelope::AttackReleaseEnvelope(
    const AttackReleaseEnvelopeParams& params, float sample_rate_hz)
    :  envelope_(0.0),
       attack_(FirstOrderCoefficientFromTimeConstant(params.attack_s,
                                                     sample_rate_hz)),
       release_(FirstOrderCoefficientFromTimeConstant(params.release_s,
                                                      sample_rate_hz)),
       sample_rate_hz_(sample_rate_hz) {
    constexpr float kOverdamped = 0.49;  // Prevent param oscillations.
    linear_filters::BiquadFilterCoefficients smoothing_coeffs =
        LowpassBiquadFilterCoefficients(sample_rate_hz,
                                        params.interpolation_rate_hz,
                                        kOverdamped);
    constexpr int kOneChannel = 1;
    attack_param_smoother_.Init(kOneChannel, smoothing_coeffs);
    release_param_smoother_.Init(kOneChannel, smoothing_coeffs);
    Reset();
}

void AttackReleaseEnvelope::SetAttackTimeSeconds(float attack_s) {
  attack_ = FirstOrderCoefficientFromTimeConstant(attack_s, sample_rate_hz_);
}

void AttackReleaseEnvelope::SetReleaseTimeSeconds(float release_s) {
  release_ = FirstOrderCoefficientFromTimeConstant(release_s, sample_rate_hz_);
}

float AttackReleaseEnvelope::Output(float input) {
  float rectified = std::abs(input);

  float current_attack;
  attack_param_smoother_.ProcessSample(attack_, &current_attack);
  float current_release;
  release_param_smoother_.ProcessSample(release_, &current_release);
  const float coefficient =
      rectified > envelope_ ? current_attack : current_release;
  envelope_ += coefficient * (rectified - envelope_);

  // TODO: Add more smoothing to account for the slope discontinuity
  // when switching time constants.
  return envelope_;
}

}  // namespace audio_dsp
