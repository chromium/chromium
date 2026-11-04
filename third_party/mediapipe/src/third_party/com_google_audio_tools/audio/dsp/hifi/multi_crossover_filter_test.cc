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

#include <cmath>
#include <complex>
#include <vector>

#include "audio/dsp/kiss_fft.h"
#include "audio/dsp/max_filter.h"
#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "glog/logging.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {

using audio_dsp::EigenArrayNear;

TEST(MultiCrossoverFilter, BasicTest) {
  constexpr int kNumChannels = 2;
  constexpr int kBands = 5;
  MultiCrossoverFilter<float> filter(kBands, 4);
  std::vector<float> crossover_frequencies = {10, 100, 1000, 10000};
  filter.Init(kNumChannels, 48000.0f, crossover_frequencies);
  Eigen::ArrayXXf data = Eigen::ArrayXXf::Zero(kNumChannels, 100);
  filter.ProcessBlock(data);
  for (int i = 0; i < kBands; ++i) {
    ASSERT_THAT(
        filter.FilteredOutput(i),
        EigenArrayNear(Eigen::ArrayXXf::Zero(kNumChannels, 100), 1e-4));
  }
}

int Square(int i) { return i * i; }

// Makes sure that for an arbitrary number of bands that the total filter output
// energy is equal to the total filter input energy.
TEST(MultiCrossoverFilter, DifferentBandsTest) {
  constexpr int kNumChannels = 2;

  for (int bands = 2; bands < 3; ++bands) {
    MultiCrossoverFilter<float> filter(bands, 4);
    std::vector<float> crossover_frequencies(bands - 1);
    for (int i = 0; i < bands - 1; ++i) {
      crossover_frequencies[i] = 100.0f * Square(i + 1);
    }
    filter.Init(kNumChannels, 48000.0f, crossover_frequencies);
    Eigen::ArrayXXf data = Eigen::ArrayXXf::Zero(kNumChannels, 100);
    filter.ProcessBlock(data);
    for (int i = 0; i < bands; ++i) {
      ASSERT_THAT(
          filter.FilteredOutput(i),
          EigenArrayNear(Eigen::ArrayXXf::Zero(kNumChannels, 100), 1e-4));
    }
  }
}

// This has some overlap with the sweep tests below, but it also checks
// individual band flatness. The sweep tests feel more like the kinds of tests
// that clients would do to test a filterbank, so we ensure that they meet
// expectations.
TEST(MultiCrossoverFilter, FlatSpectrum) {
  constexpr int kNumChannels = 1;
  constexpr int kBands = 4;
  constexpr int kBlockSize = 2048;
  constexpr float kSampleRate = 48000.0f;
  MultiCrossoverFilter<double> filter(kBands, 4);
  std::vector<double> crossover_frequencies = {100, 1000, 10000};
  std::vector<double> band_start_frequencies = {10, 200, 2000, 14000};
  std::vector<double> band_stop_frequencies = {50, 600, 6000, 20000};
  filter.Init(kNumChannels, kSampleRate, crossover_frequencies);
  Eigen::ArrayXXd data = Eigen::ArrayXXd::Zero(kNumChannels, kBlockSize);
  // An impulse.
  data(0, 0) = 1;
  filter.ProcessBlock(data);

  RealFFTTransformer fft(kBlockSize, true);
  std::vector<std::complex<float>> transformed(fft.GetTransformedSize());
  std::vector<std::complex<float>> magnitude_summation(
      fft.GetTransformedSize());
  for (int i = 0; i < kBands; ++i) {
    Eigen::ArrayXXf float_data =
        filter.FilteredOutput(i).template cast<float>();
    fft.ForwardTransform(float_data.data(), transformed.data());
    int start_bin = band_start_frequencies[i] / kSampleRate * kBlockSize;
    int stop_bin = band_stop_frequencies[i] / kSampleRate * kBlockSize;
    // Bands of interest are reasonably near one where they're supposed to be.
    for (int j = start_bin; j < stop_bin; ++j) {
      ASSERT_NEAR(std::abs(transformed[j]), 1.0, 0.12);
    }
    for (int j = 0; j < fft.GetTransformedSize(); ++j) {
      magnitude_summation[j] += transformed[j];
    }
  }
  // Make sure that the magnitude responses sum to 1.
  for (int i = 0; i < fft.GetTransformedSize(); ++i) {
    ASSERT_NEAR(std::abs(magnitude_summation[i]), 1.0, 1e-3);
  }
}

