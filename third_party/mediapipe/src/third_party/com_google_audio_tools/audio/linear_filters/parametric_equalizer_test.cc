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

#include "audio/linear_filters/parametric_equalizer.h"

#include <complex>

#include "audio/dsp/decibels.h"
#include "audio/dsp/signal_generator.h"
#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/biquad_filter_coefficients.h"
#include "audio/linear_filters/biquad_filter_design.h"
#include "audio/linear_filters/equalizer_filter_params.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

ParametricEqualizerParams ParametricEqualizerParamsForTest(
    const std::vector<float>& gains_db, float gain_term_db) {
  ParametricEqualizerParams params;
  params.AddStage(EqualizerFilterParams::kLowShelf, 50.0f, 0.707f, gains_db[0]);
  params.AddStage(EqualizerFilterParams::kPeak, 700.0f, 2.0f, gains_db[1]);
  params.AddStage(EqualizerFilterParams::kPeak, 5000.0f, 3.0f, gains_db[2]);
  params.AddStage(EqualizerFilterParams::kPeak, 6000.0f, 0.60f, gains_db[3]);
  params.AddStage(
      EqualizerFilterParams::kHighShelf, 14000.0f, 1.0f, gains_db[4]);

  params.SetGainDb(gain_term_db);

  return params;
}

TEST(ParametricEqualizerParamsTest, EnableDisableTest) {
  ParametricEqualizerParams params;
  EXPECT_EQ(params.GetGainDb(), 0);  // Initial gain is zero dB.
  EXPECT_EQ(params.GetNumEnabledStages(), 0);
  EXPECT_EQ(params.GetTotalNumStages(), 0);

  params.AddStage(EqualizerFilterParams::kLowShelf, 50.0f, 0.707f, 0.3f);
  EXPECT_EQ(params.GetNumEnabledStages(), 1);
  EXPECT_EQ(params.GetTotalNumStages(), 1);

  params.AddStage(EqualizerFilterParams::kPeak, 700.0f, 2.0f, 5.0f);
  EXPECT_EQ(params.GetNumEnabledStages(), 2);
  EXPECT_EQ(params.GetTotalNumStages(), 2);

  EXPECT_TRUE(params.IsStageEnabled(1));
  params.SetStageEnabled(1, false);
  EXPECT_FALSE(params.IsStageEnabled(1));
  EXPECT_EQ(params.GetNumEnabledStages(), 1);
  EXPECT_EQ(params.GetTotalNumStages(), 2);
}

TEST(ParametricEqualizerParamsTest, SetValuesTest) {
  ParametricEqualizerParams params;

  params.AddStage(EqualizerFilterParams::kPeak, 200.0f, 2.0f, 1.0f);
  EXPECT_EQ(params.StageParams(0).frequency_hz, 200.0f);

  // Set some params for a high shelf.
  params.AddStage(EqualizerFilterParams::kHighShelf, 500.0f, 2.0f, 1.0f);
  EXPECT_EQ(params.StageParams(1).frequency_hz, 500.0f);

  // Disable the first peak filter and make sure nothing has changed.
  params.SetStageEnabled(0, false);
  EXPECT_EQ(params.StageParams(0).frequency_hz, 200.0f);
}

TEST(ParametricEqualizerParamsTest, SetAndClearGainsTest) {
  ParametricEqualizerParams params;

  params.AddStage(EqualizerFilterParams::kPeak, 500.0f, 2.0f, 1.0f);
  EXPECT_EQ(params.StageParams(0).gain_db, 1.0f);

  params.AddStage(EqualizerFilterParams::kLowpass, 500.0f, 2.0f, 1.0f);
  params.MutableStageParams(1)->gain_db = 0.4f;
  EXPECT_EQ(params.StageParams(1).gain_db, 0.4f);

  params.SetGainDb(0.3f);
  EXPECT_EQ(params.GetGainDb(), 0.3f);

  params.ClearAllGains();
  EXPECT_EQ(params.StageParams(0).gain_db, 0.0f);
  EXPECT_EQ(params.GetGainDb(), 0.0f);
  EXPECT_TRUE(params.IsStageEnabled(0));
  EXPECT_FALSE(params.IsStageEnabled(1));
}

