// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/windowed_analyzer.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/latency/fixed_point.h"
#include "ui/latency/frame_metrics_test_common.h"

namespace ui {
namespace frame_metrics {
namespace {

// Verify that the worst values for Mean, SMR, and RMS are all the same if
// every value added is the same. Makes for a nice sanity check.
TEST(FrameMetricsWindowedAnalyzerTest, AllResultsTheSame) {
  // For this test, we don't care about the timeline, so just keep it constant.
  TestWindowedAnalyzerClient client;
  SharedWindowedAnalyzerClient shared_client(
      60, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));

  // Try adding a single sample vs. multiple samples.
  for (size_t samples : {1u, 100u}) {
    // A power of 2 sweep for both the value and weight dimensions.
    for (uint64_t value = 1; value < 0x100000000ULL; value *= 2) {
      // Adding too many samples can result in overflow when multiplied by the
      // weight. Divide by samples to avoid overflow.
      for (uint64_t weight = 1; weight < 0x100000000ULL / samples;
           weight *= 2) {
        WindowedAnalyzer analyzer(&client, &shared_client);
        AddSamplesHelper(&analyzer, value, weight, samples);
        uint64_t expected_value =
            value * TestWindowedAnalyzerClient::result_scale;
        EXPECT_EQ(analyzer.ComputeWorstMean().value, expected_value)
            << value << " x " << weight;
        EXPECT_NEAR_SQRT_APPROX(analyzer.ComputeWorstRMS().value,
                                expected_value)
            << value << " x " << weight;
        EXPECT_NEAR_SQRT_APPROX(analyzer.ComputeWorstSMR().value,
                                expected_value)
            << value << " x " << weight;
      }
    }
  }

  // All min/max combinations of value and weight.
  for (uint64_t value : {0u, 0xFFFFFFFFu}) {
    for (uint64_t weight : {1u, 0xFFFFFFFFu}) {
      const size_t kSamplesToAdd = weight == 1 ? 100 : 1;
      WindowedAnalyzer analyzer(&client, &shared_client);
      AddSamplesHelper(&analyzer, value, weight, kSamplesToAdd);

      // TestWindowedAnalyzerClient scales the result by 2.
      uint64_t expected_value =
          value * TestWindowedAnalyzerClient::result_scale;
      // Makes sure our precision is good enough.
      EXPECT_EQ(analyzer.ComputeWorstMean().value, expected_value)
          << value << " x " << weight;
      EXPECT_NEAR_SQRT_APPROX(analyzer.ComputeWorstRMS().value, expected_value)
          << value << " x " << weight;
      EXPECT_NEAR_SQRT_APPROX(analyzer.ComputeWorstSMR().value, expected_value)
          << value << " x " << weight;
    }
  }
}

// Verify that the worst values and their time regions are properly tracked
// seperately for mean, SMR, and RMS.
TEST(FrameMetricsWindowedAnalyzerTest, AllResultsDifferent) {
  const size_t kMaxWindowSize = 6;  // Same as the pattern length.
  const uint32_t kSampleWeight = 100;

  TestWindowedAnalyzerClient client;
  SharedWindowedAnalyzerClient shared_client(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  WindowedAnalyzer analyzer(&client, &shared_client);

  // Used to "clear" all the windowed accumulators.
  const std::vector<uint32_t> pattern_clear = {0, 0, 0, 0, 0, 0};
  // Worst mean pattern: mean of 3, smr of 1.5, rms of ~4.2.
  const std::vector<uint32_t> pattern_max_mean = {0, 6, 0, 6, 0, 6};
  double expected_worst_mean =
      3 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  // Lots of small janks maximizes the SMR.
  // Worst SMR pattern: mean of 2, smr of 2, rms of 2.
  const std::vector<uint32_t> pattern_max_smr = {2, 2, 2, 2, 2, 2};
  double expected_worst_smr =
      2 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  // A few big janks dominate RMS.
  // Worst RMS pattern: Mean of 2, smr of ~.3, rms of ~4.9
  const std::vector<uint32_t> pattern_max_rms = {0, 0, 0, 0, 0, 12};
  double expected_worst_rms = std::sqrt((12 * 12) / 6) * kFixedPointMultiplier *
                              TestWindowedAnalyzerClient::result_scale;

  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_mean, kSampleWeight);
  SharedWindowedAnalyzerClient worst_mean_client(shared_client);

  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_smr, kSampleWeight);
  SharedWindowedAnalyzerClient worst_smr_client(shared_client);

  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_rms, kSampleWeight);
  SharedWindowedAnalyzerClient worst_rms_client(shared_client);

