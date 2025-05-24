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

#include "audio/linear_filters/biquad_filter_coefficients.h"

#include <complex>

#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/biquad_filter.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

using ::absl::StrFormat;
using ::Eigen::Infinity;
using ::std::complex;
using ::std::vector;

using PolePair = std::pair<complex<double>, complex<double>>;

MATCHER_P(PolePairNear, expected_pair, "") {
  constexpr double kTol = 1e-8;
  return std::max(std::abs(arg.first - expected_pair.first),
                  std::abs(arg.second - expected_pair.second)) <= kTol ||
      std::max(std::abs(arg.first - expected_pair.second),
               std::abs(arg.second - expected_pair.first)) <= kTol;
}

// Gray & Markel work through this exact example at the end of their paper.
// Gray & Markel (1973), Digital Lattice and Ladder Filter Synthesis:
TEST(LadderCoeffsTest, ConvertToLadderTest) {
  vector<double> b = {0.0154, 0.0462, 0.0462, 0.0154};
  vector<double> a = {1, -1.990, 1.572, -0.4583};
  vector<double> k;
  vector<double> v;
  MakeLadderCoefficientsFromTransferFunction(b, a, &k, &v);
  vector<double> expected_k = {-0.8755871285, 0.835462647, -0.4583};
  vector<double> expected_v = {0.0856458833, 0.1454909806, 0.076846, 0.0154};
  EXPECT_THAT(v, audio_dsp::FloatArrayNear(expected_v, 1e-8));
  EXPECT_THAT(k, audio_dsp::FloatArrayNear(expected_k, 1e-8));
}

TEST(BiquadFilterCoefficientsTest, GetPoles) {
  {  // The poles of H(z) = 1 / (1 - 0.3 z^-1 + 0.02 z^-2) are z = 0.1, 0.2.
    BiquadFilterCoefficients coeffs = {{1, 0, 0}, {1.0, -0.3, 0.02}};
    EXPECT_THAT(coeffs.GetPoles(), PolePairNear(PolePair(0.1, 0.2)));
    EXPECT_TRUE(coeffs.IsStable());
  }
  {  // Test a complex pair with denominator (25 + z^-2), poles are z = +/-0.2i.
    BiquadFilterCoefficients coeffs = {{1, 0, 0}, {25, 0, 1}};
    EXPECT_THAT(coeffs.GetPoles(), PolePairNear(PolePair({0, -0.2}, {0, 0.2})));
    EXPECT_TRUE(coeffs.IsStable());
  }
  {  // Case that would have degraded precision with a naive quadratic formula
     // implementation: 1 / (1 + 0.1 z^-1 - 1e-9 z^-2) has poles z = -0.1, 1e-8.
    BiquadFilterCoefficients coeffs = {{1, 0, 0}, {1.0, 0.1, -1e-9}};
    EXPECT_THAT(coeffs.GetPoles(), PolePairNear(PolePair(-0.1, 1e-8)));
    EXPECT_TRUE(coeffs.IsStable());
  }
  {  // Test a double pole, (1 - 0.5 z^-1 + 0.0625 z^-2) = (1 - 0.25 z^-1)^2.
    BiquadFilterCoefficients coeffs = {{1, 0, 0}, {1.0, -0.5, 0.0625}};
    EXPECT_THAT(coeffs.GetPoles(), PolePairNear(PolePair(0.25, 0.25)));
    EXPECT_TRUE(coeffs.IsStable());
  }
  {  // Test an unstable filter, (1 - 4 z^-1 + 4 z^-2) = (1 - 2 z^-1)^2.
    BiquadFilterCoefficients coeffs = {{1, 0, 0}, {1.0, -4, 4}};
    EXPECT_THAT(coeffs.GetPoles(), PolePairNear(PolePair(2.0, 2.0)));
    EXPECT_FALSE(coeffs.IsStable());
  }
}

// Note that there are other tests of FindPeakFrequencyRadiansPerSample in
// biquad_filter_test.cc.

