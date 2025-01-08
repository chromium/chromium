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

#include "audio/linear_filters/biquad_filter_test_tools.h"

#include "gtest/gtest.h"

#include "audio/linear_filters/biquad_filter.h"

using testing::DoubleEq;
using testing::Not;

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

TEST(BiquadFilterTestToolsTest, EvaluatesMagnitudeResponseCorrectly) {
  BiquadFilterCoefficients test_coefficients = {{1.0, 2.0, 1.0},
                                                {1.0, -1.0, 1.0}};
  // Evaluate at z^-1 = 1.
  //
  //   1z^-2 + 2z^-1 + 1     1 + 2 + 1
  //  ------------------- = ----------- = 4
  //   1z^-2 - 1z^-1 + 1     1 - 1 + 1
  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(4.0), 0.0, 1.0));
  EXPECT_THAT(test_coefficients, PhaseResponseIs(DoubleEq(0.0), 0.0, 1.0));

  // Evaluate at z^-1 = j, one fourth of the sampling rate.
  //
  //   1z^-2 + 2z^-1 + 1     -1 + 2j + 1     2j
  //  ------------------- = ------------- = ---- = -2, a magnitude of 2.
  //   1z^-2 - 1z^-1 + 1     -1 - 1j + 1    -1j
  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(2.0), 0.25, 1.0));
  EXPECT_THAT(test_coefficients, PhaseResponseIs(DoubleEq(0.0), 0.0, 1.0));

  // Evaluate at z^-1 = -1, the Nyquist frequency.
  //
  //   1z^-2 + 2z^-1 + 1     1 - 2 + 1
  //  ------------------- = ------------- = 0.
  //   1z^-2 - 1z^-1 + 1     1 + 1 + 1
  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(0.0), 0.5, 1.0));
}

TEST(BiquadFilterTestToolsTest, EvaluatesMagnitudeResponseCorrectlyCascade) {
  // Same filter as above, but cascaded with itself. The magnitude response
  // of the cascade is the square of each individual filter.
  std::vector<BiquadFilterCoefficients> coeffs_vector = {
      {{1.0, 2.0, 1.0}, {1.0, -1.0, 1.0}}, {{1.0, 2.0, 1.0}, {1.0, -1.0, 1.0}}};
  BiquadFilterCascadeCoefficients test_coefficients(coeffs_vector);

  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(16.0), 0.0, 1.0));
  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(4.0), 0.25, 1.0));
  EXPECT_THAT(test_coefficients, MagnitudeResponseIs(DoubleEq(0.0), 0.5, 1.0));
}

TEST(BiquadFilterTestToolsTest, MonotonicityTest) {
  // The continuous time differentiator, s, is discretized via the bilinear
  // transform as
  //            2 (1 - z^-1)
  // H(z^-1) = --------------.
  //            T (1 + z^-1)
  // The differentiator is monotonically increasing as a function of frequency,
  // and since the bilinear transform preserves the magnitude response, we
  // expect it to do the same. We choose our sampling rate such that T = 1.
  BiquadFilterCoefficients differentiator = {{2.0, -2.0, 0.0}, {1.0, 1.0, 0.0}};
  EXPECT_THAT(differentiator, MagnitudeResponseIncreases(0.01, 0.49,
                                                         1.0, 1000));
  EXPECT_THAT(differentiator, Not(MagnitudeResponseDecreases(0.01, 0.49,
                                                             1.0, 1000)));

  // The continuous time integrator, 1/s, is discretized via the bilinear
  // transform as
  //            T (1 + z^-1)
  // H(z^-1) = --------------.
  //            2 (1 - z^-1)
  // The integrator is monotonically decreasing as a function of frequency,
  // and since the bilinear transform preserves the magnitude response, we
  // expect the discretized filter to do the same. We choose our sampling rate
  // such that T = 1.
  BiquadFilterCoefficients integrator = {{1.0, 1.0, 0.0}, {2.0, -2.0, 0.0}};
  EXPECT_THAT(integrator, MagnitudeResponseDecreases(0.01, 0.49, 1.0, 1000));
  EXPECT_THAT(integrator, Not(MagnitudeResponseIncreases(0.01, 0.49,
                                                         1.0, 1000)));
}

}  // namespace
}  // namespace linear_filters