  // If there is a tie, the first window detected wins.
  // This can go wrong if there's any accumulation of error because the
  // values added aren't exactly the same as the values removed.
  // This only catches accumulation of error in one direction, so isn't
  // thorough, but it does help improve coverage.
  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_mean, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_smr, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_max_rms, kSampleWeight);
  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);

  FrameRegionResult worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_worst_mean, worst_mean.value);
  EXPECT_EQ(worst_mean_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(worst_mean_client.window_end, worst_mean.window_end);

  FrameRegionResult worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_worst_smr, worst_smr.value);
  EXPECT_EQ(worst_smr_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(worst_smr_client.window_end, worst_smr.window_end);

  FrameRegionResult worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_worst_rms, worst_rms.value);
  EXPECT_EQ(worst_rms_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(worst_rms_client.window_end, worst_rms.window_end);
}

// Verify that the worst values and their time regions are properly tracked
// even before a full window's worth is available.
TEST(FrameMetricsWindowedAnalyzerTest, SmallSampleSize) {
  const size_t kMaxWindowSize = 6;  // Bigger than the pattern length.
  const uint32_t kSampleWeight = 100;

  TestWindowedAnalyzerClient client;
  SharedWindowedAnalyzerClient shared_client(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  WindowedAnalyzer analyzer(&client, &shared_client);

  const std::vector<uint32_t> pattern_short = {2, 2, 2};
  double expected_initial_value =
      2 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  AddPatternHelper(&shared_client, &analyzer, pattern_short, kSampleWeight);
  SharedWindowedAnalyzerClient short_client(shared_client);

  FrameRegionResult worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_initial_value, worst_mean.value);
  EXPECT_EQ(short_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(short_client.window_end, worst_mean.window_end);

  FrameRegionResult worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_smr.value);
  EXPECT_EQ(short_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(short_client.window_end, worst_smr.window_end);

  FrameRegionResult worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_rms.value);
  EXPECT_EQ(short_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(short_client.window_end, worst_rms.window_end);
}

// Verify that a few bad values at the start don't dominate the result.
TEST(FrameMetricsWindowedAnalyzerTest, BadFirstSamples) {
  const size_t kMaxWindowSize = 6;
  const uint32_t kSampleWeight = 100;
  FrameRegionResult worst_mean, worst_smr, worst_rms;

  TestWindowedAnalyzerClient client;
  SharedWindowedAnalyzerClient shared_client(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  WindowedAnalyzer analyzer(&client, &shared_client);

  // The 7's at the start will dominate the result if the implemenationd
  // doesn't only start remembering the worst values after receiving at least
  // a window's worth of samples.
  const std::vector<uint32_t> pattern_short = {7, 7};
  double expected_initial_value =
      7 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  AddPatternHelper(&shared_client, &analyzer, pattern_short, kSampleWeight);
  SharedWindowedAnalyzerClient short_client(shared_client);

  worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_initial_value, worst_mean.value);
  EXPECT_EQ(short_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(short_client.window_end, worst_mean.window_end);

  worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_smr.value);
  EXPECT_EQ(short_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(short_client.window_end, worst_smr.window_end);

  worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_rms.value);
  EXPECT_EQ(short_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(short_client.window_end, worst_rms.window_end);

  // Clear the window.
  const std::vector<uint32_t> pattern_clear = {0, 0, 0, 0, 0, 0};
  AddPatternHelper(&shared_client, &analyzer, pattern_clear, kSampleWeight);

  // Make sure a new worst window with results less than 7 is detected.
  const std::vector<uint32_t> pattern_long = {6, 6, 6, 6, 6, 6};
  double expected_final_value =
      6 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  AddPatternHelper(&shared_client, &analyzer, pattern_long, kSampleWeight);
  SharedWindowedAnalyzerClient long_client(shared_client);

  worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_final_value, worst_mean.value);
  EXPECT_EQ(long_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(long_client.window_end, worst_mean.window_end);

  worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_final_value, worst_smr.value);
  EXPECT_EQ(long_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(long_client.window_end, worst_smr.window_end);

  worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_final_value, worst_rms.value);
  EXPECT_EQ(long_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(long_client.window_end, worst_rms.window_end);
}

// Verify ResetAccumulators is continuous across the reset boundary.
TEST(FrameMetricsWindowedAnalyzerTest, ResetWorstValues) {
  const size_t kMaxWindowSize = 6;  // Same as the pattern length.
  const uint32_t kSampleWeight = 100;
  FrameRegionResult worst_mean, worst_smr, worst_rms;

  TestWindowedAnalyzerClient client;
  SharedWindowedAnalyzerClient shared_client(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  WindowedAnalyzer analyzer(&client, &shared_client);

  // Start off with the worst pattern.
  const std::vector<uint32_t> pattern1 = {9, 9, 9, 9, 9, 9};
  double expected_initial_value =
      9 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  AddPatternHelper(&shared_client, &analyzer, pattern1, kSampleWeight);
  SharedWindowedAnalyzerClient initial_client(shared_client);

  worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_initial_value, worst_mean.value);
  EXPECT_EQ(initial_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_mean.window_end);

  worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_smr.value);
  EXPECT_EQ(initial_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_smr.window_end);

  worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_rms.value);
  EXPECT_EQ(initial_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_rms.window_end);

  // The 4's below will affect the window, even after a reset, but
  // won't affect the current worst values.
  const std::vector<uint32_t> pattern2 = {4, 4, 4, 4, 4, 4};
  AddPatternHelper(&shared_client, &analyzer, pattern2, kSampleWeight);

  worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_initial_value, worst_mean.value);
  EXPECT_EQ(initial_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_mean.window_end);

  worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_smr.value);
  EXPECT_EQ(initial_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_smr.window_end);

  worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_initial_value, worst_rms.value);
  EXPECT_EQ(initial_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(initial_client.window_end, worst_rms.window_end);

  // Reset the worst value. This should not destroy sample history or
  // any accumulators.
  analyzer.ResetWorstValues();

  // The first 4 below will be included with the previous 4's to detect an
  // entire window of results, even though we've reset the worst values.
  const std::vector<uint32_t> pattern3 = {4};
  double expected_final_value =
      4 * kFixedPointMultiplier * TestWindowedAnalyzerClient::result_scale;
  AddPatternHelper(&shared_client, &analyzer, pattern3, kSampleWeight);
  SharedWindowedAnalyzerClient final_client(shared_client);

  // Add a window of 1's here to verify it does not affect the window of 4's.
  const std::vector<uint32_t> pattern4 = {1, 1, 1, 1, 1, 1};
  AddPatternHelper(&shared_client, &analyzer, pattern4, kSampleWeight);

  worst_mean = analyzer.ComputeWorstMean();
  EXPECT_DOUBLE_EQ(expected_final_value, worst_mean.value);
  EXPECT_EQ(final_client.window_begin, worst_mean.window_begin);
  EXPECT_EQ(final_client.window_end, worst_mean.window_end);

  worst_smr = analyzer.ComputeWorstSMR();
  EXPECT_NEAR_SQRT_APPROX(expected_final_value, worst_smr.value);
  EXPECT_EQ(final_client.window_begin, worst_smr.window_begin);
  EXPECT_EQ(final_client.window_end, worst_smr.window_end);

  worst_rms = analyzer.ComputeWorstRMS();
  EXPECT_NEAR_SQRT_APPROX(expected_final_value, worst_rms.value);
  EXPECT_EQ(final_client.window_begin, worst_rms.window_begin);
  EXPECT_EQ(final_client.window_end, worst_rms.window_end);
}