// Just a shorthand alias for a function with a long signature.
auto WrapAngle = [](double x) {
  return MultiCrossoverFilter<double>::WrapAngle<double>(x);
};

TEST(WrapPhaseTest, WrapPhase) {
  EXPECT_NEAR(WrapAngle(M_PI), -M_PI, 1e-5);  // Inverts, but that's fine.
  EXPECT_NEAR(WrapAngle(M_PI - 0.001), M_PI - 0.001, 1e-5);
  EXPECT_NEAR(WrapAngle(2*M_PI), 0, 1e-5);
  EXPECT_NEAR(WrapAngle(-M_PI), -M_PI, 1e-5);
  EXPECT_NEAR(WrapAngle(-M_PI + 0.001), -M_PI + 0.001, 1e-5);
  EXPECT_NEAR(WrapAngle(M_PI_2), M_PI_2, 1e-5);
  EXPECT_NEAR(WrapAngle(-M_PI_2), -M_PI_2, 1e-5);
  EXPECT_NEAR(WrapAngle(3 * M_PI_2), -M_PI_2, 1e-5);
  EXPECT_NEAR(WrapAngle(-3 * M_PI_2), M_PI_2, 1e-5);
}

template <typename T>
void CheckPhasesForEachBand(const MultiCrossoverFilter<T>& filter,
                            const std::vector<T>& frequencies) {
  // Test every pair of bands at every crossover frequency.
  for (int f = 0; f < frequencies.size(); ++f) {
    EXPECT_NEAR(WrapAngle(filter.GetPhaseResponseAt(f, frequencies[f]) -
                          filter.GetPhaseResponseAt(f + 1, frequencies[f])),
                0, 1e-3);
  }
}

// NOTE: This tests the reported transfer function of the filter, NOT the actual
// filter response. If the phase computation is wrong, this test won't catch it.
// However, the flatness tests will catch it.
void TestForConsistentPhase(const std::vector<double>& crossover_frequencies,
                            int order) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRate = 48000.0f;
  MultiCrossoverFilter<double> filter(
      crossover_frequencies.size() + 1, order);
  filter.Init(kNumChannels, kSampleRate, crossover_frequencies);
  CheckPhasesForEachBand(filter, crossover_frequencies);
}

TEST(MultiCrossoverFilter, PhaseTests_2WayCrossover) {
  TestForConsistentPhase({  500}, 2);
  TestForConsistentPhase({12000}, 2);
  TestForConsistentPhase({  500}, 4);
  TestForConsistentPhase({12000}, 4);
  TestForConsistentPhase({  500}, 6);
  TestForConsistentPhase({12000}, 6);
  TestForConsistentPhase({  500}, 8);
  TestForConsistentPhase({12000}, 8);
}

TEST(MultiCrossoverFilter, PhaseTests_3WayCrossover) {
  TestForConsistentPhase({100, 1000}, 2);
  TestForConsistentPhase({333, 6202}, 2);
  TestForConsistentPhase({100, 1000}, 4);
  TestForConsistentPhase({333, 6202}, 4);
  TestForConsistentPhase({100, 1000}, 6);
  TestForConsistentPhase({333, 6202}, 6);
  TestForConsistentPhase({100, 1000}, 8);
  TestForConsistentPhase({333, 6202}, 8);
}

TEST(MultiCrossoverFilter, PhaseTests_4WayCrossover) {
  TestForConsistentPhase({100, 1000,  5000}, 2);
  TestForConsistentPhase({333, 6202, 12345}, 2);
  TestForConsistentPhase({100, 1000,  5000}, 4);
  TestForConsistentPhase({333, 6202, 12345}, 4);
  TestForConsistentPhase({100, 1000,  5000}, 6);
  TestForConsistentPhase({333, 6202, 12345}, 6);
  TestForConsistentPhase({100, 1000,  5000}, 8);
  TestForConsistentPhase({333, 6202, 12345}, 8);
}

TEST(MultiCrossoverFilter, PhaseTests_5WayCrossover) {
  TestForConsistentPhase({100, 1000, 5000, 12000}, 2);
  TestForConsistentPhase({333, 1034, 6202, 12345}, 2);
  TestForConsistentPhase({100, 1000, 5000, 12000}, 4);
  TestForConsistentPhase({333, 1034, 6202, 12345}, 4);
  TestForConsistentPhase({100, 1000, 5000, 12000}, 6);
  TestForConsistentPhase({333, 1034, 6202, 12345}, 6);
  TestForConsistentPhase({100, 1000, 5000, 12000}, 8);
  TestForConsistentPhase({333, 1034, 6202, 12345}, 8);
}