// Take log of frequencies. Used for convergence tolerance test, since we fit
// and apply convergence stopping criterion in log-frequency space.
ParametricEqualizerParams ToLogFrequency(
    const ParametricEqualizerParams& params) {
  ParametricEqualizerParams params_with_log_frequency = params;
  for (int i = 0; i < params.GetNumEnabledStages(); ++i) {
    params_with_log_frequency.MutableStageParams(i)->frequency_hz =
        std::log(params.StageParams(i).frequency_hz);
  }
  return params_with_log_frequency;
}

using ::audio_dsp::AmplitudeRatioToDecibels;
using ::std::vector;

TEST(ParametricEqualizerTest, InitTest) {
  constexpr float kSampleRateHz = 48000.0f;
  constexpr float k2dBLinear = 1.258925f;
  constexpr float kNeg6dBLinear = 0.5011872f;
  constexpr float k6dBLinear = 1.99526f;
  constexpr float kGainFactorLinear = kNeg6dBLinear;

  ParametricEqualizerParams params;
  params.SetGainDb(-6.0f);
  params.AddStage(EqualizerFilterParams::kLowShelf, 50.0f, 0.707f, 2.0f);
  params.AddStage(EqualizerFilterParams::kPeak, 750.0f, 6.0f, 6.0f);
  params.AddStage(EqualizerFilterParams::kPeak, 5000.0f, 8.0f, -6.0f);
  params.AddStage(EqualizerFilterParams::kHighShelf, 18000.0f, 0.707f, 6.0f);

  BiquadFilterCascadeCoefficients coeffs =
      params.GetCoefficients(kSampleRateHz);
  EXPECT_EQ(coeffs.size(), 4);
  constexpr float kNearCornerTol = 7e-4;
  EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(0, kSampleRateHz),
              kGainFactorLinear * k2dBLinear, kNearCornerTol);
  EXPECT_NEAR(
      coeffs.GainMagnitudeAtFrequency(
          params.StageParams(1).frequency_hz, kSampleRateHz),
      kGainFactorLinear * k6dBLinear, kNearCornerTol);
  EXPECT_NEAR(
      coeffs.GainMagnitudeAtFrequency(
          params.StageParams(2).frequency_hz, kSampleRateHz),
      kGainFactorLinear * kNeg6dBLinear, kNearCornerTol);
  EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(kSampleRateHz / 2, kSampleRateHz),
              kGainFactorLinear * k6dBLinear, kNearCornerTol);

  // Spot check some frequencies in between the ones specified above.
  constexpr float kAwayFromPeakTol = 1e-2;
  EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(300, kSampleRateHz),
              kGainFactorLinear, kAwayFromPeakTol);
  EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(2000, kSampleRateHz),
              kGainFactorLinear, kAwayFromPeakTol);
  EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(10000, kSampleRateHz),
              kGainFactorLinear, kAwayFromPeakTol);
}

