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

#include "audio/linear_filters/perceptual_filter_design.h"

namespace linear_filters {

using ::std::complex;
using ::std::vector;

FilterPolesAndZeros PerceptualLoudnessFilterPolesAndZeros(
    PerceptualWeightingType weighting, float sample_rate_hz) {
  // All zero and pole values are in radians per second. Some of these poles
  // and zeros are in conjugate pairs; they are ordered such that they will
  // appear in the same biquad stage.
  vector<double> poles;
  vector<complex<double>> conjugate_poles;
  vector<double> zeros;
  vector<complex<double>> conjugate_zeros;
  double gain = 1;

  FilterPolesAndZeros zeros_poles_gain;
  // The roots come from:
  // https://en.wikipedia.org/wiki/A-weighting#Transfer_function_equivalent
  switch (weighting) {
    case kAWeighting:
      zeros = {0, 0, 0, 0};
      poles = {-129.4, -129.4, -676.4, -4636.0, -76655.0, -76655.0};
      gain = 7.39705e9;
      break;
    case kBWeighting:
      zeros = {0, 0, 0};
      poles = {-129.4, -129.4, -996.9, -76655.0, -76655.0};
      gain = 5.99185e9;
      break;
    case kCWeighting:
      zeros = {0, 0};
      poles = {-129.4, -129.4, -76655, -76655};
      gain = 5.91797e9;
      break;
    case kDWeighting:
      zeros = {0};
      conjugate_zeros = {{-3266, 5505.2}};
      poles = {-1776.3, -7288.5};
      conjugate_poles = {{-10757, 16512.02}};
      gain = 91104.32;
      break;
    case kRlbWeighting:
      // Found by getting the roots of the polynomial listed in
      // "Evaluation of Objective Loudness Meters".
      zeros = {0, 0};
      poles = {-240, -240};
      gain = 1.0;
      break;
    case kKWeighting:
      // RLB filter in series with a shelf filter.
      zeros = {0, 0};
      poles = {-240, -240};
      conjugate_zeros = {{-5943.129, 5976.7400}};
      conjugate_poles = {{-7471.63, 7534.19}};
      // Chosen to give a gain of 0dB at 500 Hz.
      gain = 1.585;
      break;
  }

  const float sample_rate_rads_per_second = 2 * M_PI * sample_rate_hz;
  const float nyquist_rads_per_second = sample_rate_rads_per_second / 2;
  // Drop the poles and zeros that are above the Nyquist frequency. For every
  // zero or pole we drop, we must compensate for the DC gain by multiplying or
  // dividing by the pole radius.
  for (const auto& zero : zeros) {
    const double magnitude = std::abs(zero);
    if (magnitude < nyquist_rads_per_second) {
      zeros_poles_gain.AddZero(zero);
    } else {
      gain *= magnitude;
    }
  }
  for (const auto& zero : conjugate_zeros) {
    const double magnitude = std::abs(zero);
    if (magnitude < nyquist_rads_per_second) {
      zeros_poles_gain.AddConjugateZeroPair(zero);
    } else {
      gain *= magnitude * magnitude;  // Two conjugate zeros.
    }
  }
  for (const auto& pole : poles) {
    const double magnitude = std::abs(pole);
    if (magnitude < nyquist_rads_per_second) {
      zeros_poles_gain.AddPole(pole);
    } else {
      gain /= magnitude;
    }
  }
  for (const auto& pole : conjugate_poles) {
    const double magnitude = std::abs(pole);
    if (magnitude < nyquist_rads_per_second) {
      zeros_poles_gain.AddConjugatePolePair(pole);
    } else {
      gain /= magnitude * magnitude;  // Two conjugate poles.
    }
  }
  zeros_poles_gain.SetGain(gain);
  return zeros_poles_gain;
}

BiquadFilterCascadeCoefficients PerceptualLoudnessFilterCoefficients(
    PerceptualWeightingType weighting, float sample_rate_hz) {
  // Discretize the zeros and poles.
  return BilinearTransform(
      PerceptualLoudnessFilterPolesAndZeros(weighting, sample_rate_hz),
      sample_rate_hz).GetCoefficients();
}

}  // namespace linear_filters
