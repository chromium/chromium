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

#include "audio/linear_filters/biquad_filter_design.h"

#include <cmath>
#include <complex>
#include <vector>

#include "audio/dsp/decibels.h"
#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/biquad_filter_test_tools.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {

using ::absl::StrFormat;
using ::audio_dsp::AmplitudeRatioToDecibels;
using ::std::complex;
using ::std::vector;
using ::testing::DoubleNear;
using ::testing::Le;

namespace {

constexpr float kSampleRateHz = 48000.0f;
constexpr float kNyquistHz = kSampleRateHz / 2;
constexpr int kNumPoints = 40;  // Points to check for monotonicity.

// This test verifies that the DC gain of the filter is unity, the
// high frequency gain is zero, and the gain at the corner frequency is equal
// to the quality factor. The response monotonically decreases above the cutoff.
TEST(BiquadFilterDesignTest, LowpassCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float corner_frequency_hz : {10.0f, 100.0f, 1000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f, 10.0f}) {
      BiquadFilterCoefficients coeffs =
          LowpassBiquadFilterCoefficients(kSampleRateHz,
                                          corner_frequency_hz,
                                          quality_factor);
      SCOPED_TRACE(StrFormat("Lowpass (Q = %f) with corner = %f.",
                             quality_factor, corner_frequency_hz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              0.0f, kSampleRateHz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(quality_factor, kTolerance),
                                      corner_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                              kNyquistHz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          corner_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
    }
  }
}

// This test verifies that the high frequency gain of the filter is unity, the
// DC is zero, and the gain at the corner frequency is equal to the quality
// factor. The response monotonically increases below the cutoff.
TEST(BiquadFilterDesignTest, HighpassCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float corner_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f, 10.0f}) {
      BiquadFilterCoefficients coeffs =
           HighpassBiquadFilterCoefficients(kSampleRateHz,
                                            corner_frequency_hz,
                                            quality_factor);
      SCOPED_TRACE(StrFormat("Highpass (Q = %f) with corner = %f.",
                             quality_factor, corner_frequency_hz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                      0.0f, kSampleRateHz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(quality_factor, kTolerance),
                                      corner_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              kNyquistHz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          20.0f, corner_frequency_hz, kSampleRateHz, kNumPoints));
    }
  }
}

// Verifies that the shape of the bandpass filter is such that around the
// center frequency, frequencies are passed with unity gain and away from that
// frequency, we see monotonically decreasing response.
TEST(BiquadFilterDesignTest, BandpassCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float center_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f, 10.0f}) {
      BiquadFilterCoefficients coeffs =
           BandpassBiquadFilterCoefficients(kSampleRateHz,
                                            center_frequency_hz,
                                            quality_factor);
      SCOPED_TRACE(StrFormat("Bandpass (Q = %f) with center frequency = %f.",
                             quality_factor, center_frequency_hz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                      center_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          center_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
    }
  }
}

// Verifies that the shape of the bandstop filter is such that around the
// center frequency, frequencies are blocked, and moving away from that
// frequency we see monotonically increasing response.
TEST(BiquadFilterDesignTest, BandstopCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float center_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f, 10.0f}) {
      BiquadFilterCoefficients coeffs =
          BandstopBiquadFilterCoefficients(kSampleRateHz,
                                           center_frequency_hz,
                                           quality_factor);
      SCOPED_TRACE(StrFormat("Bandstop (Q = %f) with center frequency = %f.",
                             quality_factor, center_frequency_hz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                      center_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          center_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
    }
  }
}

// Verifies that the shape of the bandpass filter is such that around some
// center frequency, frequencies are passed with unity gain and moving away from
// that frequency, we see monotonically decreasing response. These filters are
// specified by their band edges rather than a center frequency and a quality
// factor.
TEST(BiquadFilterDesignTest, RangedBandpassCoefficientsTest) {
  // These loops are a construct to give us reasonable upper and lower cutoffs.
  // The band edges used in this test are:
  //   approximate_center_hz +/- approximate_half bandwidth.
  for (float approximate_center_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (float approximate_half_bandwidth : {1.0f, 15.0f, 50.0f, 200.0f}) {
      if (approximate_half_bandwidth > approximate_center_hz / 2) { continue; }

      const float lower_band_edge_hz =
          approximate_center_hz - approximate_half_bandwidth;
      const float upper_band_edge_hz =
          approximate_center_hz + approximate_half_bandwidth;
      BiquadFilterCoefficients coeffs =
          RangedBandpassBiquadFilterCoefficients(kSampleRateHz,
                                                 lower_band_edge_hz,
                                                 upper_band_edge_hz);
      SCOPED_TRACE(
          StrFormat("Ranged bandpass with approximate center = %f and "
                    "half bandwidth %f",
                    approximate_center_hz, approximate_half_bandwidth));

      // The actual center of the filter is located near the geometric mean of
      // the cutoff specifications.
      const float better_approximation_center =
          std::sqrt(lower_band_edge_hz * upper_band_edge_hz);

      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(0.95, 0.051),
                                              better_approximation_center,
                                              kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          lower_band_edge_hz, better_approximation_center,
          kSampleRateHz, kNumPoints));
      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          better_approximation_center, upper_band_edge_hz,
          kSampleRateHz, kNumPoints));
    }
  }
}