TEST(ParametricEqualizerTest, CanMatchResponse_FitGainsOnly) {
  constexpr float kSampleRateHz = 48000.0f;
  constexpr float gain_term_db = 6.0f;
  vector<float> gains_db = {2.0f, 6.0f, -6.0f, 1.4f, -2.3f};
  // Make equalizer with some known set of gains.
  ParametricEqualizerParams params =
      ParametricEqualizerParamsForTest(gains_db, gain_term_db);

  // Sample the frequency response.
  constexpr int kNumPoints = 1000;
  BiquadFilterCascadeCoefficients coeffs =
      params.GetCoefficients(kSampleRateHz);
  EXPECT_EQ(coeffs.size(), 5);
  vector<float> frequencies_hz(kNumPoints);
  vector<float> magnitudes_db(kNumPoints);

  // Generates log range from kStartFreq to kEndFreq.
  constexpr float kStartFreq = 20;
  constexpr float kEndFreq = 24000;
  const float kNoiseStdDevDb = 0.01;
  const float kNoiseMeanDb = 0.0;
  const float exp_constant = std::log(kEndFreq / kStartFreq) / kNumPoints;
  auto noise = audio_dsp::GenerateWhiteGaussianNoise(
      magnitudes_db.size(), kNoiseMeanDb, kNoiseStdDevDb);
  for (int i = 0; i < kNumPoints; ++i) {
    frequencies_hz[i] = kStartFreq * std::exp(exp_constant * i);
    magnitudes_db[i] = AmplitudeRatioToDecibels(coeffs.GainMagnitudeAtFrequency(
                           frequencies_hz[i], kSampleRateHz)) +
                       noise(i);
  }

  // Reset the gains.
  params.ClearAllGains();

  // A function for checking that gains are within 'tol' dB of the expected
  // value.
  auto ExpectResultsWithinTolerance = [&params, &gains_db](float tol) {
    // Check that the gains are correctly found.
    ASSERT_NEAR(params.StageParams(0).gain_db, gains_db[0], tol);
    ASSERT_NEAR(params.StageParams(1).gain_db, gains_db[1], tol);
    ASSERT_NEAR(params.StageParams(2).gain_db, gains_db[2], tol);
    ASSERT_NEAR(params.StageParams(3).gain_db, gains_db[3], tol);
    ASSERT_NEAR(params.StageParams(4).gain_db, gains_db[4], tol);
    ASSERT_NEAR(params.GetGainDb(), gain_term_db, tol);
  };

  // Check that we do ok with a single iteration.
  constexpr float kVeryLooseToleranceDb = 1.5;
  ConvergenceParams convergence_params;
  convergence_params.max_iterations = 1;
  SetParametricEqualizerGainsToMatchMagnitudeResponse(
      frequencies_hz, magnitudes_db, convergence_params, kSampleRateHz,
      &params);
  {
    SCOPED_TRACE("Very loose tolerance and a single iteration.");
    ExpectResultsWithinTolerance(kVeryLooseToleranceDb);
  }
  // Check that we do pretty well with few iterations.
  constexpr float kLooseToleranceDb = 0.15;
  convergence_params.max_iterations = 2;
  params.ClearAllGains();
  SetParametricEqualizerGainsToMatchMagnitudeResponse(
      frequencies_hz, magnitudes_db, convergence_params, kSampleRateHz,
      &params);
  {
    SCOPED_TRACE("Somewhat loose tolerance and two iterations.");
    ExpectResultsWithinTolerance(kLooseToleranceDb);
  }

  // Check that we can converge to the correct answer.
  constexpr float kTighterToleranceDb = 0.015;
  convergence_params.max_iterations = 5;
  convergence_params.convergence_threshold_db = kTighterToleranceDb;
  params.ClearAllGains();
  SetParametricEqualizerGainsToMatchMagnitudeResponse(
      frequencies_hz, magnitudes_db, convergence_params, kSampleRateHz,
      &params);
  {
    SCOPED_TRACE("Tighter tolerance and many iterations.");
    ExpectResultsWithinTolerance(kTighterToleranceDb);
  }
}

