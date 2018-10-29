// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/stream_analyzer.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ui/latency/frame_metrics_test_common.h"

namespace ui {
namespace frame_metrics {
namespace {

class StreamAnalyzerTest : public testing::Test {
 public:
  StreamAnalyzerTest() { NewAnalyzer(10, {2, 7, 10}); }

  void SetUp() override {}

  StreamAnalyzer* analyzer() { return analyzer_.get(); }

  void NewAnalyzer(size_t window_size, std::vector<uint32_t> thresholds) {
    shared_client_.max_window_size = window_size;
    for (auto& t : thresholds) {
      t *= kFixedPointMultiplier;
    }
    thresholds_ = std::move(thresholds);
    std::unique_ptr<TestHistogram> histogram =
        std::make_unique<TestHistogram>();
    histogram_ = histogram.get();
    analyzer_ = std::make_unique<StreamAnalyzer>(
        &client_, &shared_client_, thresholds_, std::move(histogram));
  }

 protected:
  size_t window_size;
  TestStreamAnalyzerClient client_;
  SharedWindowedAnalyzerClient shared_client_;
  std::vector<uint32_t> thresholds_;
  TestHistogram* histogram_;
  std::unique_ptr<StreamAnalyzer> analyzer_;
};

TEST_F(StreamAnalyzerTest, AllResultsTheSame) {
  const double approx_sqrt_error = 0.0001;
  // Try adding a single sample vs. multiple samples.
  for (size_t samples : {1u, 100u}) {
    // A power of 2 sweep for both the value and weight dimensions.
    for (uint64_t value = 1; value < 0x100000000ULL; value *= 2) {
      // Adding too many samples can result in overflow when multiplied by the
      // weight. Divide by samples to avoid overflow.
      for (uint64_t weight = 1; weight < 0x100000000ULL / samples;
           weight *= 2) {
        analyzer()->Reset();
        AddSamplesHelper(analyzer(), value, weight, samples);
        uint64_t expected_value =
            value * TestStreamAnalyzerClient::result_scale;
        EXPECT_EQ(expected_value, analyzer_->ComputeMean());
        EXPECT_NEAR_SQRT_APPROX(expected_value, analyzer_->ComputeRMS());
        EXPECT_NEAR_SQRT_APPROX(analyzer_->ComputeSMR(), expected_value);
        EXPECT_NEAR_SQRT_APPROX(0, analyzer_->ComputeStdDev());
        EXPECT_NEAR(0, analyzer_->ComputeVarianceOfRoots(),
                    approx_sqrt_error * value);

        // Verify values are forwarded to the WindowedAnalyzer.
        EXPECT_EQ(expected_value, analyzer_->window().ComputeWorstMean().value);
        EXPECT_NEAR_SQRT_APPROX(expected_value,
                                analyzer_->window().ComputeWorstRMS().value);
        EXPECT_NEAR_SQRT_APPROX(expected_value,
                                analyzer_->window().ComputeWorstSMR().value);
      }
    }
  }

  // All min/max combinations of value and weight.
  for (uint64_t value : {0u, 0xFFFFFFFFu}) {
    for (uint64_t weight : {1u, 0xFFFFFFFFu}) {
      const size_t kSamplesToAdd = weight == 1 ? 100 : 1;
      analyzer()->Reset();
      AddSamplesHelper(analyzer(), value, weight, kSamplesToAdd);

      // TestWindowedAnalyzerClient scales the result by 2.
      uint64_t expected_value = value * TestStreamAnalyzerClient::result_scale;
      // Makes sure our precision is good enough.
      EXPECT_EQ(expected_value, analyzer_->ComputeMean());
      EXPECT_NEAR_SQRT_APPROX(expected_value, analyzer_->ComputeRMS());
      EXPECT_NEAR_SQRT_APPROX(expected_value, analyzer_->ComputeSMR());
      EXPECT_NEAR_SQRT_APPROX(0, analyzer_->ComputeStdDev());
      EXPECT_NEAR(0, analyzer_->ComputeVarianceOfRoots(),
                  approx_sqrt_error * value);

      // Verify values are forwarded to the WindowedAnalyzer.
      EXPECT_EQ(expected_value, analyzer_->window().ComputeWorstMean().value);
      EXPECT_NEAR_SQRT_APPROX(expected_value,
                              analyzer_->window().ComputeWorstRMS().value);
      EXPECT_NEAR_SQRT_APPROX(expected_value,
                              analyzer_->window().ComputeWorstSMR().value);
    }
  }
}

// This applies a pattern of 2 values that are easy to calculate the expected
// results for. It verifies the mean, rms, smr, standard deviation,
// variance of the roots, and thresholds are calculated properly.
// This doesn't check histogram or windowed analyzer related values since they
// are tested separately and other unit tests verify their interactions
// with StreamAnalyzer.
TEST_F(StreamAnalyzerTest, AllResultsDifferent) {
  const uint32_t kSampleWeight = 100;

  const std::vector<uint32_t> pattern49 = {4, 9, 4, 9, 4, 9};
  const std::vector<uint32_t> pattern4 = {4, 4, 4, 4, 4, 4};
  const std::vector<uint32_t> pattern9 = {9, 9, 9, 9, 9, 9};

  // Calculate the expected values for an equal number of 4's and 9's.
  const double expected_mean = (4 + 9) * .5 * kFixedPointMultiplier *
                               TestStreamAnalyzerClient::result_scale;
  const double expected_rms = std::sqrt((16 + 81) * .5) *
                              kFixedPointMultiplier *
                              TestStreamAnalyzerClient::result_scale;
  const double mean_root = (2 + 3) * .5;
  const double expected_smr = mean_root * mean_root * kFixedPointMultiplier *
                              TestStreamAnalyzerClient::result_scale;
  const double expected_std_dev = (9 - 4) * .5 * kFixedPointMultiplier *
                                  TestStreamAnalyzerClient::result_scale;
  const double std_dev_of_roots = (3 - 2) * .5;
  const double expected_variance_of_roots =
      std_dev_of_roots * std_dev_of_roots * kFixedPointMultiplier *
      TestStreamAnalyzerClient::result_scale;

  std::vector<ThresholdResult> thresholds;

  // Alternate 4 and 9.
  for (size_t i = 0; i < 1000; i++) {
    AddPatternHelper(&shared_client_, analyzer(), pattern49, kSampleWeight);
    EXPECT_DOUBLE_EQ(expected_mean, analyzer_->ComputeMean());
    // since each value creates a difference of 0.001, the result should
    EXPECT_NEAR_SQRT_APPROX(expected_smr, analyzer_->ComputeSMR());
    EXPECT_NEAR_SQRT_APPROX(expected_rms, analyzer_->ComputeRMS());
    EXPECT_NEAR_SQRT_APPROX(expected_std_dev, analyzer_->ComputeStdDev());
    EXPECT_NEAR_SQRT_APPROX(expected_variance_of_roots,
                            analyzer_->ComputeVarianceOfRoots());
  }
  thresholds = analyzer_->ComputeThresholds();
  ASSERT_EQ(3u, thresholds.size());
  EXPECT_EQ(client_.TransformResult(thresholds_[0]), thresholds[0].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[1]), thresholds[1].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[2]), thresholds[2].threshold);
  EXPECT_EQ(1.0, thresholds[0].ge_fraction);
  EXPECT_EQ(0.5, thresholds[1].ge_fraction);
  EXPECT_EQ(0.0, thresholds[2].ge_fraction);

