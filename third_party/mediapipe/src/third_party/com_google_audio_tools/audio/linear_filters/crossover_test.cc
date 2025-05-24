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

#include "audio/dsp/decibels.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

using ::absl::StrFormat;
using ::audio_dsp::AmplitudeRatioToDecibels;

static constexpr float kSampleRate = 48000.0;
static constexpr float kNyquist = kSampleRate / 2;

float ComputeRolloff(float start_frequency_hz, float end_frequency_hz,
                     float sample_rate_hz,
                     const BiquadFilterCascadeCoefficients& coeffs) {
  constexpr int kNumSteps = 20;
  const float step_size =
      end_frequency_hz - start_frequency_hz / (kNumSteps - 1);
  float sum_rolloff_db = 0;
  for (int i = 1; i < kNumSteps; ++i) {
    const float freq = start_frequency_hz + i * step_size;
    const float prev_freq = start_frequency_hz + (i - 1) * step_size;
    float rolloff = -AmplitudeRatioToDecibels(
        coeffs.GainMagnitudeAtFrequency(freq, sample_rate_hz) /
        coeffs.GainMagnitudeAtFrequency(prev_freq, sample_rate_hz));

    float rolloff_per_octave = rolloff / std::log2(freq / prev_freq);
    sum_rolloff_db += rolloff_per_octave;
  }
  return sum_rolloff_db / (kNumSteps - 1);
}

float CombinedGainDb(float crossover_frequency_hz,  float sample_rate_hz,
                     const CrossoverFilterDesign& crossover) {
  float gain_lowpass = crossover.GetLowpassCoefficients()
      .GainMagnitudeAtFrequency(crossover_frequency_hz, sample_rate_hz);
  float gain_highpass = crossover.GetHighpassCoefficients()
      .GainMagnitudeAtFrequency(crossover_frequency_hz, sample_rate_hz);
  return 20 * std::log10(gain_lowpass + gain_highpass);
}

TEST(CrossoverTest, FirstOrderTest) {
  for (float crossover_frequency_hz : {850.0, 1200.0}) {
    SCOPED_TRACE(
        StrFormat("First order, crossover = %f.", crossover_frequency_hz));
    CrossoverFilterDesign crossover(CrossoverType::kButterworth, 1,
                                    crossover_frequency_hz, kSampleRate);
    // Tests that in the limit frequency -> 0, the slope is -6dB/octave per
    // order.
    EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 0.1, 20, kSampleRate,
                               crossover.GetHighpassCoefficients()),
                -6, 0.75);
    // Tests that in the limit frequency -> kNyquist, the slope is -6dB/octave
    // per order.
    // Don't go all the way to Nyquist, Bilinear transform warping actually
    // causes much steeper rolloff above 20k.
    EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 1.5, 0.75 * kNyquist,
                               kSampleRate, crossover.GetLowpassCoefficients()),
                -6, 0.75);
  }
}

bool PhasesNear(double actual, double expected, double tolerance) {
  return (std::abs(actual - expected) < tolerance) ||
         (std::abs(std::abs(actual - expected) - 2 * M_PI) < tolerance);
}

TEST(CrossoverTest, HigherOrderButterworthTest) {
  for (int order : {2, 3, 4, 6}) {
    for (float crossover_frequency_hz : {850.0, 1200.0}) {
      SCOPED_TRACE(StrFormat("Butterworth: Order %d, crossover = %f.", order,
                             crossover_frequency_hz));
      CrossoverFilterDesign crossover(CrossoverType::kButterworth, order,
                                      crossover_frequency_hz, kSampleRate);
      // Test that we get the expected rolloff and low and high frequencies.
      EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 0.1, 20, kSampleRate,
                                 crossover.GetHighpassCoefficients()),
                  -6 * order, 1.5);
      EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 1.5, 0.75 * kNyquist,
                                 kSampleRate,
                                 crossover.GetLowpassCoefficients()),
                  -6 * order,
                  order > 4 ? 5.0 : 2.5);  // High order increases tolerance.
      // The Butterworth crossover filter has a 3dB bump at the
      // crossover point.
      EXPECT_NEAR(CombinedGainDb(crossover_frequency_hz, kSampleRate,
                                 crossover),
                  3.01, 1e-3);
      // It is otherwise fairly flat.
      for (int f = 20; f < crossover_frequency_hz / 3; f += 1.15) {
        ASSERT_NEAR(CombinedGainDb(f, kSampleRate, crossover), 0.0, 1.0);
      }
      for (int f = crossover_frequency_hz * 3;  f < kNyquist; f += 1.15) {
        ASSERT_NEAR(CombinedGainDb(f, kSampleRate, crossover), 0.0, 1.0);
      }

      if (order % 2 == 0) {
        // The outputs are in phase with each other.
        EXPECT_TRUE(PhasesNear(
            crossover.GetLowpassCoefficients().PhaseResponseAtFrequency(
                crossover_frequency_hz, kSampleRate),
            crossover.GetHighpassCoefficients().PhaseResponseAtFrequency(
                crossover_frequency_hz, kSampleRate), 0.1));
      }
    }
  }
}

TEST(CrossoverTest, LRTest) {
  for (int order : {2, 4, 6, 8}) {
    for (float crossover_frequency_hz : {850.0, 1200.0}) {
      SCOPED_TRACE(StrFormat("Linkwitz-Riley: Order %d, crossover = %f.", order,
                             crossover_frequency_hz));
      CrossoverFilterDesign crossover(CrossoverType::kLinkwitzRiley, order,
                                      crossover_frequency_hz, kSampleRate);
      // Test that we get the expected rolloff and low and high frequencies.
      EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 0.1, 20, kSampleRate,
                                 crossover.GetHighpassCoefficients()),
                  -6 * order, 1.5);
      EXPECT_NEAR(ComputeRolloff(crossover_frequency_hz * 1.5, 0.75 * kNyquist,
                                 kSampleRate,
                                 crossover.GetLowpassCoefficients()),
                  -6 * order,
                  order > 4 ? 5.0 : 2.5);  // High order increases tolerance.
      // The Linkwitz-Riley filter has a flat response at the crossover
      // point.
      EXPECT_NEAR(CombinedGainDb(crossover_frequency_hz, kSampleRate,
                                 crossover),
                  0.0, 1e-3);
      // Make sure we're flat everywhere else, too.
      for (int f = 20; f < kNyquist; f += 1.15) {
        ASSERT_NEAR(CombinedGainDb(f, kSampleRate, crossover), 0.0, 1e-3);
      }

      // The outputs are in phase with each other.
      EXPECT_TRUE(PhasesNear(
          crossover.GetLowpassCoefficients().PhaseResponseAtFrequency(
              crossover_frequency_hz, kSampleRate),
          crossover.GetHighpassCoefficients().PhaseResponseAtFrequency(
              crossover_frequency_hz, kSampleRate), 0.1));
    }
  }
}


}  // namespace
}  // namespace linear_filters