TEST(MultiCrossoverFilter, PhaseTests_6WayCrossover) {
  TestForConsistentPhase({100, 450, 1000, 5000, 12000}, 2);
  TestForConsistentPhase({333, 650, 1034, 6202, 12345}, 2);
  TestForConsistentPhase({100, 450, 1000, 5000, 12000}, 4);
  TestForConsistentPhase({333, 650, 1034, 6202, 12345}, 4);
  TestForConsistentPhase({100, 450, 1000, 5000, 12000}, 6);
  TestForConsistentPhase({333, 650, 1034, 6202, 12345}, 6);
  TestForConsistentPhase({100, 450, 1000, 5000, 12000}, 8);
  TestForConsistentPhase({333, 650, 1034, 6202, 12345}, 8);
}

TEST(MultiCrossoverFilter, PhaseTests_10WayCrossover) {
  TestForConsistentPhase({50, 100, 200, 400, 800, 1600, 3200, 6400, 12800}, 2);
  TestForConsistentPhase({50, 100, 200, 400, 800, 1600, 3200, 6400, 12800}, 4);
  TestForConsistentPhase({50, 100, 200, 400, 800, 1600, 3200, 6400, 12800}, 6);
  TestForConsistentPhase({50, 100, 200, 400, 800, 1600, 3200, 6400, 12800}, 10);
}

Eigen::ArrayXd MakeSweep(double start_frequency_hz, double end_frequency_hz,
                          double duration_seconds, double sample_rate) {
  int n_steps = duration_seconds * sample_rate;
  Eigen::ArrayXd sweep = Eigen::ArrayXd::LinSpaced(n_steps, 1, n_steps);
  double octaves_per_second =  // Technically log2 would be an octave...
      log(end_frequency_hz / start_frequency_hz) / duration_seconds;
  double radians_per_sample =
      2 * M_PI * start_frequency_hz / octaves_per_second;
  sweep *= octaves_per_second * duration_seconds / static_cast<float>(n_steps);
  sweep = (radians_per_sample * sweep.exp()).sin();
  return sweep;
}

void TestForSweepFlatness(const std::vector<double>& crossover_frequencies,
                          int order, double tolerance) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRate = 48000.0f;
  const int num_bands = crossover_frequencies.size() + 1;
  MultiCrossoverFilter<double> filter(num_bands, order);

  filter.Init(kNumChannels, kSampleRate, crossover_frequencies);
  // Use a long max filter to estimate the envelope.
  double start_frequency = 100.0f;
  int max_filter_taps = kSampleRate / start_frequency;
  ABSL_CHECK_LT(max_filter_taps, 1000);  // Don't build too big of a filter.
  Eigen::ArrayXXd data = MakeSweep(100, 20000, 3, kSampleRate).transpose();
  Eigen::ArrayXXd output = Eigen::ArrayXXd::Zero(kNumChannels, data.cols());
  filter.ProcessBlock(data);
  for (int i = 0; i < num_bands; ++i) {
    output += filter.FilteredOutput(i);
  }
  CheckPhasesForEachBand(filter, crossover_frequencies);
  // Abs makes the estimate a bit smoother at low frequencies.
  Eigen::ArrayXd arr = output.abs().row(0);
  Eigen::ArrayXd envelope_estimate = Eigen::ArrayXd::Zero(data.cols());
  MaxFilter<double>(arr.data(), arr.size(), max_filter_taps,
                    envelope_estimate.data());
  int num_rows = std::round(0.99 * envelope_estimate.size());
  Eigen::ArrayXd most_of_envelope = envelope_estimate.bottomRows(num_rows);
  EXPECT_NEAR(most_of_envelope.maxCoeff(), 1, tolerance);
  EXPECT_NEAR(most_of_envelope.minCoeff(), 1, tolerance);
}

