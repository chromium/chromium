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

#include "audio/dsp/loudness/loudness_1771_two_way_compressor.h"
#include "glog/logging.h"
#include "absl/memory/memory.h"

namespace audio_dsp {

using ::Eigen::ArrayXf;

namespace {

bool VerifyParams(const Loudness1771TwoWayCompressorParams& params,
                  int num_channels, int max_block_size_samples,
                  float sample_rate_hz) {
  bool params_ok = true;
  if (params.attack_s <= 0) {
    LOG(ERROR) << "The attack time constant must be greater than 0s.";
    params_ok = false;
  }

  if (params.release_s <= 0) {
    LOG(ERROR) << "The release time constant must be greater than 0s.";
    params_ok = false;
  }

  if (num_channels <= 0) {
    LOG(ERROR) << "Invalid number of channels provided.";
    params_ok = false;
  }

  if (max_block_size_samples <= 0) {
    LOG(ERROR) << "Invalid maximum block size provided.";
    params_ok = false;
  }

  if (sample_rate_hz <= 0) {
    LOG(ERROR) << "Invalid sample rate provided.";
    params_ok = false;
  }

  return params_ok;
}

}  // namespace

bool Loudness1771TwoWayCompressor::Init(
    const Loudness1771TwoWayCompressorParams& params, int num_channels,
    int max_block_size_samples, float sample_rate_hz) {
  params_ = params;
  if (!VerifyParams(params_, num_channels, max_block_size_samples,
                    sample_rate_hz)) {
    return false;
  }

  num_channels_ = num_channels;
  max_block_size_samples_ = max_block_size_samples;
  sample_rate_hz_ = sample_rate_hz;
  workspace_ = ArrayXf::Zero(max_block_size_samples_);
  workspace_drc_output_ = ArrayXf::Zero(max_block_size_samples_);

  if (!loudness_meter_.Init(params_.streaming_loudness_params, num_channels_,
                            sample_rate_hz_)) {
    return false;
  }
  envelope_ = absl::make_unique<AttackReleaseEnvelope>(
      params_.attack_s, params_.release_s, sample_rate_hz_);

  int lookahead_samples = std::round(params_.lookahead_s * sample_rate_hz_);
  lookahead_delay_.Init(num_channels, lookahead_samples,
                        max_block_size_samples_);

  return true;
}

void Loudness1771TwoWayCompressor::Reset() {
  loudness_meter_.Reset();
  envelope_->Reset();
}

void Loudness1771TwoWayCompressor::ComputeGainFromDetectedSignal(
    VectorType* data_ptr) {
  // Most of the computation happens in-place on *data_ptr.
  VectorType& data = *data_ptr;
  for (int i = 0; i < data.rows(); ++i) {
    // Occasionally a negative will come up due to numerical imprecision.
    // Apply attack/release smoothing.
    data[i] = std::max(1e-12f, envelope_->Output(data[i]));
  }

  PowerRatioToDecibels(data, &data);

  // Store the gain computation in temporary workspace in order to preserve the
  // loudness measurements for computing gain changes. A second workspace
  // variable is needed because data is actually workspace_ under typical use.
  // Note that the following line is creating a block type and not allocating
  // something significant.
  VectorType gain_computation_workspace =
      workspace_drc_output_.head(data.size());

  // Compute the compressed signal envelope.
  OutputLevelTwoWayCompressor(data + params_.input_gain_db,
                              params_.two_way_compression_params,
                              &gain_computation_workspace);
  // Compute only the gain changes.
  gain_computation_workspace -= data;
  gain_computation_workspace += params_.output_gain_db;

  DecibelsToAmplitudeRatio(gain_computation_workspace, &data /* linear gain */);
  ABSL_DCHECK(data.allFinite());
}

}  // namespace audio_dsp
