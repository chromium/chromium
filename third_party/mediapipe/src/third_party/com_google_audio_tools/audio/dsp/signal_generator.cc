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

#include "audio/dsp/signal_generator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "glog/logging.h"

namespace audio_dsp {

using ::Eigen::ArrayXf;

ArrayXf GenerateSineEigen(int num_samples, float sample_rate, float frequency,
                          float amplitude, float phase_begin) {
  const float kRadiansPerSample = 2 * M_PI * frequency / sample_rate;
  ArrayXf signal =
      (ArrayXf::LinSpaced(num_samples, 0, num_samples - 1) * kRadiansPerSample -
       phase_begin)
          .sin() *
      amplitude;
  return signal;
}

namespace internal {

linear_filters::BiquadFilterCascadeCoefficients PinkNoiseCoeffs(
    int* t60_samples) {
  // This code uses the familiar DSP variables B and A for the expanded filter
  // coefficients and Z, P, and K to designate the zeros, poles, and gain term
  // in the roots representation.
  linear_filters::BiquadFilterCascadeCoefficients coeffs;
  // Generate biquad sections from the third order polynomials.
  // https://ccrma.stanford.edu/~jos/sasp/Example_Synthesis_1_F_Noise.html
  std::vector<double> B = {0.049922035, -0.095993537, 0.050612699,
                           -0.004408786};
  std::vector<double> A = {1.0, -2.494956002, 2.017265875, -0.522189400};
  double K = B[0];
  // Normalize B.
  for (int i = 0; i < B.size(); ++i) {
    B[i] /= K;
  }

  std::vector<long double> Z(3);
  std::vector<long double> P(3);
  ABSL_CHECK(MathUtil::RealRootsForCubic(B[1], B[2], B[3], &Z[0], &Z[1], &Z[2]));
  ABSL_CHECK(MathUtil::RealRootsForCubic(A[1], A[2], A[3], &P[0], &P[1], &P[2]));
  // Same descending order as returned by Octave's roots function.
  auto sort_fun = [](long double a, long double b) { return a > b; };
  std::sort(Z.begin(), Z.end(), sort_fun);
  std::sort(P.begin(), P.end(), sort_fun);
  // Verify that for this filter the roots are real.
  std::vector<double> abs_P;
  abs_P.reserve(P.size());
  for (double root : P) {
    abs_P.push_back(std::abs(root));
  }
  // Three roots, (Z[0] - x)(Z[1] - x)(Z[2] - x), but the
  // Append* functions expect a second order polynomial.
  coeffs.AppendNumerator({K, -K * static_cast<double>(Z[1] + Z[2]),
                           K * static_cast<double>(Z[1] * Z[2])});
  coeffs.AppendDenominator({
      1, -static_cast<double>(P[1] + P[2]), static_cast<double>(P[1] * P[2])});
  // Remaining first order section.
  coeffs.AppendNumerator(std::vector<double>{1, static_cast<double>(-Z[0]), 0});
  coeffs.AppendDenominator(
      std::vector<double>{1, static_cast<double>(-P[0]), 0});
  ABSL_CHECK(coeffs.IsStable());

  // https://ccrma.stanford.edu/~jos/mdft/Audio_Decay_Time_T60.html
  *t60_samples = std::round(
      6.91 / (1.0 - *std::max_element(abs_P.cbegin(), abs_P.cend())));
  ABSL_CHECK_GT(*t60_samples, 0);
  return coeffs;
}

}  // namespace internal
}  // namespace audio_dsp