  // 4's then 9's.
  analyzer()->Reset();
  for (size_t i = 0; i < 500; i++) {
    AddPatternHelper(&shared_client_, analyzer(), pattern4, kSampleWeight);
  }
  for (size_t i = 0; i < 500; i++) {
    AddPatternHelper(&shared_client_, analyzer(), pattern9, kSampleWeight);
  }
  thresholds = analyzer_->ComputeThresholds();
  EXPECT_DOUBLE_EQ(expected_mean, analyzer_->ComputeMean());
  EXPECT_NEAR_SQRT_APPROX(expected_smr, analyzer_->ComputeSMR());
  EXPECT_NEAR_SQRT_APPROX(expected_rms, analyzer_->ComputeRMS());
  EXPECT_NEAR_SQRT_APPROX(expected_std_dev, analyzer_->ComputeStdDev());
  EXPECT_NEAR_SQRT_APPROX(expected_variance_of_roots,
                          analyzer_->ComputeVarianceOfRoots());
  EXPECT_EQ(client_.TransformResult(thresholds_[0]), thresholds[0].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[1]), thresholds[1].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[2]), thresholds[2].threshold);
  EXPECT_EQ(1.0, thresholds[0].ge_fraction);
  EXPECT_EQ(0.5, thresholds[1].ge_fraction);
  EXPECT_EQ(0.0, thresholds[2].ge_fraction);

  // 9's then 4's.
  analyzer()->Reset();
  for (size_t i = 0; i < 500; i++) {
    AddPatternHelper(&shared_client_, analyzer(), pattern9, kSampleWeight);
  }
  for (size_t i = 0; i < 500; i++) {
    AddPatternHelper(&shared_client_, analyzer(), pattern4, kSampleWeight);
  }
  thresholds = analyzer_->ComputeThresholds();
  EXPECT_DOUBLE_EQ(expected_mean, analyzer_->ComputeMean());
  EXPECT_NEAR_SQRT_APPROX(expected_smr, analyzer_->ComputeSMR());
  EXPECT_NEAR_SQRT_APPROX(expected_rms, analyzer_->ComputeRMS());
  EXPECT_NEAR_SQRT_APPROX(expected_std_dev, analyzer_->ComputeStdDev());
  EXPECT_NEAR_SQRT_APPROX(expected_variance_of_roots,
                          analyzer_->ComputeVarianceOfRoots());
  EXPECT_EQ(client_.TransformResult(thresholds_[0]), thresholds[0].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[1]), thresholds[1].threshold);
  EXPECT_EQ(client_.TransformResult(thresholds_[2]), thresholds[2].threshold);
  EXPECT_EQ(1.0, thresholds[0].ge_fraction);
  EXPECT_EQ(0.5, thresholds[1].ge_fraction);
  EXPECT_EQ(0.0, thresholds[2].ge_fraction);
}