TEST(BiquadFilterCoefficientsTest, MaxGainTest) {
  constexpr float kSampleRateHz = 48000.0;
  for (float center_frequency_hz : {1000.0f, 15000.0f}) {
    for (float quality_factor : {2.0f, 10.0f}) {
      BiquadFilterCoefficients coeffs =
          BandpassBiquadFilterCoefficients(kSampleRateHz,
                                          center_frequency_hz,
                                          quality_factor);
      SCOPED_TRACE(StrFormat("Bandstop (Q = %f) with center frequency = %f.",
                             quality_factor, center_frequency_hz));
      EXPECT_NEAR(coeffs.FindPeakFrequencyRadiansPerSample().first,
                  center_frequency_hz * 2 * M_PI / kSampleRateHz, 1e-4);
      EXPECT_NEAR(coeffs.FindPeakFrequencyRadiansPerSample().second,
                  1.0, 1e-4);
    }
  }
  BiquadFilterCoefficients highpass_coeffs =
      HighpassBiquadFilterCoefficients(kSampleRateHz, 400.0f, 0.707f);
  EXPECT_NEAR(highpass_coeffs.FindPeakFrequencyRadiansPerSample().first,
              M_PI, 1e-4);
  EXPECT_NEAR(highpass_coeffs.FindPeakFrequencyRadiansPerSample().second,
              1.0, 1e-4);

  BiquadFilterCoefficients lowpass_coeffs =
      LowpassBiquadFilterCoefficients(kSampleRateHz, 400.0f, 0.707f);
  EXPECT_NEAR(lowpass_coeffs.FindPeakFrequencyRadiansPerSample().first,
              0, 1e-4);
  EXPECT_NEAR(lowpass_coeffs.FindPeakFrequencyRadiansPerSample().second,
              1.0, 1e-4);
}

TEST(BiquadFilterCoefficientsTest, DifferentPeakFindingMethodsMatchTest) {
  BiquadFilterCoefficients coeffs =
          BandpassBiquadFilterCoefficients(48000.0f, 4234.0f, 1.23f);
  std::vector<BiquadFilterCoefficients> all_coeffs = {coeffs};
  BiquadFilterCascadeCoefficients cascade(all_coeffs);
  EXPECT_NEAR(coeffs.FindPeakFrequencyRadiansPerSample().first,
              cascade.FindPeakFrequencyRadiansPerSample().first, 1e-5);
}

// Get the decay time from sample-by-sample examination of the impulse response.
int ReferenceDecayTime(const BiquadFilterCoefficients& coeffs,
                       double decay_db) {
  constexpr int kNumSamples = 250;
  BiquadFilter<float> filter;
  filter.Init(1, coeffs);
  Eigen::VectorXf impulse_response(kNumSamples);
  for (int n = 0; n < kNumSamples; ++n) {
    filter.ProcessSample(n == 0 ? 1.0 : 0.0, &impulse_response[n]);
  }
  float peak = impulse_response.lpNorm<Infinity>();  // Max absolute magnitude.
  float target = peak * std::pow(10.0, decay_db / -20);
  ABSL_CHECK_LT(std::abs(impulse_response[kNumSamples - 1]), target);
  for (int n = kNumSamples - 2; n > 0; --n) {
    if (std::abs(impulse_response[n]) >= target) {
      return n + 1;
    }
  }
  return 0;
}

TEST(BiquadFilterCoefficientsTest, EstimateDecayTime) {
  for (const auto& coeffs : std::vector<BiquadFilterCoefficients>({
      // Commented decay times were computed with ReferenceDecayTime().
      {{1, 2, 1}, {1, 0, 0}},  // 60dB decay time of 3 samples.
      {{1, 0, 0}, {1.0, 0.5, -0.06}},  // 60dB decay time of 14 samples.
      {{1, 0, 0}, {1.0, 1.4, 0.49}},  // 60dB decay time of 28 samples.
      {{1, 0, 0}, {1.0, 1.6, 0.65}},  // 60dB decay time of 39 samples.
      {{1, 0, 0}, {1.0, -0.4, 0.85}},  // 60dB decay time of 85 samples.
      {{1, 0, 0}, {1.0, -0.7, 0.94}}})) {  // 60dB decay time of 221 samples.
    SCOPED_TRACE("coeffs: " + coeffs.ToString());
    for (double decay_db : {20, 30, 40, 60}) {
      SCOPED_TRACE(StrFormat("decay_db: %g", decay_db));
      EXPECT_NEAR(coeffs.EstimateDecayTime(decay_db),
                  ReferenceDecayTime(coeffs, decay_db), 7);
    }
  }
  // Decay time is infinite for an unstable filter.
  EXPECT_EQ(BiquadFilterCoefficients({{1, 0, 0}, {1, 0, -1}})
            .EstimateDecayTime(60), std::numeric_limits<double>::infinity());
}

TEST(BiquadFilterTypedTest, BiquadFilterCoefficientsDefaultConstructor) {
  BiquadFilterCoefficients coefficients;
  EXPECT_THAT(coefficients.b, testing::ElementsAre(1.0, 0.0, 0.0));
  EXPECT_THAT(coefficients.a, testing::ElementsAre(1.0, 0.0, 0.0));
}