// Verifies that the shape of the bandstop filter is such that around some
// center frequency, frequencies are blocked (near zero gain) and away from
// that frequency, we see monotonically increasing response. These filters are
// specified by their band edges rather than a center frequency and a quality
// factor.
TEST(BiquadFilterDesignTest, RangedBandstopCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  std::vector<double> coeffs_b, coeffs_a;
  // These loops are a construct to give us reasonable upper and lower cutoffs.
  // The band edges used in this test are:
  //   approximate_center_hz +/- approximate_half bandwidth.
  for (float approximate_center_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (float approximate_half_bandwidth : {1.0f, 15.0f, 50.0f, 200.0f}) {
      if (approximate_half_bandwidth > approximate_center_hz / 2) { continue; }

      const float lower_band_edge_hz =
          approximate_center_hz - approximate_half_bandwidth;
      const float upper_band_edge_hz =
          approximate_center_hz + approximate_half_bandwidth;
      BiquadFilterCoefficients coeffs =
          RangedBandstopBiquadFilterCoefficients(kSampleRateHz,
                                                 lower_band_edge_hz,
                                                 upper_band_edge_hz);
      SCOPED_TRACE(
          StrFormat("Ranged bandstop with approximate center = %f and "
                    "half bandwidth %f",
                    approximate_center_hz, approximate_half_bandwidth));

      // The actual center of the filter is located near the geometric mean of
      // the cutoff specifications.
      const float better_approximation_center =
          std::sqrt(lower_band_edge_hz * upper_band_edge_hz);

      ASSERT_THAT(coeffs, MagnitudeResponseIs(Le(0.01),
                                              better_approximation_center,
                                              kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              0.0f, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              kNyquistHz, kSampleRateHz));

      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          lower_band_edge_hz, better_approximation_center,
          kSampleRateHz, kNumPoints));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          better_approximation_center, upper_band_edge_hz,
          kSampleRateHz, kNumPoints));
    }
  }
}

// Verifies that the shape of the low shelf filter is such that below the
// corner frequency, the gain is set to a specified constant, and above that
// frequency, the gain is unity (with some transition region.
TEST(BiquadFilterDesignTest, LowShelfCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float corner_frequency_hz : {500.0f, 2000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 2.0f}) {
      for (float gain : {0.5, 2.0}) {
        BiquadFilterCoefficients coeffs =
            LowShelfBiquadFilterCoefficients(kSampleRateHz,
                                             corner_frequency_hz,
                                             quality_factor,
                                             gain);
        SCOPED_TRACE(StrFormat(
            "LowShelf (Q = %f) with center frequency = %f and gain %f.",
            quality_factor, corner_frequency_hz, gain));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                        kSampleRateHz / 2, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(std::sqrt(gain), kTolerance),
                                        corner_frequency_hz, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(gain, kTolerance),
                                        0, kSampleRateHz));
        if (quality_factor < 1 / M_SQRT2) {
          if (gain < 1.0) {
            ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
                20.0f, kSampleRateHz / 2, kSampleRateHz, kNumPoints));
          } else {
            ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
                20.0f, kSampleRateHz / 2, kSampleRateHz, kNumPoints));
          }
        }
      }
    }
  }
}

// Verifies that the shape of the high shelf filter is such that above the
// center frequency, the gain is set to a specified constant, and below that
// frequency, the gain is unity (with some transition region.
TEST(BiquadFilterDesignTest, HighShelfCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float center_frequency_hz : {500.0f, 2000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f}) {
      for (float gain : {0.5, 2.0}) {
        BiquadFilterCoefficients coeffs =
            HighShelfBiquadFilterCoefficients(kSampleRateHz,
                                              center_frequency_hz,
                                              quality_factor,
                                              gain);
        SCOPED_TRACE(StrFormat(
            "LowShelf (Q = %f) with center frequency = %f and gain %f.",
            quality_factor, center_frequency_hz, gain));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                        0, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(std::sqrt(gain), kTolerance),
                                        center_frequency_hz, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(gain, kTolerance),
                                        kSampleRateHz / 2, kSampleRateHz));
        if (quality_factor < 1/ M_SQRT2) {
          if (gain < 1.0) {
            ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
                20.0f, kSampleRateHz / 2, kSampleRateHz, kNumPoints));
          } else {
            ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
                20.0f, kSampleRateHz / 2, kSampleRateHz, kNumPoints));
          }
        }
      }
    }
  }
}

