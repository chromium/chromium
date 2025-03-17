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

#include "audio/linear_filters/discretization.h"

namespace linear_filters {

using ::std::vector;

namespace {

vector<double> BilinearMap(vector<double> quadratic_poly, double K) {
  // Bilinear mapping as in Wikipedia
  // (see https://en.wikipedia.org/wiki/Bilinear_transform).
  // Here c0 is for wikipedia's b0*K^2 or a0*K^2, c1 if for a1*K or b1*K,
  // and c2 is a2 or b2.  And K is the domain scaling factor 2/T, or a
  // modification of that to do pre-compensation for frequency warping.
  double c0 = quadratic_poly[0] * K * K;
  double c1 = quadratic_poly[1] * K;
  double c2 = quadratic_poly[2];
  // This quadratic has z-plane roots corresponding to the bilinear mapping
  // of the s-plane roots of quadratic_poly, via s = K * (z - 1) / (z + 1).
  return {c0 + c1 + c2, 2 * (c2 - c0), c0 - c1 + c2};
}

// Wikipedia uses K = 2/T (or 2 * sample_rate_hz).
// Our K formula factors in the prewarping to match at match_frequency_hz.
// The mathworks page http://www.mathworks.com/help/signal/ref/bilinear.html
// has this factor most explicitly, after where it says:
// "bilinear can accept an optional parameter Fp that specifies prewarping."
// (interpreting f_z as a typo for f_s there).
double ComputeKForFrequencyMatch(double match_frequency_hz,
                                 double sample_rate_hz) {
  return (match_frequency_hz < sample_rate_hz * 1e-7)
          ? 2 * sample_rate_hz  // Match at low frequencies, no pre-warping.
          : 2 * M_PI * match_frequency_hz /
                tan(M_PI * match_frequency_hz / sample_rate_hz);
}
}  // namespace

BiquadFilterCoefficients BilinearTransform(
    const vector<double>& s_numerator, const vector<double>& s_denominator,
    double sample_rate_hz, double match_frequency_hz) {
  ABSL_CHECK_EQ(s_numerator.size(), 3);
  ABSL_CHECK_EQ(s_denominator.size(), 3);
  ABSL_CHECK_GT(sample_rate_hz, 0.0);
  ABSL_CHECK_GE(match_frequency_hz, 0.0);
  ABSL_CHECK_LT(match_frequency_hz, sample_rate_hz / 2.0);
  double K = ComputeKForFrequencyMatch(match_frequency_hz, sample_rate_hz);
  BiquadFilterCoefficients coeffs(BilinearMap(s_numerator, K),
                                  BilinearMap(s_denominator, K));
  coeffs.Normalize();
  return coeffs;
}

BiquadFilterCoefficients BilinearTransform(
    const std::vector<double>& s_numerator,
    const std::vector<double>& s_denominator,
    double sample_rate_hz) {
  return BilinearTransform(s_numerator, s_denominator, sample_rate_hz, 0);
}

FilterPolesAndZeros BilinearTransform(const FilterPolesAndZeros& analog_filter,
                                      double sample_rate_hz,
                                      double match_frequency_hz) {
  FilterPolesAndZeros discretized_filter;
  double K = ComputeKForFrequencyMatch(match_frequency_hz, sample_rate_hz);
  // Apply bilinear transform to poles and zeros.
  for (double pole : analog_filter.GetRealPoles()) {
    discretized_filter.AddPole((K + pole) / (K - pole));
  }
  for (const std::complex<double>& pole : analog_filter.GetConjugatedPoles()) {
    discretized_filter.AddConjugatePolePair((K + pole) / (K - pole));
  }
  for (double zero : analog_filter.GetRealZeros()) {
    discretized_filter.AddZero((K + zero) / (K - zero));
  }
  for (const std::complex<double>& zero : analog_filter.GetConjugatedZeros()) {
    discretized_filter.AddConjugateZeroPair((K + zero) / (K - zero));
  }
  // If there are more poles than zeros, add zeros at Nyquist.
  int relative_degree = analog_filter.RelativeDegree();
  for (int i = 0; i < relative_degree; ++i) {
    discretized_filter.AddZero(-1.0);
  }
  // TODO: The gains are set the same way they are in scipy (at least
  // in the case where there is no warping). It is not clear to me why this
  // frequency (K) was chosen.
  discretized_filter.SetGain(discretized_filter.GetGain() *
                             std::abs(analog_filter.Eval(K)));

  return discretized_filter;
}

FilterPolesAndZeros BilinearTransform(const FilterPolesAndZeros& analog_filter,
                                      double sample_rate_hz) {
  return BilinearTransform(analog_filter, sample_rate_hz, 0);
}


}  // namespace linear_filters
