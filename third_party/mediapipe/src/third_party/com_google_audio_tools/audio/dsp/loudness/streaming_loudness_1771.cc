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

#include "audio/dsp/loudness/streaming_loudness_1771.h"
#include "audio/linear_filters/perceptual_filter_design.h"

namespace audio_dsp {

using ::linear_filters::BiquadFilterCascadeCoefficients;
using ::linear_filters::kKWeighting;
using ::linear_filters::PerceptualLoudnessFilterCoefficients;

bool StreamingLoudness1771::Init(const StreamingLoudnessParams& params,
                                 int num_channels, float sample_rate_hz) {
  params_ = params;

  if (params.channel_weights.size() != num_channels) {
    LOG(ERROR) << "The number of channel weights does not match the number of "
                  "channels.";
    return false;
  }

  BiquadFilterCascadeCoefficients perceptual_filter_coefficients =
      PerceptualLoudnessFilterCoefficients(kKWeighting, sample_rate_hz);

  k_weighting_filter_.Init(num_channels, perceptual_filter_coefficients);
  averaging_filter_.Init(1, params_.averaging_coefficients);

  return true;
}

void StreamingLoudness1771::Reset() {
  k_weighting_filter_.Reset();
  averaging_filter_.Reset();
}

}  // namespace audio_dsp
