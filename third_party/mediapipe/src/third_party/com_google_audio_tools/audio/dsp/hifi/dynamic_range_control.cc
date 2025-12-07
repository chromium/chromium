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

#include "audio/dsp/hifi/dynamic_range_control.h"

#include "audio/dsp/decibels.h"
#include "audio/dsp/hifi/dynamic_range_control_functions.h"

namespace audio_dsp {

using ::Eigen::ArrayXf;

namespace {
void VerifyParams(const DynamicRangeControlParams& params) {
  ABSL_CHECK_GE(params.knee_width_db, 0);
  ABSL_CHECK_GT(params.ratio, 0);
  ABSL_CHECK_GT(params.attack_s, 0);
  ABSL_CHECK_GT(params.release_s, 0);
}
}  // namespace

DynamicRangeControl::DynamicRangeControl(
    const DynamicRangeControlParams& initial_params)
      : num_channels_(0),
        sample_rate_hz_(0),
        max_block_size_samples_(0),
        params_(initial_params),
        params_change_needed_(false) {
  VerifyParams(initial_params);
}

void DynamicRangeControl::Init(int num_channels, int max_block_size_samples,
                               float sample_rate_hz) {
  ABSL_CHECK_GT(num_channels, 0);
  ABSL_CHECK_GT(max_block_size_samples, 0);
  ABSL_CHECK_GT(sample_rate_hz, 0);
  num_channels_ = num_channels;
  sample_rate_hz_ = sample_rate_hz;
  max_block_size_samples_ = max_block_size_samples;
  workspace_ = ArrayXf::Zero(max_block_size_samples_);
  workspace_drc_output_ = ArrayXf::Zero(max_block_size_samples_);
  envelope_.reset(new AttackReleaseEnvelope(params_.attack_s,
                                            params_.release_s,
                                            sample_rate_hz_));
  int lookahead_samples = std::round(params_.lookahead_s * sample_rate_hz);
  lookahead_delay_.Init(
      num_channels, lookahead_samples, max_block_size_samples);
}

void DynamicRangeControl::SetDynamicRangeControlParams(
    const DynamicRangeControlParams& params) {
  VerifyParams(params);
  next_params_ = params;
  // Update envelope time constants now. The rest will get interpolated via
  // crossfade.
  envelope_->SetAttackTimeSeconds(next_params_.attack_s);
  envelope_->SetReleaseTimeSeconds(next_params_.release_s);
  params_change_needed_ = true;
  ABSL_CHECK_EQ(params.lookahead_s, params_.lookahead_s)
      << "This parameter cannot be changed without reinitializing the "
         "DynamicRangeControl. Please call Init(...) again.";
}

void DynamicRangeControl::Reset() {
  envelope_->Reset();
  params_ = next_params_;
  params_change_needed_ = false;
}

void DynamicRangeControl::ComputeGainFromDetectedSignal(VectorType* data_ptr) {
  // Most of the computation for this function happens in-place on *data_ptr.
  VectorType& data = *data_ptr;
  for (int i = 0; i < data.rows(); ++i) {
    // Occasionally a negative will come up due to numerical imprecision.
    // Apply attack/release smoothing.
    data[i] = std::max(1e-12f, envelope_->Output(data[i]));
  }
  // Convert to decibels.
  // TODO: Consider downsampling the envelope by an integer factor
  // to 8k or lower.
  if (params_.envelope_type == kRms) {
    PowerRatioToDecibels(data /* rectified envelope */,
                         &data /* signal level in decibels */);
  } else {
    AmplitudeRatioToDecibels(data /* rectified envelope */,
                             &data /* signal level in decibels */);
  }
  // Store the gain computation in the workspace.
  // A second workspace variable is needed because data is actually workspace_
  // under typical use. Note that the following line is creating a block type
  // and not allocating something significant.
  VectorType signal_gain_db = workspace_drc_output_.head(data.size());
  ComputeGainForSpecificDynamicRangeControlType(
      data /* signal level in decibels */, &signal_gain_db);
  signal_gain_db += params_.output_gain_db;

  if (params_change_needed_) {
    params_ = next_params_;
    VectorType interp_signal_gain_db =
        workspace_drc_output_.head(data.size());
    ComputeGainForSpecificDynamicRangeControlType(
        data /* signal level in decibels */, &interp_signal_gain_db);
    interp_signal_gain_db += params_.output_gain_db;
    params_change_needed_ = false;

    const float data_size_inv = 1.0f / data.size();
    float k = 0;
    for (int i = 0; i < data.size(); ++i) {
      k += data_size_inv;
      signal_gain_db[i] += k * (interp_signal_gain_db[i] - signal_gain_db[i]);
    }
  }
  // Convert back to linear.
  DecibelsToAmplitudeRatio(signal_gain_db, &data /* linear gain */);
  ABSL_DCHECK(data.allFinite());
}

void DynamicRangeControl::ComputeGainForSpecificDynamicRangeControlType(
    const VectorType& input_level, VectorType* output_gain) {
  switch (params_.dynamics_type) {
    case kCompressor:
      OutputLevelCompressor(
          input_level + params_.input_gain_db, params_.threshold_db,
          params_.ratio, params_.knee_width_db, output_gain);

      break;
    case kLimiter:
      OutputLevelLimiter(
          input_level + params_.input_gain_db,
          params_.threshold_db, params_.knee_width_db, output_gain);

      break;
    case kExpander:
      OutputLevelExpander(
          input_level + params_.input_gain_db, params_.threshold_db,
          params_.ratio, params_.knee_width_db, output_gain);
      break;
    case kNoiseGate:
      // Avoid actually using infinity, which could cause numerical problems.
      // 1000dB of suppression is way more than enough for any use case.
      constexpr float kInfiniteRatio = 1000;
      OutputLevelExpander(
          input_level + params_.input_gain_db, params_.threshold_db,
          kInfiniteRatio, params_.knee_width_db, output_gain);
      break;
  }
  // Compute the gain to apply rather than the output level.
  *output_gain -= input_level;
}

}  // namespace audio_dsp