// WindowedAnalyzerNaive is a version of WindowedAnalyzer that doesn't use
// fixed point math and can accumulate error, even with double precision
// accumulators. This is used to verify patterns that accumulate error without
// fixed point math, so we can then verify those patterns don't result in
// acculated error in the actual implementation.
class WindowedAnalyzerNaive {
 public:
  WindowedAnalyzerNaive(size_t max_window_size)
      : max_window_size_(max_window_size) {}

  void AddSample(uint32_t value,
                 uint32_t weight,
                 uint64_t weighted_value,
                 uint64_t weighted_root,
                 const Accumulator96b& weighted_square) {
    if (history_.size() >= max_window_size_) {
      Sample old = history_.front();
      history_.pop_front();
      naive_accumulator_ -= static_cast<double>(old.weight) * old.value;
      naive_root_accumulator_ -=
          static_cast<double>(old.weight) * std::sqrt(old.value);
      naive_square_accumulator_ -=
          static_cast<double>(old.weight) * old.value * old.value;
      naive_total_weight_ -= old.weight;
    }

    history_.push_back({value, weight});
    naive_accumulator_ += static_cast<double>(weight) * value;
    naive_root_accumulator_ += static_cast<double>(weight) * std::sqrt(value);
    naive_square_accumulator_ += static_cast<double>(weight) * value * value;
    naive_total_weight_ += weight;
  }