TEST(BiquadFilterCascadeCoefficientsTest, AsPolynomialRatioTest) {
  vector<double> b;
  vector<double> a;
  BiquadFilterCascadeCoefficients coeffs;
  coeffs.AppendNumerator({1, 2, 3});
  coeffs.AsPolynomialRatio(&b, &a);
  EXPECT_THAT(b, testing::ElementsAre(1.0, 2.0, 3.0));
  EXPECT_THAT(a, testing::ElementsAre(1.0));

  coeffs.AppendDenominator({2, 4, 6});
  coeffs.AsPolynomialRatio(&b, &a);
  EXPECT_THAT(b, testing::ElementsAre(1.0, 2.0, 3.0));
  EXPECT_THAT(a, testing::ElementsAre(2.0, 4.0, 6.0));

  coeffs.AppendDenominator({1, 1, 0});
  coeffs.AsPolynomialRatio(&b, &a);
  EXPECT_THAT(b, testing::ElementsAre(1.0, 2.0, 3.0));
  EXPECT_THAT(a, testing::ElementsAre(2.0, 6.0, 10.0, 6.0));

  coeffs.AppendNumerator({2, 2, 0});
  coeffs.AsPolynomialRatio(&b, &a);
  EXPECT_THAT(b, testing::ElementsAre(2.0, 6.0, 10.0, 6.0));
  EXPECT_THAT(a, testing::ElementsAre(2.0, 6.0, 10.0, 6.0));

  coeffs.AppendNumerator({2, 0, 2});
  coeffs.AsPolynomialRatio(&b, &a);
  EXPECT_THAT(b, testing::ElementsAre(4.0, 12.0, 24.0, 24.0, 20.0, 12.0));
  EXPECT_THAT(a, testing::ElementsAre(2.0, 6.0, 10.0, 6.0));
}

TEST(BiquadFilterCascadeCoefficientsTest, SimplifyTrivialTest) {
  BiquadFilterCascadeCoefficients coeffs;
  {
    vector<double> b;
    vector<double> a;
    coeffs.AppendNumerator({1, 2, 3});
    coeffs.AsPolynomialRatio(&b, &a);
    EXPECT_THAT(b, testing::ElementsAre(1.0, 2.0, 3.0));
    EXPECT_THAT(a, testing::ElementsAre(1.0));
  }
  {
    // Polynomials with trivial numerators and denominators get combined.
    vector<double> b;
    vector<double> a;
    coeffs.AppendDenominator({1, 3, 1});
    coeffs.AsPolynomialRatio(&b, &a);
    EXPECT_THAT(b, testing::ElementsAre(1.0, 2.0, 3.0));
    EXPECT_THAT(a, testing::ElementsAre(1.0, 3.0, 1.0));
  }
}

TEST(BiquadFilterCascadeCoefficientsTest, FirstOrderTrivialTest) {
  {
    BiquadFilterCascadeCoefficients coeffs;
    // Polynomials with trivial numerators and denominators get combined.
    vector<double> b;
    vector<double> a;
    coeffs.AppendBiquad({{1, 2, 0}, {2, 2, 0}});
    coeffs.AppendBiquad({{1, 3, 0}, {7, 5, 0}});
    coeffs.AsPolynomialRatio(&b, &a);
    EXPECT_EQ(coeffs.size(), 1);  // They get merged into a single biquad.
    EXPECT_THAT(b, testing::ElementsAre(1.0, 5.0, 6.0));
    EXPECT_THAT(a, testing::ElementsAre(14.0, 24.0, 10.0));
  }
  {
    BiquadFilterCascadeCoefficients coeffs;
    // Polynomials with trivial numerators and denominators get combined.
    vector<double> b;
    vector<double> a;
    coeffs.AppendBiquad({{1, 2, 0}, {2, 2, 0}});
    coeffs.AppendBiquad({{1, 2, 1}, {2, 2, 1}});
    coeffs.AppendBiquad({{1, 3, 0}, {7, 5, 0}});
    EXPECT_EQ(coeffs.size(), 2);  // They get merged into a single biquad.
    EXPECT_THAT(coeffs[0].b, testing::ElementsAre(1.0, 5.0, 6.0));
    EXPECT_THAT(coeffs[0].a, testing::ElementsAre(14.0, 24.0, 10.0));
  }
  {
    BiquadFilterCascadeCoefficients coeffs;
    // Polynomials with trivial numerators and denominators get combined.
    vector<double> b;
    vector<double> a;
    coeffs.AppendBiquad({{1, 1, 0}, {1, 0, 0}});
    coeffs.AppendBiquad({{1, 2, 0}, {1, 1, 1}});
    coeffs.AppendBiquad({{1, 3, 1}, {1, 1, 0}});
    coeffs.AppendBiquad({{1, 0, 0}, {2, 2, 0}});
    coeffs.AsPolynomialRatio(&b, &a);
    EXPECT_EQ(coeffs.size(), 2);
    EXPECT_THAT(b, testing::ElementsAre(1.0, 6.0, 12.0, 9.0, 2.0));
    EXPECT_THAT(a, testing::ElementsAre(2.0, 6.0, 8.0, 6.0, 2.0));
  }
}

}  // namespace
}  // namespace linear_filters