TEST_F(StreamAnalyzerTest, SamplesForwardedToHistogram) {
  const uint32_t kSampleWeight = 123;
  const std::vector<uint32_t> pattern = {4, 9, 16, 25, 36, 49};
  AddPatternHelper(&shared_client_, analyzer(), pattern, kSampleWeight);
  std::vector<TestHistogram::ValueWeightPair> samples(
      histogram_->GetAndResetAllAddedSamples());
  ASSERT_EQ(pattern.size(), samples.size());
  for (size_t i = 0; i < samples.size(); i++) {
    EXPECT_EQ(pattern[i] * kFixedPointMultiplier, samples[i].value);
    EXPECT_EQ(kSampleWeight, samples[i].weight);
  }
}

TEST_F(StreamAnalyzerTest, PercentilesModifiedByClient) {
  double result0 = 7;
  double result1 = 11;
  histogram_->SetResults({{result0, result1}});
  PercentileResults results = analyzer()->ComputePercentiles();
  EXPECT_EQ(client_.TransformResult(result0), results.values[0]);
  EXPECT_EQ(client_.TransformResult(result1), results.values[1]);
}

// StreamAnalyzerNaive is a subset of stream analyzer that only uses single
// precision floating point accumulators and can accumulate error.
// This is used to verify patterns that accumulate error, so we can then verify
// those patterns don't result in acculated error in the actual implementation.
struct StreamAnalyzerNaive {
  void AddSample(uint32_t value,
                 uint32_t weight,
                 uint64_t weighted_value,
                 uint64_t weighted_root,
                 const Accumulator96b& weighted_square) {
    accumulator_ += static_cast<double>(weight) * value;
    root_accumulator_ += static_cast<double>(weight) * std::sqrt(value);
    square_accumulator_ += static_cast<double>(weight) * value * value;
    total_weight_ += weight;
  }