// Verifies that the shape of the peak filter is such that at the
// center frequency, the gain is set to a specified constant, and away from that
// frequency, the gain is unity (with some transition region.
TEST(BiquadFilterDesignTest, ParametricPeakCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float center_frequency_hz : {500.0f, 2000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f}) {
      for (float gain : {0.5, 2.0}) {
        BiquadFilterCoefficients coeffs =
            ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                   center_frequency_hz,
                                                   quality_factor,
                                                   gain);
        SCOPED_TRACE(
            StrFormat("Parametric filter (Q = %f) with center frequency = %f "
                      "and gain %f.",
                      quality_factor, center_frequency_hz, gain));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                        0, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(gain, kTolerance),
                                        center_frequency_hz, kSampleRateHz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                        kSampleRateHz / 2, kSampleRateHz));
        if (gain < 1.0) {
          ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
              20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
          ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
              center_frequency_hz, kSampleRateHz / 2, kSampleRateHz,
              kNumPoints));
        } else {
          ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
              20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
          ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
              center_frequency_hz, kSampleRateHz / 2, kSampleRateHz,
              kNumPoints));
        }
      }
    }
  }
}

// Make sure the filters get narrower as the Q is increased.
TEST(BiquadFilterDesignTest, ParametricPeakCoefficientsQualityFactorTest) {
  constexpr float kCenterFrequency = 1000.0f;
  constexpr float kDefaultQualityFactor = 1.0f;
  {
    float gain = 3.0;
    BiquadFilterCoefficients coeffs_narrow =
              ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                     kCenterFrequency,
                                                     3 * kDefaultQualityFactor,
                                                     gain);
    BiquadFilterCoefficients coeffs_wide =
              ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                     kCenterFrequency,
                                                     kDefaultQualityFactor,
                                                     gain);
    for (float f = 100; f < 600; f += 10) {
      EXPECT_LT(coeffs_narrow.GainMagnitudeAtFrequency(kCenterFrequency - f,
                                                       kSampleRateHz),
                coeffs_wide.GainMagnitudeAtFrequency(kCenterFrequency - f,
                                                     kSampleRateHz));
      EXPECT_LT(coeffs_narrow.GainMagnitudeAtFrequency(kCenterFrequency + f,
                                                       kSampleRateHz),
                coeffs_wide.GainMagnitudeAtFrequency(kCenterFrequency + f,
                                                     kSampleRateHz));
    }
  }
  {
    float gain = 0.3;
    BiquadFilterCoefficients coeffs_narrow =
              ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                     kCenterFrequency,
                                                     3 * kDefaultQualityFactor,
                                                     gain);
    BiquadFilterCoefficients coeffs_wide =
              ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                     kCenterFrequency,
                                                     kDefaultQualityFactor,
                                                     gain);
    for (float f = 100; f < 600; f += 10) {
      EXPECT_GT(coeffs_narrow.GainMagnitudeAtFrequency(kCenterFrequency - f,
                                                       kSampleRateHz),
                coeffs_wide.GainMagnitudeAtFrequency(kCenterFrequency - f,
                                                     kSampleRateHz));
      EXPECT_GT(coeffs_narrow.GainMagnitudeAtFrequency(kCenterFrequency + f,
                                                       kSampleRateHz),
                coeffs_wide.GainMagnitudeAtFrequency(kCenterFrequency + f,
                                                     kSampleRateHz));
    }
  }
}

TEST(BiquadFilterDesignTest,
     ParametricPeakCoefficientsVerticalFlipPropertyTest) {
  constexpr double kTolerance = 1e-4;
  for (float center_frequency_hz : {500.0f, 2000.0f, 10000.0f}) {
    for (float quality_factor : {0.707f, 3.0f}) {
      for (float gain : {2.0, 12.0}) {
        ABSL_CHECK_GT(gain, 1.0);  // Gain must be amplification for this test.
        BiquadFilterCoefficients amplified_coeffs =
            ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                   center_frequency_hz,
                                                   quality_factor,
                                                   gain);
        BiquadFilterCoefficients attenuation_coeffs =
            ParametricPeakBiquadFilterCoefficients(kSampleRateHz,
                                                   center_frequency_hz,
                                                   quality_factor,
                                                   1 / gain);
        BiquadFilterCoefficients symmetric_amplified_coeffs =
            ParametricPeakBiquadFilterSymmetricCoefficients(kSampleRateHz,
                                                            center_frequency_hz,
                                                            quality_factor,
                                                            gain);
        BiquadFilterCoefficients symmetric_attenuation_coeffs =
            ParametricPeakBiquadFilterSymmetricCoefficients(kSampleRateHz,
                                                            center_frequency_hz,
                                                            quality_factor,
                                                            1 / gain);
        SCOPED_TRACE(
            StrFormat("Parametric filter comparisons (Q = %f) with center "
                      "frequency = %f and gain %f.",
                      quality_factor, center_frequency_hz, gain));
        // The amplification filters are the same.
        for (float factor = 0.1; factor < 10; factor *= 1.3) {
          ASSERT_EQ(amplified_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz),
                    symmetric_amplified_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz));
        }
        // The attenuation filters are not the same.
        for (float factor = 0.1; factor < 10; factor *= 1.3) {
          ASSERT_NE(attenuation_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz),
                    symmetric_attenuation_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz));
        }
        // The amplified filter and the symmetric attenuation filter are
        // symmetric with respect to gain = 1 on a loglog (Bode) plot.
        for (float factor = 0.1; factor < 10; factor *= 1.3) {
          ASSERT_NEAR(AmplitudeRatioToDecibels(
              amplified_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz)),
                     -AmplitudeRatioToDecibels(
              symmetric_attenuation_coeffs.GainMagnitudeAtFrequency(
                        center_frequency_hz * factor, kSampleRateHz)),
                      kTolerance);
        }
      }
    }
  }
}

