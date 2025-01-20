/*
 * Copyright 2020-2021 Google LLC
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

#include "audio/dsp/envelope_detector.h"

#include "audio/linear_filters/biquad_filter_design.h"

namespace audio_dsp {

using ::Eigen::ArrayXf;
using ::Eigen::ArrayXXf;

void EnvelopeDetector::Init(
    int num_channels, float sample_rate_hz, float envelope_cutoff_hz,
    float envelope_sample_rate_hz,
    const linear_filters::BiquadFilterCascadeCoefficients& coeffs) {
  ABSL_CHECK_LE(envelope_sample_rate_hz, sample_rate_hz);
  ABSL_CHECK_LT(envelope_cutoff_hz, envelope_sample_rate_hz / 2);
  ABSL_CHECK_GT(envelope_cutoff_hz, 0);

  ABSL_CHECK_GT(num_channels, 0);
  num_channels_ = num_channels;
  sample_rate_hz_ = sample_rate_hz;
  envelope_cutoff_hz_ = envelope_cutoff_hz;
  envelope_sample_rate_hz_ = envelope_sample_rate_hz;
  most_recent_output_ = ArrayXf::Zero(num_channels_);

  prefilter_.Init(num_channels, coeffs);

  // Smoother filter.
  constexpr double kOverdamped = 0.5;
  linear_filters::BiquadFilterCoefficients smoother_coeffs_ =
      linear_filters::LowpassBiquadFilterCoefficients(
          sample_rate_hz_, envelope_cutoff_hz_, kOverdamped);
  envelope_smoother_.Init(num_channels, smoother_coeffs_);

  QResamplerParams downsampler_params;
  downsampler_params.max_denominator = 500;
  downsampler_.Init(sample_rate_hz, envelope_sample_rate_hz, num_channels_,
                    downsampler_params);
}

void EnvelopeDetector::Reset() {
  prefilter_.Reset();
  envelope_smoother_.Reset();
  downsampler_.Reset();
}

bool EnvelopeDetector::ProcessBlock(const ArrayXXf& input, ArrayXXf* output) {
  if (num_channels_ == 0) {
    LOG(WARNING) << "You must initialize!";
    return false;
  }
  // Process with the prefilter.
  prefilter_.ProcessBlock(input, &workspace_);
  // Rectify the signal and smooth to get the RMS envelope.
  workspace_ = workspace_.abs2();
  if (envelope_sample_rate_hz_ == sample_rate_hz_) {
    // Bypass downsampling, which may add a few samples of delay for identity
    // resampling.
    envelope_smoother_.ProcessBlock(workspace_, output);
  } else {
    envelope_smoother_.ProcessBlock(workspace_, &workspace_);
    // Downsample the signal.
    downsampler_.ProcessSamples(workspace_, output);
  }
  // Undo the square to obtain the RMS value.
  *output = output->array().abs().sqrt();
  // Store the most recent output so that we always have a level estimate,
  // even when Process didn't have enough input samples to produce any output
  // samples.
  if (output->cols() > 0) {
    most_recent_output_ = output->col(output->cols() - 1);
  }
  return true;
}

}  // namespace audio_dsp