TEST(ParametricEqualizerTest, CanMatchResponse_EstimateAllParams) {
  constexpr float kSampleRateHz = 48000.0f;
  constexpr float gain_term_db = 6.0f;
  vector<float> gains_db = {2.0f, 6.0f, -6.0f, 1.4f, -2.3f};
  ParametricEqualizerParams expected_params =
      ParametricEqualizerParamsForTest(gains_db, gain_term_db);

  // Sample the frequency response.
  constexpr int kNumPoints = 1000;
  Eigen::ArrayXf frequencies_hz(kNumPoints);

  // Generates log range from kStartFreq to kEndFreq.
  constexpr float kStartFreq = 5;
  constexpr float kEndFreq = 24000;
  const float exp_constant = std::log(kEndFreq / kStartFreq) / kNumPoints;
  for (int i = 0; i < kNumPoints; ++i) {
    frequencies_hz(i) = kStartFreq * std::exp(exp_constant * i);
  }
  Eigen::ArrayXf magnitudes_db;
  AmplitudeRatioToDecibels(
      ParametricEqualizerGainMagnitudeAtFrequencies(
          expected_params, kSampleRateHz, frequencies_hz),
      &magnitudes_db);
  // Add a bit of Gaussian noise.
  const float kNoiseStdDevDb = 0.001;
  const float kNoiseMeanDb = 0.0;
  magnitudes_db += audio_dsp::GenerateWhiteGaussianNoise(
      magnitudes_db.size(), kNoiseMeanDb, kNoiseStdDevDb);

  //  Reset the gains and perturb the frequencies, and quality factors.
  ParametricEqualizerParams params = expected_params;
  params.ClearAllGains();
  params.MutableStageParams(0)->frequency_hz = 40.0f;
  params.MutableStageParams(0)->quality_factor = 1.3f;
  params.MutableStageParams(1)->frequency_hz = 800.0f;
  params.MutableStageParams(1)->quality_factor = 1.0f;
  params.MutableStageParams(2)->frequency_hz = 4000.0f;
  params.MutableStageParams(2)->quality_factor = 2.0f;
  params.MutableStageParams(3)->frequency_hz = 7000.0f;
  params.MutableStageParams(3)->quality_factor = 0.4f;
  params.MutableStageParams(4)->frequency_hz = 16000.0f;
  params.MutableStageParams(4)->quality_factor = 0.8f;

  // See if we can converge to a relatively loose tolerance.
  const float kToleranceDb = 0.2;
  NelderMeadFitParams fit_params;
  fit_params.magnitude_db_rms_error_tol = kToleranceDb / 20;
  float rms_error = FitParametricEqualizer(
      frequencies_hz, magnitudes_db, kSampleRateHz, fit_params, &params);
  auto log_freq = ToLogFrequency(params);
  auto log_freq_expected = ToLogFrequency(expected_params);
  for (int i = 0; i < log_freq.GetNumEnabledStages(); ++i) {
    EXPECT_NEAR(log_freq.StageParams(i).frequency_hz,
                log_freq_expected.StageParams(i).frequency_hz,
                kToleranceDb);
  }

  EXPECT_NEAR(rms_error, 0, kToleranceDb);

  // See if we can converge to a tighter tolerance.
  const float kTightToleranceDb = 0.13;
  fit_params.magnitude_db_rms_error_tol = kTightToleranceDb / 20;
  fit_params.max_iterations = 2000;
  fit_params.inner_convergence_params.convergence_threshold_db =
      kTightToleranceDb / 2;
  fit_params.inner_convergence_params.max_iterations = 5;
  rms_error = FitParametricEqualizer(frequencies_hz, magnitudes_db,
                                     kSampleRateHz, fit_params, &params);
  for (int i = 0; i < log_freq.GetNumEnabledStages(); ++i) {
    EXPECT_NEAR(log_freq.StageParams(i).frequency_hz,
                log_freq_expected.StageParams(i).frequency_hz,
                kTightToleranceDb);
  }
  EXPECT_NEAR(rms_error, 0, kTightToleranceDb);
}

TEST(ParametricEqualizerGainMagnitude, SimpleTest) {
  constexpr float kSampleRateHz = 48000.0f;
  constexpr float kTolerance = 0.005f;
  ParametricEqualizerParams params;
  params.SetGainDb(-6.0f),
  params.AddStage(EqualizerFilterParams::kLowShelf, 50.0f, 0.707f, 2.0f);
  params.AddStage(EqualizerFilterParams::kPeak, 5000.0f, 6.0f, 6.0f);
  params.AddStage(EqualizerFilterParams::kHighShelf, 18000.0f, 0.707f, 5.0f);

  Eigen::ArrayXf frequencies_hz(4);
  frequencies_hz << 10, 5000, 12000, 23000;
  Eigen::ArrayXf gains = ParametricEqualizerGainMagnitudeAtFrequencies(
      params, kSampleRateHz, frequencies_hz);
  EXPECT_THAT(gains, audio_dsp::EigenArrayNear<float>({0.63, 1, 0.51, 0.89},
                                                      kTolerance));
}

}  // namespace
}  // namespace linear_filters