// Verifies that the allpass filter is flat across the entire bandwidth.
TEST(BiquadFilterDesignTest, AllpassCoefficients_MagnitudeResponseFlatTest) {
  constexpr double kTolerance = 1e-4;
  for (double corner_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (double phase_delay_radians : {0.1f, 0.3f}) {
      BiquadFilterCoefficients coeffs =
          AllpassBiquadFilterCoefficients(kSampleRateHz,
                                          corner_frequency_hz,
                                          phase_delay_radians);
      SCOPED_TRACE(StrFormat(
          "Allpass with corner_frequency_hz = %f and phase delay = %f.",
          corner_frequency_hz, phase_delay_radians));
      for (int i = 0; i < kNumPoints; ++i) {
        ASSERT_THAT(coeffs, MagnitudeResponseIs(
                                DoubleNear(1.0, kTolerance),
                                i * kSampleRateHz / (2.0 * kNumPoints),
                                kSampleRateHz));
      }
    }
  }
}

TEST(BiquadFilterDesignTest, AllpassCoefficients_PhaseResponseTest) {
  for (double corner_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (double phase_delay_radians : {0.001, 0.1, 1.4, 2.9, -0.72, -3.14}) {
      ASSERT_THAT(AllpassBiquadFilterCoefficients(
                      kSampleRateHz, corner_frequency_hz, phase_delay_radians),
                  PhaseResponseIs(DoubleNear(phase_delay_radians, 1e-3),
                                  corner_frequency_hz, kSampleRateHz));
    }
  }
}

std::string ToString(const complex<double>& value) {
  return absl::StrFormat("%.12g+%.12gj", value.real(), value.imag());
}

// Check that a complex value is within 1e-9 of expected value.
MATCHER_P(ComplexNear, expected, "is near " + ToString(expected)) {
  constexpr double kTol = 1e-9;
  std::complex<double> error = arg - expected;
  return std::abs(std::real(error)) < kTol && std::abs(std::imag(error)) < kTol;
}

ComplexNearMatcherP<std::complex<double>> ComplexNear(
    double expected_real, double expected_imag) {
  return ComplexNear(std::complex<double>(expected_real, expected_imag));
}

TEST(PoleZeroFilterDesignTest, ButterworthAnalogPrototype) {
  // Compare with scipy.signal.butter(n, 1, analog=True, output="zpk").
  // First order.
  FilterPolesAndZeros zpk1 = ButterworthFilterDesign(1).GetAnalogPrototype();
  EXPECT_EQ(zpk1.GetPolesDegree(), 1);
  EXPECT_EQ(zpk1.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk1.GetGain(), 1, 1e-9);
  EXPECT_EQ(zpk1.GetRealPoles()[0], -1.0);
  // Second order.
  FilterPolesAndZeros zpk2 = ButterworthFilterDesign(2).GetAnalogPrototype();
  EXPECT_EQ(zpk2.GetPolesDegree(), 2);
  EXPECT_EQ(zpk2.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk2.GetGain(), 1, 1e-9);
  EXPECT_THAT(zpk2.GetConjugatedPoles()[0],
              ComplexNear(-0.707106781187, 0.707106781187));
  // Third order.
  FilterPolesAndZeros zpk3 = ButterworthFilterDesign(3).GetAnalogPrototype();
  EXPECT_EQ(zpk3.GetPolesDegree(), 3);
  EXPECT_EQ(zpk3.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk3.GetGain(), 1, 1e-9);
  EXPECT_EQ(zpk3.GetRealPoles()[0], -1.0);
  EXPECT_THAT(zpk3.GetConjugatedPoles()[0], ComplexNear(-0.5, 0.866025403784));
  // Seventh order.
  FilterPolesAndZeros zpk7 = ButterworthFilterDesign(7).GetAnalogPrototype();
  EXPECT_EQ(zpk7.GetPolesDegree(), 7);
  EXPECT_EQ(zpk7.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk7.GetGain(), 1.0, 1e-9);
  EXPECT_EQ(zpk7.GetRealPoles()[0], -1.0);
  EXPECT_THAT(zpk7.GetConjugatedPoles()[0],
              ComplexNear(-0.900968867902, 0.433883739118));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[1],
              ComplexNear(-0.623489801859, 0.781831482468));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[2],
              ComplexNear(-0.222520933956, 0.974927912182));
}

