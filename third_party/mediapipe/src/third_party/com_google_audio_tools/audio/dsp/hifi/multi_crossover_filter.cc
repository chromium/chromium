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

#include "audio/dsp/hifi/multi_crossover_filter.h"

namespace audio_dsp {

using linear_filters::CrossoverFilterDesign;
using std::vector;

void MultiCrossoverFilter::Init(
    int num_channels, float sample_rate_hz,
    const std::vector<float>& crossover_frequencies_hz) {
  ABSL_CHECK_EQ(crossover_frequencies_hz.size(), num_bands_ - 1);
  num_channels_ = num_channels;
  sample_rate_hz_ = sample_rate_hz;

  SetCrossoverFrequenciesInternal(crossover_frequencies_hz, true);
  Reset();
}

void MultiCrossoverFilter::SetCrossoverFrequencies(
    const std::vector<float>& crossover_frequencies_hz) {
  SetCrossoverFrequenciesInternal(crossover_frequencies_hz, false);
}

void MultiCrossoverFilter::SetCrossoverFrequenciesInternal(
    const std::vector<float>& crossover_frequencies_hz, bool initial) {
  ABSL_CHECK_EQ(crossover_frequencies_hz.size(), num_bands_ - 1);
  for (int i = 0; i < num_bands_ - 1; ++i) {
    if (i > 0) {
      ABSL_CHECK_GT(crossover_frequencies_hz[i], crossover_frequencies_hz[i - 1]);
    }
    float frequency = crossover_frequencies_hz[i];
    CrossoverFilterDesign crossover(type_, order_, frequency, sample_rate_hz_);
    vector<double> k;
    vector<double> v;
    crossover.GetLowpassCoefficients().AsLadderFilterCoefficients(&k, &v);
    if (initial) {
      lowpass_filters_[i].InitFromLadderCoeffs(num_channels_, k, v);
    } else {
      lowpass_filters_[i].ChangeLadderCoeffs(k, v);
    }
    crossover.GetHighpassCoefficients().AsLadderFilterCoefficients(&k, &v);
    if (initial) {
      highpass_filters_[i].InitFromLadderCoeffs(num_channels_, k, v);
    } else {
      highpass_filters_[i].ChangeLadderCoeffs(k, v);
    }
  }
}

// A four-way crossover would look like this:
// Stage:     2        1        0
// input --> HP_2 -------------------------> Output band 3
//       \-> LP_2 --> HP_1-----------------> Output band 2
//                \-> LP_1 --> HP_0 -------> Output band 1
//                         \-> LP_0 -------> Output band 0
void MultiCrossoverFilter::ProcessBlock(const Eigen::ArrayXXf& input) {
  ABSL_DCHECK_GT(num_bands_, 1);
  const Eigen::ArrayXXf* next_in = &input;
  for (int stage = num_bands_ - 2; stage >= 0; --stage) {
    lowpass_filters_[stage].ProcessBlock(*next_in, &filtered_output_[stage]);
    highpass_filters_[stage].ProcessBlock(
        *next_in, &filtered_output_[stage + 1]);
    next_in = &filtered_output_[stage];
  }
}

}  // namespace audio_dsp