  double ComputeMean() {
    return client_.TransformResult(accumulator_ / total_weight_);
  }
  double ComputeRMS() {
    return client_.TransformResult(
        std::sqrt(square_accumulator_ / total_weight_));
  }
  double ComputeSMR() {
    double mean_root = root_accumulator_ / total_weight_;
    return client_.TransformResult(mean_root * mean_root);
  }

  float total_weight_ = 0;
  float accumulator_ = 0;
  float root_accumulator_ = 0;
  float square_accumulator_ = 0;

  TestStreamAnalyzerClient client_;
};

// Unlike the WindowedAnalyzer, there aren't patterns of input that would
// affect the precision of our results very much with double precision floating
// point accumulators. This is because we aren't subtracting values like the
// WindowedAnalyzer does. Nevertheless, there can be issues if the accumulators
// are only single precision.
TEST_F(StreamAnalyzerTest, Precision) {
  StreamAnalyzerNaive naive_analyzer;

  uint32_t large_value = 20 * base::TimeTicks::kMicrosecondsPerSecond;
  uint32_t large_weight = large_value;
  size_t large_sample_count = 1;
  AddSamplesHelper(&naive_analyzer, large_value, large_weight,
                   large_sample_count);
  AddSamplesHelper(analyzer(), large_value, large_weight, large_sample_count);

  uint32_t small_value = 1 * base::TimeTicks::kMicrosecondsPerMillisecond;
  uint32_t small_weight = small_value;
  size_t small_sample_count = 60 * 60 * 60;  // 1hr of 60Hz frames.
  AddSamplesHelper(&naive_analyzer, small_value, small_weight,
                   small_sample_count);
  AddSamplesHelper(analyzer(), small_value, small_weight, small_sample_count);

  double total_weight = static_cast<double>(large_sample_count) * large_weight +
                        static_cast<double>(small_sample_count) * small_weight;

  double large_value_f = large_value;
  double small_value_f = small_value;

  double expected_mean = client_.TransformResult(
      (large_value_f * large_weight +
       small_sample_count * small_value_f * small_weight) /
      total_weight);
  EXPECT_ABS_LT(expected_mean * .001,
                expected_mean - naive_analyzer.ComputeMean());
  EXPECT_DOUBLE_EQ(expected_mean, analyzer_->ComputeMean());

  double large_value_squared = large_value_f * large_value_f * large_weight;
  double small_value_squared = small_value_f * small_value_f * small_weight;
  double mean_square =
      (large_value_squared + small_sample_count * small_value_squared) /
      total_weight;
  double expected_rms = client_.TransformResult(std::sqrt(mean_square));
  EXPECT_ABS_LT(expected_rms * .001,
                expected_rms - naive_analyzer.ComputeRMS());
  EXPECT_NEAR_SQRT_APPROX(expected_rms, analyzer_->ComputeRMS());

  double large_value_root = std::sqrt(large_value_f) * large_weight;
  double small_value_root = std::sqrt(small_value_f) * small_weight;
  double mean_root =
      (large_value_root + small_sample_count * small_value_root) / total_weight;
  double expected_smr = client_.TransformResult(mean_root * mean_root);
  EXPECT_ABS_LT(expected_smr * .001,
                expected_smr - naive_analyzer.ComputeSMR());
  EXPECT_NEAR_SQRT_APPROX(expected_smr, analyzer_->ComputeSMR());
}

}  // namespace
}  // namespace frame_metrics
}  // namespace ui