TEST(PoleZeroFilterDesignTest, Chebyshev1AnalogPrototype) {
  // Compare with scipy.signal.cheby1(n, 0.25, 1, analog=True, output="zpk").
  // First order.
  FilterPolesAndZeros zpk1 =
      ChebyshevType1FilterDesign(1, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk1.GetPolesDegree(), 1);
  EXPECT_EQ(zpk1.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk1.GetRealPoles()[0], -4.108111009150, 1e-9);
  EXPECT_NEAR(zpk1.GetGain(), 4.108111009150, 1e-9);
  // Second order.
  FilterPolesAndZeros zpk2 =
      ChebyshevType1FilterDesign(2, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk2.GetPolesDegree(), 2);
  EXPECT_EQ(zpk2.GetZerosDegree(), 0);
  EXPECT_THAT(zpk2.GetConjugatedPoles()[0],
              ComplexNear(-0.898341529763, 1.14324866241));
  EXPECT_NEAR(zpk2.GetGain(), 2.054055504575, 1e-9);
  // Third order.
  FilterPolesAndZeros zpk3 =
      ChebyshevType1FilterDesign(3, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk3.GetPolesDegree(), 3);
  EXPECT_EQ(zpk3.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk3.GetRealPoles()[0], -0.767222665927, 1e-9);
  EXPECT_THAT(zpk3.GetConjugatedPoles()[0],
              ComplexNear(-0.383611332964, 1.09154613477));
  EXPECT_NEAR(zpk3.GetGain(), 1.027027752287, 1e-9);

  // Seventh order.
  FilterPolesAndZeros zpk7 =
      ChebyshevType1FilterDesign(7, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk7.GetPolesDegree(), 7);
  EXPECT_EQ(zpk7.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk7.GetRealPoles()[0], -0.307598675776, 1e-9);
  EXPECT_THAT(zpk7.GetConjugatedPoles()[0],
              ComplexNear(-0.0684471446174, 1.02000802334));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[1],
              ComplexNear(-0.191784637412, 0.817982924742));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[2],
              ComplexNear(-0.277136830682, 0.453946275994));
  EXPECT_NEAR(zpk7.GetGain(), 0.0641892345179, 1e-9);
}

TEST(PoleZeroFilterDesignTest, Chebyshev2AnalogPrototype) {
  // Compare with scipy.signal.cheby2(n, 0.25, 1, analog=True, output="zpk").
  // First order.
  FilterPolesAndZeros zpk1 =
      ChebyshevType2FilterDesign(1, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk1.GetPolesDegree(), 1);
  EXPECT_EQ(zpk1.GetZerosDegree(), 0);
  EXPECT_NEAR(zpk1.GetRealPoles()[0], -4.10811100915, 1e-9);
  EXPECT_NEAR(zpk1.GetGain(), 4.10811100915, 1e-9);
  // Second order.
  FilterPolesAndZeros zpk2 =
      ChebyshevType2FilterDesign(2, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk2.GetPolesDegree(), 2);
  EXPECT_EQ(zpk2.GetZerosDegree(), 2);
  EXPECT_THAT(zpk2.GetConjugatedZeros()[0], ComplexNear(0, 1.414213562373));
  EXPECT_THAT(zpk2.GetConjugatedPoles()[0],
              ComplexNear(-0.16603335596, 1.38408411156));
  EXPECT_NEAR(zpk2.GetGain(), 0.971627951577, 1e-9);
  // Third order.
  FilterPolesAndZeros zpk3 =
      ChebyshevType2FilterDesign(3, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk3.GetPolesDegree(), 3);
  EXPECT_NEAR(zpk3.GetRealPoles()[0], -12.4306769322, 1e-9);
  EXPECT_THAT(zpk3.GetConjugatedZeros()[0], ComplexNear(0, 1.154700538379));
  EXPECT_THAT(zpk3.GetConjugatedPoles()[0],
              ComplexNear(-0.0531719523966, 1.14852055605));
  EXPECT_NEAR(zpk3.GetGain(), 12.3243330274, 1e-9);
  // Seventh order.
  FilterPolesAndZeros zpk7 =
      ChebyshevType2FilterDesign(7, 0.25).GetAnalogPrototype();
  EXPECT_EQ(zpk7.GetPolesDegree(), 7);
  EXPECT_NEAR(zpk7.GetRealPoles()[0], -29.0304010997, 1e-9);
  EXPECT_THAT(zpk7.GetConjugatedZeros()[0], ComplexNear(0, 1.025716863273));
  EXPECT_THAT(zpk7.GetConjugatedZeros()[1], ComplexNear(0, 1.279048007690));
  EXPECT_THAT(zpk7.GetConjugatedZeros()[2], ComplexNear(0, 2.304764870962));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[0],
              ComplexNear(-0.00805435931033, 1.02504557345));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[1],
              ComplexNear(-0.0350677400886, 1.27732709156));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[2],
              ComplexNear(-0.163825398625, 2.29168734868));
  EXPECT_NEAR(zpk7.GetGain(), 28.756777064, 1e-9);
}