void TestForSplitMergeOutput(const std::vector<double>& crossover_frequencies,
                             int order, double tolerance) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRate = 48000.0f;
  const int num_bands = crossover_frequencies.size() + 1;
  MultiCrossoverFilter<double> filter(num_bands, order);

  filter.Init(kNumChannels, kSampleRate, crossover_frequencies);
  // Use a long max filter to estimate the envelope.
  double start_frequency = 100.0f;
  int max_filter_taps = kSampleRate / start_frequency;
  ABSL_CHECK_LT(max_filter_taps, 1000);  // Don't build too big of a filter.
  Eigen::ArrayXXd data = MakeSweep(100, 20000, 3, kSampleRate).transpose();
  Eigen::ArrayXXd output = Eigen::ArrayXXd::Zero(kNumChannels, data.cols());
  filter.ProcessBlock(data);
  for (int i = 0; i < num_bands; ++i) {
    output += filter.FilteredOutput(i);
  }

  std::vector<Eigen::ArrayXXd> filtered_output;
  Eigen::ArrayXXd split_merge_output =
      Eigen::ArrayXXd::Zero(kNumChannels, data.cols());
  filter.Init(kNumChannels, kSampleRate, crossover_frequencies);
  filter.SplitBands(data);
  filtered_output.resize(filter.num_bands());
  for (int i = 0; i < filter.num_bands(); ++i) {
    filtered_output[i] = filter.FilteredOutput(i);
  }
  filter.MergeBands(filtered_output,
                    Eigen::Map<Eigen::ArrayXXd>(split_merge_output.data(),
                                                split_merge_output.rows(),
                                                split_merge_output.cols()));

  EXPECT_THAT(output, audio_dsp::EigenArrayNear(split_merge_output, tolerance));
}

TEST(MultiCrossoverFilter, SweepFlatness_2WayCrossover) {
  TestForSweepFlatness({1000}, 2, 1e-2);
  TestForSweepFlatness({1000}, 4, 1e-2);
  TestForSweepFlatness({1000}, 6, 1e-2);
  TestForSweepFlatness({1000}, 8, 1e-2);
}

TEST(MultiCrossoverFilter, SweepFlatness_3WayCrossover) {
  TestForSweepFlatness({500, 3000}, 2, 1e-2);
  TestForSweepFlatness({500, 3000}, 4, 1e-2);
  TestForSweepFlatness({500, 3000}, 6, 1e-2);
  TestForSweepFlatness({500, 3000}, 8, 1e-2);
}

TEST(MultiCrossoverFilter, SweepFlatness_4WayCrossover) {
  TestForSweepFlatness({200, 2000, 8000}, 2, 1e-2);
  TestForSweepFlatness({200, 2000, 8000}, 4, 1e-2);
  TestForSweepFlatness({200, 2000, 8000}, 6, 1e-2);
  TestForSweepFlatness({200, 2000, 8000}, 8, 1e-2);
}

TEST(MultiCrossoverFilter, SweepFlatness_5WayCrossover) {
  // The tolerance starts to increase at 5-way.
  TestForSweepFlatness({300, 1200, 6000, 12000}, 2, 2e-2);
  TestForSweepFlatness({300, 1200, 6000, 12000}, 4, 2e-2);
  TestForSweepFlatness({300, 1200, 6000, 12000}, 6, 2e-2);
  TestForSweepFlatness({300, 1200, 6000, 12000}, 8, 2e-2);
}

TEST(MultiCrossoverFilter, SweepFlatness_6WayCrossover) {
  std::vector<double> crossover_frequencies =
      {300, 1200, 4000, 7000, 15000};
  TestForSweepFlatness(crossover_frequencies, 2, 6e-2);
  TestForSweepFlatness(crossover_frequencies, 4, 6e-2);
  TestForSweepFlatness(crossover_frequencies, 6, 6e-2);
  TestForSweepFlatness(crossover_frequencies, 8, 6e-2);
}

TEST(MultiCrossoverFilter, SweepFlatness_10WayCrossover) {
  std::vector<double> crossover_frequencies =
      {50, 100, 200, 400, 800, 1600, 3200, 6400, 12800};
  // Flatness error is within 22%.
  TestForSweepFlatness(crossover_frequencies, 2, 2.2e-1);
  TestForSweepFlatness(crossover_frequencies, 4, 2.2e-1);
  TestForSweepFlatness(crossover_frequencies, 6, 2.2e-1);
  TestForSweepFlatness(crossover_frequencies, 8, 2.2e-1);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_2WayCrossover) {
  TestForSplitMergeOutput({1000}, 2, 1e-6);
  TestForSplitMergeOutput({1000}, 4, 1e-6);
  TestForSplitMergeOutput({1000}, 6, 1e-6);
  TestForSplitMergeOutput({1000}, 8, 1e-6);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_3WayCrossover) {
  TestForSplitMergeOutput({500, 3000}, 2, 1e-6);
  TestForSplitMergeOutput({500, 3000}, 4, 1e-6);
  TestForSplitMergeOutput({500, 3000}, 6, 1e-6);
  TestForSplitMergeOutput({500, 3000}, 8, 1e-6);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_4WayCrossover) {
  TestForSplitMergeOutput({200, 2000, 8000}, 2, 1e-5);
  TestForSplitMergeOutput({200, 2000, 8000}, 4, 1e-5);
  TestForSplitMergeOutput({200, 2000, 8000}, 6, 1e-5);
  TestForSplitMergeOutput({200, 2000, 8000}, 8, 1e-5);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_5WayCrossover) {
  // The tolerance starts to increase at 5-way.
  TestForSplitMergeOutput({300, 1200, 6000, 12000}, 2, 1e-3);
  TestForSplitMergeOutput({300, 1200, 6000, 12000}, 4, 1e-3);
  TestForSplitMergeOutput({300, 1200, 6000, 12000}, 6, 1e-3);
  TestForSplitMergeOutput({300, 1200, 6000, 12000}, 8, 1e-3);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_6WayCrossover) {
  std::vector<double> crossover_frequencies = {300, 1200, 4000, 7000, 15000};
  TestForSplitMergeOutput(crossover_frequencies, 2, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 4, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 6, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 8, 1e-2);
}

