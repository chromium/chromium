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

#include "audio/linear_filters/crossover.h"

#include "audio/linear_filters/biquad_filter_design.h"

namespace linear_filters {
namespace {
void MakeButterworth(int order, float crossover, float sample_rate,
                       BiquadFilterCascadeCoefficients* lowpass,
                       BiquadFilterCascadeCoefficients* highpass) {
  ButterworthFilterDesign filter_design = ButterworthFilterDesign(order);
  *lowpass = filter_design.LowpassCoefficients(sample_rate, crossover);
  *highpass = filter_design.HighpassCoefficients(sample_rate, crossover);
}

// A Linkwitz-Riley filter is made from a cascade of two Butterworth filters of
// half the order.
void MakeLinkwitzRiley(int order, float crossover, float sample_rate,
                       BiquadFilterCascadeCoefficients* lowpass,
                       BiquadFilterCascadeCoefficients* highpass) {
  ABSL_CHECK_EQ(order % 2, 0);
  MakeButterworth(order / 2, crossover, sample_rate, lowpass, highpass);
  const int initial_size = lowpass->size();
  for (int i = 0; i < initial_size; ++i) {
    lowpass->AppendBiquad((*lowpass)[i]);
    highpass->AppendBiquad((*highpass)[i]);
  }
}


}  // namespace

CrossoverFilterDesign::CrossoverFilterDesign(CrossoverType type, int order,
                                             float crossover_frequency_hz,
                                             float sample_rate_hz) {
  ABSL_CHECK_GT(order, 0);
  ABSL_CHECK_LT(0, crossover_frequency_hz);
  ABSL_CHECK_LT(crossover_frequency_hz, sample_rate_hz / 2);

  switch (type) {
    case kButterworth: {
      MakeButterworth(order, crossover_frequency_hz, sample_rate_hz,
                      &lowpass_, &highpass_);
    }
    break;
    case kLinkwitzRiley: {
      MakeLinkwitzRiley(order, crossover_frequency_hz, sample_rate_hz,
                        &lowpass_, &highpass_);
    }
    break;
  }
  // Account for the 180 degrees phase difference between the two filters.
  // https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter
  if ((order - 2) % 4 == 0) {  // Check if order is 2, 6, 10, 14, ...
    highpass_.AdjustGain(-1);
  }
}

}  // namespace linear_filters