// Test elliptic filter analog prototype.
//
// NOTE: We don't compare with scipy.signal.ellip. Its results are accurate to
// only ~4 digits because it does some steps as numeric root finding with a
// hard-coded stopping tolerance of 1e-4. Mathematica's implementation on the
// other hand can execute with arbitrarily high precision.
TEST(PoleZeroFilterDesignTest, EllipticAnalogPrototype) {
  // First order. Compare with Mathematica:
  //   tf = N[EllipticFilterModel[{"Lowpass", {1, 10}, {1/2, 10}}], 24];
  FilterPolesAndZeros zpk1 =
      EllipticFilterDesign(1, 0.5, 10).GetAnalogPrototype();
  EXPECT_EQ(zpk1.GetZerosDegree(), 0);
  EXPECT_EQ(zpk1.GetPolesDegree(), 1);
  EXPECT_NEAR(zpk1.GetRealPoles()[0], -2.862775161243, 1e-9);
  EXPECT_NEAR(zpk1.GetGain(), 2.862775161243, 1e-9);

  // Second order. Compare with Mathematica:
  //   tf = N[EllipticFilterModel[{"Lowpass", {1, 10}, {1/2, 40}}], 24];
  FilterPolesAndZeros zpk2 =
      EllipticFilterDesign(2, 0.5, 40).GetAnalogPrototype();
  EXPECT_EQ(zpk2.GetZerosDegree(), 2);
  EXPECT_EQ(zpk2.GetPolesDegree(), 2);
  EXPECT_THAT(zpk2.GetConjugatedZeros()[0], ComplexNear(0, 11.984640209256));
  EXPECT_THAT(zpk2.GetConjugatedPoles()[0],
              ComplexNear(-0.709000512616, 1.009327180183));
  EXPECT_NEAR(zpk2.GetGain(), 0.01, 1e-9);

  // Third order. Compare with Mathematica:
  //   tf = N[EllipticFilterModel[{"Lowpass", {1, 3}, {1/2, 40}}], 24];
  FilterPolesAndZeros zpk3 =
      EllipticFilterDesign(3, 0.5, 40).GetAnalogPrototype();
  EXPECT_EQ(zpk3.GetZerosDegree(), 2);
  EXPECT_EQ(zpk3.GetPolesDegree(), 3);
  EXPECT_THAT(zpk3.GetConjugatedZeros()[0], ComplexNear(0, 3.103097653306));
  EXPECT_NEAR(zpk3.GetRealPoles()[0], -0.659093088080, 1e-9);
  EXPECT_THAT(zpk3.GetConjugatedPoles()[0],
              ComplexNear(-0.290319112981, 1.030497104301));
  EXPECT_NEAR(zpk3.GetGain(), 0.078454862117, 1e-9);

  // Fourth order. Compare with Mathematica:
  //   tf = N[EllipticFilterModel[{"Lowpass", {1, 2}, {1/2, 40}}], 24];
  FilterPolesAndZeros zpk4 =
      EllipticFilterDesign(4, 0.5, 40).GetAnalogPrototype();
  EXPECT_EQ(zpk4.GetZerosDegree(), 4);
  EXPECT_EQ(zpk4.GetPolesDegree(), 4);
  EXPECT_THAT(zpk4.GetConjugatedZeros()[0], ComplexNear(0, 1.734663423950));
  EXPECT_THAT(zpk4.GetConjugatedZeros()[1], ComplexNear(0, 3.861380228415));
  EXPECT_THAT(zpk4.GetConjugatedPoles()[0],
              ComplexNear(-0.135958718125, 1.021219506553));
  EXPECT_THAT(zpk4.GetConjugatedPoles()[1],
              ComplexNear(-0.453528561667, 0.492009877225));
  EXPECT_NEAR(zpk4.GetGain(), 0.01, 1e-9);

  // Seventh order. Compare with Mathematica:
  //   tf = N[EllipticFilterModel[{"Lowpass", {1, 11/10}, {1/2, 40}}], 24];
  FilterPolesAndZeros zpk7 =
      EllipticFilterDesign(7, 0.5, 40).GetAnalogPrototype();
  EXPECT_EQ(zpk7.GetZerosDegree(), 6);
  EXPECT_EQ(zpk7.GetPolesDegree(), 7);
  EXPECT_THAT(zpk7.GetConjugatedZeros()[0], ComplexNear(0, 1.069178406340));
  EXPECT_THAT(zpk7.GetConjugatedZeros()[1], ComplexNear(0, 1.165240166429));
  EXPECT_THAT(zpk7.GetConjugatedZeros()[2], ComplexNear(0, 1.704070901857));
  EXPECT_NEAR(zpk7.GetRealPoles()[0], -0.430200300943, 1e-9);
  EXPECT_THAT(zpk7.GetConjugatedPoles()[0],
                 ComplexNear(-0.016074016029, 1.003360790160));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[1],
                 ComplexNear(-0.080307772790, 0.941380150432));
  EXPECT_THAT(zpk7.GetConjugatedPoles()[2],
                 ComplexNear(-0.256233622857, 0.687630058081));
  EXPECT_NEAR(zpk7.GetGain(), 0.046200568751, 1e-9);
}

