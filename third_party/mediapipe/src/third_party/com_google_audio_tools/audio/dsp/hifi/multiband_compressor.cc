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

#include "audio/dsp/hifi/multiband_compressor.h"

namespace audio_dsp {

using ::Eigen::ArrayXXf;
using ::Eigen::Map;

MultibandCompressor::MultibandCompressor(
    const MultibandCompressorParams& params)
  : num_channels_(0 /* uninitialized */),
    sample_rate_hz_(0 /* uninitialized */),
    crossover_frequencies_hz_(params.GetCrossoverFrequencies()),
    band_splitter_(params.num_bands(),
                   params.crossover_order(),
                   params.crossover_type()) {
  ABSL_DCHECK_GT(params.num_bands(), 0)
      << "You must configure the MultibandCompressorParams";
  per_band_drc_.reserve(params.num_bands());

  for (auto& drc_params : params.GetDynamicRangeControlParams()) {
    per_band_drc_.emplace_back(drc_params);
  }
}

void MultibandCompressor::Init(int num_channels,
                               int max_block_size_samples,
                               float sample_rate_hz) {
  num_channels_ = num_channels;
  sample_rate_hz_ = sample_rate_hz;
  band_splitter_.Init(num_channels_, sample_rate_hz_,
                      crossover_frequencies_hz_);
  for (auto& drc : per_band_drc_) {
    drc.Init(num_channels_, max_block_size_samples, sample_rate_hz_);
  }
}

void MultibandCompressor::Reset() {
  band_splitter_.Reset();
  for (auto& drc : per_band_drc_) {
    drc.Reset();
  }
}

// Process a block of samples. input is a 2D Eigen array with contiguous
// column-major data, where the number of rows equals GetNumChannels().
void MultibandCompressor::ProcessBlock(const Eigen::ArrayXXf& input,
                                       Eigen::ArrayXXf* output) {
  output->resizeLike(input);
  ProcessBlock(Map<const ArrayXXf>(input.data(), input.rows(), input.cols()),
               Map<ArrayXXf>(output->data(), input.rows(), input.cols()));
}

void MultibandCompressor::ProcessBlock(Map<const ArrayXXf> input,
                                       Map<ArrayXXf> output) {
  // TODO: Do we care about the phase delay introduced by each stage?
  band_splitter_.ProcessBlock(input);
  workspace_.resizeLike(input);
  output.setZero();
  for (int i = 0; i < band_splitter_.num_bands(); ++i) {
    per_band_drc_[i].ProcessBlock(band_splitter_.FilteredOutput(i),
                                  &workspace_);
    output += workspace_;
  }
}

}  // namespace audio_dsp