TEST(MultiCrossoverFilter, SplitMergeOutput_10WayCrossover) {
  std::vector<double> crossover_frequencies = {50,   100,  200,  400,  800,
                                               1600, 3200, 6400, 12800};
  TestForSplitMergeOutput(crossover_frequencies, 2, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 4, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 6, 1e-2);
  TestForSplitMergeOutput(crossover_frequencies, 8, 1e-2);
}

TEST(MultiCrossoverFilter, CoefficientInterpolation) {
  constexpr int kNumChannels = 1;
  constexpr int kBands = 4;
  constexpr int kBlockSize = 4096;
  constexpr float kSampleRate = 48000.0f;
  MultiCrossoverFilter<float> expected(kBands, 4);
  MultiCrossoverFilter<float> actual(kBands, 4);
  std::vector<float> from_crossover_frequencies = {100, 1000, 10000};
  std::vector<float> to_crossover_frequencies = {200, 4000, 12000};
  expected.Init(kNumChannels, kSampleRate, to_crossover_frequencies);
  actual.Init(kNumChannels, kSampleRate, from_crossover_frequencies);

  Eigen::ArrayXXf data = Eigen::ArrayXXf::Zero(kNumChannels, kBlockSize);
  // An impulse.
  data(0, 0) = 1;
  actual.ProcessBlock(data);
  expected.ProcessBlock(data);

  actual.SetCrossoverFrequencies(to_crossover_frequencies);
  for (int i = 0; i < 10; ++i) {
    actual.ProcessBlock(data);
    expected.ProcessBlock(data);
  }
  // The coeffs should have interpolated to match the original filter quite
  // closely. The filter state would't be exactly the same.
  for (int i = 0; i < kBands; ++i) {
    ASSERT_THAT(
        actual.FilteredOutput(i),
        EigenArrayNear(expected.FilteredOutput(i), 1e-3));
  }
}

TEST(MultiCrossoverFilter, ResetTest) {
  constexpr int kNumChannels = 1;
  constexpr int kBands = 4;
  constexpr int kBlockSize = 512;
  constexpr float kSampleRate = 48000.0f;
  MultiCrossoverFilter<float> expected(kBands, 4);
  MultiCrossoverFilter<float> actual(kBands, 4);
  std::vector<float> crossover_frequencies = {100, 1000, 10000};
  expected.Init(kNumChannels, kSampleRate, crossover_frequencies);
  actual.Init(kNumChannels, kSampleRate, crossover_frequencies);

  Eigen::ArrayXXf data = Eigen::ArrayXXf::Random(kNumChannels, kBlockSize);

  actual.ProcessBlock(data);
  expected.ProcessBlock(data);

  // The output of the second block shouldn't be equal to the output of the
  // first block.
  actual.ProcessBlock(data);
  for (int i = 0; i < kBands; ++i) {
    ASSERT_THAT(
        actual.FilteredOutput(i),
        testing::Not(EigenArrayNear(expected.FilteredOutput(i), 1e-6)));
  }

  actual.Reset();

  actual.ProcessBlock(data);
  // The coeffs should have interpolated to match the original filter quite
  // closely. The filter state wouldn't be exactly the same.
  for (int i = 0; i < kBands; ++i) {
    ASSERT_THAT(
        actual.FilteredOutput(i),
        EigenArrayNear(expected.FilteredOutput(i), 1e-6));
  }
}

}  // namespace audio_dsp