  // Same as AddPatternHelper, but uses each value (+1) as its own weight.
  // The "Cubed" name comes from the fact that the squared_accumulator
  // for the RMS will effectively be a "cubed accumulator".
  void AddCubedPatternHelper(SharedWindowedAnalyzerClient* shared_client,
                             const std::vector<uint32_t>& values) {
    for (auto i : values) {
      shared_client->window_begin += base::TimeDelta::FromMicroseconds(1);
      shared_client->window_end += base::TimeDelta::FromMicroseconds(1);
      uint64_t weighted_value = (i * (i + 1));
      uint64_t updated_value = static_cast<uint64_t>(i);
      uint64_t weighted_root = (i + 1) * std::sqrt(updated_value << 32);
      Accumulator96b weighted_square(i, (i + 1));
      AddSample(i, (i + 1), weighted_value, weighted_root, weighted_square);
    }
  }

  struct Sample {
    uint32_t value;
    uint32_t weight;
  };

  const size_t max_window_size_;
  double naive_accumulator_ = 0;
  double naive_root_accumulator_ = 0;
  double naive_square_accumulator_ = 0;
  double naive_total_weight_ = 0;
  base::circular_deque<Sample> history_;
};

// A version of the WindowedAnalyzer that allows us to inspect the internal
// state for testing purposes.
class TestWindowedAnalyzer : public WindowedAnalyzer {
 public:
  TestWindowedAnalyzer(const WindowedAnalyzerClient* client,
                       const SharedWindowedAnalyzerClient* shared_client)
      : WindowedAnalyzer(client, shared_client) {}
  ~TestWindowedAnalyzer() override {}

  double CurrentAccumulator() { return accumulator_; }
  double CurrentRootAccumulator() { return root_accumulator_; }
  double CurrentSquareAccumulator() { return square_accumulator_.ToDouble(); }
};