// These tests are more thorough than the typed tests below.
TEST(PoleZeroFilterDesignTest, ButterworthLowpassTest) {
  constexpr double kTolerance = 1e-4;
  for (float corner_frequency_hz : {10.0f, 100.0f, 1000.0f, 10000.0f}) {
    for (int order : {2, 5, 8}) {
      BiquadFilterCascadeCoefficients coeffs =
          ButterworthFilterDesign(order).LowpassCoefficients(
              kSampleRateHz, corner_frequency_hz);
      SCOPED_TRACE(
          StrFormat("Butterworth lowpass with corner = %f and order %d.",
                    corner_frequency_hz, order));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              0.0f, kSampleRateHz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(1 / M_SQRT2, kTolerance),
                                      corner_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                              kNyquistHz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
          corner_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
    }
  }
}

TEST(PoleZeroFilterDesignTest, ButterworthHighpassCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float corner_frequency_hz : {100.0f, 1000.0f, 10000.0f}) {
    for (int order : {2, 5, 8}) {
      BiquadFilterCascadeCoefficients coeffs =
          ButterworthFilterDesign(order).HighpassCoefficients(
              kSampleRateHz, corner_frequency_hz);
      SCOPED_TRACE(
          StrFormat("Butterworth highpass with corner = %f and order %d.",
                    corner_frequency_hz, order));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                      0.0f, kSampleRateHz));
      ASSERT_THAT(coeffs,
                  MagnitudeResponseIs(DoubleNear(1 / M_SQRT2, kTolerance),
                                      corner_frequency_hz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                              kNyquistHz, kSampleRateHz));
      ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
          20.0f, corner_frequency_hz, kSampleRateHz, kNumPoints));
    }
  }
}

TEST(PoleZeroFilterDesignTest, ButterworthBandpassCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float low_frequency_hz : {1000.0f, 10000.0f}) {
    for (int order : {2, 3, 4}) {
      for (float bandwidth_hz : {200.0f, 400.0f}) {
        const float high_frequency_hz = low_frequency_hz + bandwidth_hz;
        const float center_frequency_hz =
            std::sqrt(low_frequency_hz * high_frequency_hz);
        BiquadFilterCascadeCoefficients coeffs =
            ButterworthFilterDesign(order).BandpassCoefficients(
                kSampleRateHz, low_frequency_hz, high_frequency_hz);
        SCOPED_TRACE(
            StrFormat("Butterworth bandpass (order %d) with range = [%f, %f].",
                      order, low_frequency_hz, high_frequency_hz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(1.0, kTolerance),
                                        center_frequency_hz, kSampleRateHz));
        ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
            20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
        ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
            center_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
      }
    }
  }
}

TEST(PoleZeroFilterDesignTest, ButterworthBandstopCoefficientsTest) {
  constexpr double kTolerance = 1e-4;
  for (float low_frequency_hz : {1000.0f, 10000.0f}) {
    for (int order : {2, 3, 4}) {
      for (float bandwidth_hz : {200.0f, 400.0f}) {
        const float high_frequency_hz = low_frequency_hz + bandwidth_hz;
        const float center_frequency_hz =
            std::sqrt(low_frequency_hz * high_frequency_hz);
        BiquadFilterCascadeCoefficients coeffs =
            ButterworthFilterDesign(order).BandstopCoefficients(
                kSampleRateHz, low_frequency_hz, high_frequency_hz);
        SCOPED_TRACE(
            StrFormat("Butterworth bandstop (order %d) with range = [%f, %f].",
                      order, low_frequency_hz, high_frequency_hz));
        ASSERT_THAT(coeffs,
                    MagnitudeResponseIs(DoubleNear(0.0, kTolerance),
                                        center_frequency_hz, kSampleRateHz));
        ASSERT_THAT(coeffs, MagnitudeResponseDecreases(
            20.0f, center_frequency_hz, kSampleRateHz, kNumPoints));
        ASSERT_THAT(coeffs, MagnitudeResponseIncreases(
            center_frequency_hz, kNyquistHz, kSampleRateHz, kNumPoints));
      }
    }
  }
}

static constexpr float kPassbandRippleDb = 0.25;
static constexpr float kStopbandRippleDb = 25.0;
static constexpr float kRippleToleranceLinear = 0.35;
static constexpr int kOrder = 4;

class ButterworthDefinedFilterDesign : public ButterworthFilterDesign  {
 public:
  ButterworthDefinedFilterDesign()
      : ButterworthFilterDesign(kOrder) {}
};