// This test verifies that it's easy to blow the dynamic range of a floating
// point accumulator with a particular pattern. Then it verifies that same
// pattern does not result in error in the actual implementation.
void TestNoAccumulatedPrecisionError(uint32_t big_value,
                                     uint32_t small_value,
                                     double naive_root_error_floor,
                                     double naive_square_error_floor) {
  const size_t kRuns = 1000;
  const size_t kMaxWindowSize = 6;  // Same as the pattern length.
  const std::vector<uint32_t> pattern_clear = {0, 0, 0, 0, 0, 0};
  const std::vector<uint32_t> pattern_bad = {big_value,   small_value,
                                             small_value, small_value,
                                             small_value, small_value};

  // Set up the actual WindowedAnalyzer implementation.
  TestWindowedAnalyzerClient client_impl;
  SharedWindowedAnalyzerClient shared_client_impl(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  TestWindowedAnalyzer analyzer_impl(&client_impl, &shared_client_impl);

  // Set up the naive WindowedAnalyzer implementation.
  SharedWindowedAnalyzerClient shared_client_naive(
      kMaxWindowSize, base::TimeTicks(),
      base::TimeTicks() + base::TimeDelta::FromSeconds(1));
  WindowedAnalyzerNaive analyzer_naive(kMaxWindowSize);

  // Verify error keeps accumulating each time the bad pattern is applied.
  // Note: We don't expect error in the mean accumulator since the values added
  // are already truncated to whole integers before being passed in via
  // AddSamples().
  double naive_root_accumulator_prev = 0;
  double naive_square_accumulator_prev = 0;
  for (size_t i = 1; i <= kRuns; i++) {
    analyzer_naive.AddCubedPatternHelper(&shared_client_naive, pattern_bad);
    analyzer_naive.AddCubedPatternHelper(&shared_client_naive, pattern_clear);
    EXPECT_EQ(0, analyzer_naive.naive_accumulator_);
    EXPECT_ABS_LT(naive_root_accumulator_prev,
                  analyzer_naive.naive_root_accumulator_);
    EXPECT_ABS_LT(naive_square_accumulator_prev,
                  analyzer_naive.naive_square_accumulator_);
    naive_root_accumulator_prev = analyzer_naive.naive_root_accumulator_;
    naive_square_accumulator_prev = analyzer_naive.naive_square_accumulator_;
  }
  // Verify naive error is bigger than some threshold after kRuns.
  EXPECT_ABS_LE(naive_root_error_floor * kRuns,
                analyzer_naive.naive_root_accumulator_);
  EXPECT_ABS_LE(naive_square_error_floor * kRuns,
                analyzer_naive.naive_square_accumulator_);
}

// This is a synthetic example that is just outside the dynamic range of a
// double accumulator. Doubles have 53 significand bits. When cubed, the
// difference between the small and big values below require just over 53 bits.
TEST(FrameMetricsWindowedAnalyzerTest, NoAccumulatedPrecisionErrorBasic) {
  constexpr uint32_t big = 1 << 19;
  constexpr uint32_t small = 2;
  TestNoAccumulatedPrecisionError(big, small, 5e-8, 60);
}

// This is a more realistic scenario with orders of magnitude we are likely
// to see in actual data. The error is small, but can become significant over
// time.
TEST(FrameMetricsWindowedAnalyzerTest, NoAccumulatedPrecisionErrorBig) {
  constexpr uint32_t big = 1 * base::TimeTicks::kMicrosecondsPerSecond;
  constexpr uint32_t small = 1 * base::TimeTicks::kMicrosecondsPerMillisecond;
  TestNoAccumulatedPrecisionError(big, small, 7e-8, 256);
}

// This is a scenario with orders of magnitude that we can see in our data,
// but that will be rare. Even after a single bad pattern, the error is
// significant.
TEST(FrameMetricsWindowedAnalyzerTest, NoAccumulatedPrecisionErrorBigger) {
  constexpr uint32_t big = 20 * base::TimeTicks::kMicrosecondsPerSecond;
  constexpr uint32_t small = 1 * base::TimeTicks::kMicrosecondsPerMillisecond;
  TestNoAccumulatedPrecisionError(big, small, 2e-5, 1e6);
}

}  // namespace
}  // namespace frame_metrics
}  // namespace ui