class ChebyshevType1DefinedFilterDesign : public ChebyshevType1FilterDesign  {
 public:
  ChebyshevType1DefinedFilterDesign()
      : ChebyshevType1FilterDesign(kOrder, kPassbandRippleDb) {}
};

class ChebyshevType2DefinedFilterDesign : public ChebyshevType2FilterDesign  {
 public:
  ChebyshevType2DefinedFilterDesign()
      : ChebyshevType2FilterDesign(kOrder, kStopbandRippleDb) {}
};

class EllipticDefinedFilterDesign : public EllipticFilterDesign  {
 public:
  EllipticDefinedFilterDesign()
      : EllipticFilterDesign(kOrder, kPassbandRippleDb, kStopbandRippleDb) {}
};

template <typename TypeParam>
class PoleZeroFilterDesignTypedTest : public ::testing::Test {};

typedef ::testing::Types<
    ButterworthDefinedFilterDesign,
    ChebyshevType1DefinedFilterDesign,
    ChebyshevType2DefinedFilterDesign,
    EllipticDefinedFilterDesign
    > TestTypes;
TYPED_TEST_SUITE(PoleZeroFilterDesignTypedTest, TestTypes);

constexpr float kTransitionFactor = 3;

TYPED_TEST(PoleZeroFilterDesignTypedTest, LowpassBasicTest) {
  constexpr float kCornerFrequency = 1000.0f;
  BiquadFilterCascadeCoefficients coeffs =
      TypeParam().LowpassCoefficients(kSampleRateHz, kCornerFrequency);
  ASSERT_THAT(coeffs, MagnitudeResponseIs(
      DoubleNear(1.0, kRippleToleranceLinear), 0.0f, kSampleRateHz));
  ASSERT_THAT(coeffs, MagnitudeResponseIs(
      DoubleNear(0.0, kRippleToleranceLinear), kNyquistHz, kSampleRateHz));
  // Test that response is less than kRippleToleranceLinear above the
  // transition and within kRippleToleranceLinear of 1 above.
  for (int f = 20; f < kCornerFrequency / kTransitionFactor; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(1.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
  for (int f = kCornerFrequency * kTransitionFactor; f < kNyquistHz; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(0.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
}

TYPED_TEST(PoleZeroFilterDesignTypedTest, HighpassBasicTest) {
  constexpr float kCornerFrequency = 1000.0f;
  BiquadFilterCascadeCoefficients coeffs =
      TypeParam().HighpassCoefficients(kSampleRateHz, kCornerFrequency);
  ASSERT_THAT(coeffs, MagnitudeResponseIs(
      DoubleNear(0.0, kRippleToleranceLinear), 0.0f, kSampleRateHz));
  ASSERT_THAT(coeffs, MagnitudeResponseIs(
      DoubleNear(1.0, kRippleToleranceLinear), kNyquistHz, kSampleRateHz));
  // Test that response is less than kRippleToleranceLinear below the
  // transition and within kRippleToleranceLinear of 1 above.
  for (int f = 20; f < kCornerFrequency / kTransitionFactor; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(0.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
  for (int f = kCornerFrequency * kTransitionFactor; f < kNyquistHz; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(1.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
}

TYPED_TEST(PoleZeroFilterDesignTypedTest, BandpassBasicTest) {
  constexpr float kLowFrequency = 1000.0f;
  constexpr float kHighFrequency = 3000.0f;
  const float center_frequency_hz = std::sqrt(kLowFrequency * kHighFrequency);
  BiquadFilterCascadeCoefficients coeffs = TypeParam().BandpassCoefficients(
      kSampleRateHz, kLowFrequency, kHighFrequency);
  ASSERT_THAT(coeffs,
              MagnitudeResponseIs(DoubleNear(1.0, kRippleToleranceLinear),
                                  center_frequency_hz, kSampleRateHz));
  for (int f = 20; f < kLowFrequency / kTransitionFactor; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(0.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
  for (int f = kHighFrequency * kTransitionFactor; f < kNyquistHz; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(0.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
}

TYPED_TEST(PoleZeroFilterDesignTypedTest, BandstopBasicTest) {
  constexpr float kLowFrequency = 1000.0f;
  constexpr float kHighFrequency = 3000.0f;
  const float center_frequency_hz = std::sqrt(kLowFrequency * kHighFrequency);
  BiquadFilterCascadeCoefficients coeffs = TypeParam().BandstopCoefficients(
      kSampleRateHz, kLowFrequency, kHighFrequency);
  ASSERT_THAT(coeffs,
              MagnitudeResponseIs(DoubleNear(0.0, kRippleToleranceLinear),
                                  center_frequency_hz, kSampleRateHz));

  for (int f = 20; f < kLowFrequency / kTransitionFactor; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(1.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
  for (int f = kHighFrequency * kTransitionFactor; f < kNyquistHz; f *= 1.1) {
    ASSERT_THAT(coeffs, MagnitudeResponseIs(
        DoubleNear(1.0, kRippleToleranceLinear), f, kSampleRateHz));
  }
}

}  // namespace
}  // namespace linear_filters
