// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/frame_metrics.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ui/latency/frame_metrics_test_common.h"

namespace ui {
namespace frame_metrics {
namespace {

// Converts a skipped:produced ratio into skipped:total, where
// total = skipped + produced.
// Internally we store the skipped:produced ratio since it is linear with
// the amount of time skipped, which has benefits for the fixed point
// representation as well as how it affects the RMS value.
// However, at a high level, we are more interested in the percent of total
// time skipped which is easier to interpret.
constexpr double SkipTransform(double ratio) {
  return 1.0 / (1.0 + (1.0 / ratio));
}

// Returns the max value of an N-bit unsigned number.
constexpr uint64_t MaxValue(int N) {
  return (1ULL << N) - 1;
}

// Define lower bounds on the saturation values of each metric.
// They are much bigger than they need to be, which ensures the range of our
// metrics will be okay.
// The constants passed to MaxValue represent the number of bits before
// the radix point in each metric's fixed-point representation.
constexpr double kSkipSaturationMin =
    SkipTransform(MaxValue(16));  // skipped : frame delta = 65535
constexpr double kLatencySaturationMin =
    MaxValue(32) / base::TimeTicks::kMicrosecondsPerSecond;  // 4294.96 seconds
constexpr double kSpeedSaturationMin =
    MaxValue(16);  // latency delta : frame delta = 65535
constexpr double kAccelerationSaturationMin =
    MaxValue(16) * base::TimeTicks::kMicrosecondsPerSecond /
    1024;  // speed delta : frame delta ~= 64M

// Define upper bounds for saturation points so we can verify the tests
// are testing what they think they are testing.
constexpr double kSkipSaturationMax = kSkipSaturationMin * 1.01;
constexpr double kLatencySaturationMax = kLatencySaturationMin * 1.01;
constexpr double kSpeedSaturationMax = kSpeedSaturationMin * 1.01;
constexpr double kAccelerationSaturationMax = kAccelerationSaturationMin * 1.01;

// TestFrameMetrics overrides some behavior of FrameMetrics for testing
// purposes.
class TestFrameMetrics : public FrameMetrics {
 public:
  TestFrameMetrics(const FrameMetricsSettings& settings)
      : FrameMetrics(settings) {}
  ~TestFrameMetrics() override = default;

  void OverrideReportPeriod(base::TimeDelta period) {
    report_period_override_ = period;
  }

  void UseDefaultReportPeriodScaled(int scale) {
    report_period_override_ = scale * FrameMetrics::ReportPeriod();
  }

  // AtStartOfNewReportPeriod works assuming it is called after every frame
  // is submitted.
  bool AtStartOfNewReportPeriod() {
    bool at_start = time_since_start_of_report_period_ <
                    time_since_start_of_report_period_previous_;
    time_since_start_of_report_period_previous_ =
        time_since_start_of_report_period_;
    return at_start;
  }

  // Convenience accessors for testing.
  const frame_metrics::StreamAnalyzer& skips() const {
    return frame_skips_analyzer_;
  }
  const frame_metrics::StreamAnalyzer& latency() const {
    return latency_analyzer_;
  }
  const frame_metrics::StreamAnalyzer& speed() const {
    return latency_speed_analyzer_;
  }
  const frame_metrics::StreamAnalyzer& acceleration() const {
    return latency_acceleration_analyzer_;
  }

 protected:
  base::TimeDelta ReportPeriod() override { return report_period_override_; }

  base::TimeDelta report_period_override_ = base::TimeDelta::FromHours(1);
  base::TimeDelta time_since_start_of_report_period_previous_;
  bool override_report_period_ = true;
};

// TestStreamAnalysis enables copying of StreamAnalysis for testing purposes.
struct TestStreamAnalysis : public StreamAnalysis {
  TestStreamAnalysis() = default;
  ~TestStreamAnalysis() = default;

  TestStreamAnalysis(const TestStreamAnalysis& src) { *this = src; }

  TestStreamAnalysis& operator=(const TestStreamAnalysis& src) {
    mean = src.mean;
    rms = src.rms;
    smr = src.smr;

    std_dev = src.std_dev;
    variance_of_roots = src.variance_of_roots;

    thresholds = src.thresholds;
    percentiles = src.percentiles;

    worst_mean = src.worst_mean;
    worst_rms = src.worst_rms;
    worst_smr = src.worst_smr;

    return *this;
  }
};

// The test fixture used by all tests in this file.
class FrameMetricsTest : public testing::Test {
 public:
  FrameMetricsTest()
      : settings(ui::FrameMetricsSource::UnitTest,
                 ui::FrameMetricsSourceThread::Unknown,
                 ui::FrameMetricsCompileTarget::Unknown) {
    settings.set_is_frame_latency_speed_on(true);
    settings.set_is_frame_latency_acceleration_on(true);
  }

  void SetUp() override {
    // Make sure we don't get an unexpected call to StartNewReportPeriod.
    frame_metrics = std::make_unique<TestFrameMetrics>(settings);
    source_timestamp_origin =
        base::TimeTicks() + base::TimeDelta::FromSeconds(1);
    current_source_timestamp = source_timestamp_origin;
  }

  // A deep reset of all sample history.
  void Reset() {
    frame_metrics->Reset();
    current_source_timestamp = source_timestamp_origin;
  }

  // Simulates frames with a repeating skip pattern, a repeating produce
  // pattern, and a repeating latency pattern. Each pattern runs in parallel
  // and independently of each other.
  // |extra_frames| can help ensure a specific number of metric values are
  // added since the speed and acceleration metrics have 1 and 2 fewer values
  // than frames respectively.
  void TestPattern(std::vector<base::TimeDelta> produced,
                   std::vector<base::TimeDelta> skipped,
                   std::vector<base::TimeDelta> latencies,
                   size_t extra_frames = 0) {
    // Make sure we run each pattern a whole number of times.
    size_t count = 1000 * produced.size() * skipped.size() * latencies.size() +
                   extra_frames;

    for (size_t i = 0; i < count; i++) {
      base::TimeDelta produce = produced[i % produced.size()];
      base::TimeDelta skip = skipped[i % skipped.size()];
      base::TimeDelta latency = latencies[i % latencies.size()];
      base::TimeTicks displayed_timestamp = current_source_timestamp + latency;
      frame_metrics->AddFrameProduced(current_source_timestamp, produce, skip);
      frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                       displayed_timestamp);
      current_source_timestamp += produce + skip;
    }
  }

  // The following methods return the corresponding analysis of all
  // frames added since the last call to Reset().
  TestStreamAnalysis SkipAnalysis() { return Analysis(frame_metrics->skips()); }
  TestStreamAnalysis LatencyAnalysis() {
    return Analysis(frame_metrics->latency());
  }
  TestStreamAnalysis SpeedAnalysis() {
    return Analysis(frame_metrics->speed());
  }
  TestStreamAnalysis AccelerationAnalysis() {
    return Analysis(frame_metrics->acceleration());
  }

  using AnalysisFunc = decltype(&FrameMetricsTest::SkipAnalysis);

  void StartNewReportPeriodAvoidsOverflowTest(base::TimeDelta produced,
                                              base::TimeDelta skipped,
                                              base::TimeDelta latency0,
                                              base::TimeDelta latency1,
                                              double threshold,
                                              AnalysisFunc analysis_method);

 protected:
  static TestStreamAnalysis Analysis(const StreamAnalyzer& analyzer) {
    TestStreamAnalysis analysis;
    analyzer.ComputeSummary(&analysis);
    return analysis;
  }

  FrameMetricsSettings settings;
  std::unique_ptr<TestFrameMetrics> frame_metrics;
  base::TimeTicks source_timestamp_origin;
  base::TimeTicks current_source_timestamp;
};

// Verify we get zeros for skips, speed, and acceleration when the values
// are constant.
TEST_F(FrameMetricsTest, PerfectSmoothnessScores) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(10);
  const base::TimeDelta skip = base::TimeDelta();
  const base::TimeDelta latency = base::TimeDelta::FromMilliseconds(10);
  TestPattern({produced}, {skip}, {latency});
  for (TestStreamAnalysis r :
       {SkipAnalysis(), SpeedAnalysis(), AccelerationAnalysis()}) {
    EXPECT_EQ(0, r.mean);
    EXPECT_EQ(0, r.rms);
    EXPECT_EQ(0, r.smr);
    EXPECT_EQ(0, r.std_dev);
    EXPECT_EQ(0, r.variance_of_roots);
    EXPECT_EQ(0, r.worst_mean.value);
    EXPECT_EQ(0, r.worst_rms.value);
    EXPECT_EQ(0, r.worst_smr.value);
  }
}

// Verify a constant fast latency is correctly reflected in stats.
TEST_F(FrameMetricsTest, PerfectLatencyScores) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(10);
  const base::TimeDelta skip = base::TimeDelta();
  const base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);
  TestPattern({produced}, {skip}, {latency});

  TestStreamAnalysis r = LatencyAnalysis();
  EXPECT_DOUBLE_EQ(latency.InSecondsF(), r.mean);
  EXPECT_NEAR_SQRT_APPROX(latency.InSecondsF(), r.rms);
  EXPECT_NEAR_SQRT_APPROX(r.smr, latency.InSecondsF());
  EXPECT_EQ(0, r.std_dev);
  EXPECT_NEAR_SQRT_APPROX(0, r.variance_of_roots);
  EXPECT_DOUBLE_EQ(latency.InSecondsF(), r.worst_mean.value);
  EXPECT_NEAR_SQRT_APPROX(latency.InSecondsF(), r.worst_rms.value);
  EXPECT_NEAR_SQRT_APPROX(r.worst_smr.value, latency.InSecondsF());
}

// Apply a saw tooth pattern to the frame skips with values that are easy to
// verify for SMR, RMS, etc.
TEST_F(FrameMetricsTest, SawToothShapedSkips) {
  const base::TimeDelta produced = base::TimeDelta::FromSeconds(1);
  const base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);
  const std::vector<base::TimeDelta> skips = {
      base::TimeDelta::FromSeconds(0), base::TimeDelta::FromSeconds(1),
  };
  TestPattern({produced}, skips, {latency});

  // Verify skip stats.
  TestStreamAnalysis r = SkipAnalysis();

  // 1 frame skipped per 3 frames of active time.
  const double expected_skip_mean = (0 + 1.0) / 3;
  EXPECT_EQ(expected_skip_mean, r.mean);
  EXPECT_EQ(expected_skip_mean, r.worst_mean.value);

  // The expected value calculations for everything other than the mean are a
  // bit convoluted since the internal calculations are performed in a different
  // space than the final result. (skip:produce vs. skip:total).
  const double expected_skip_to_produce_mean_square = (0 + 1.0) / 2;
  const double expected_skip_to_produce_rms =
      std::sqrt(expected_skip_to_produce_mean_square);
  const double expected_skip_rms = SkipTransform(expected_skip_to_produce_rms);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_rms, r.rms);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_rms, r.worst_rms.value);

  const double expected_expected_skip_to_produce_mean_root = (0 + 1.0) / 2;
  const double expected_expected_skip_to_produce_smr =
      expected_expected_skip_to_produce_mean_root *
      expected_expected_skip_to_produce_mean_root;
  const double expected_skip_smr =
      SkipTransform(expected_expected_skip_to_produce_smr);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_smr, r.smr);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_smr, r.worst_smr.value);

  const double expected_skip_to_produce_std_dev = (0.5 + 0.5) / 2;
  const double expected_skip_std_dev =
      SkipTransform(expected_skip_to_produce_std_dev);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_std_dev, r.std_dev);

  const double expected_skip_to_produce_std_dev_of_roots = (0.5 + 0.5) / 2;
  const double expected_skip_to_produce_variance_of_roots =
      expected_skip_to_produce_std_dev_of_roots *
      expected_skip_to_produce_std_dev_of_roots;
  const double expected_skip_variance_of_roots =
      SkipTransform(expected_skip_to_produce_variance_of_roots);
  EXPECT_NEAR_SQRT_APPROX(expected_skip_variance_of_roots, r.variance_of_roots);
}

// Apply a saw tooth pattern to the latency with values that are easy to
// verify for SMR, RMS, etc. Furthermore, since the latency speed and
// acceleration are constant, verify that the SMR, RMS, and mean values are
// equal.
TEST_F(FrameMetricsTest, SawToothShapedLatency) {
  const base::TimeDelta produced = base::TimeDelta::FromSeconds(1);
  const base::TimeDelta skipped = base::TimeDelta();
  const std::vector<base::TimeDelta> latencies = {
      base::TimeDelta::FromSeconds(36), base::TimeDelta::FromSeconds(100),
  };
  TestPattern({produced}, {skipped}, latencies);

  // Verify latency.
  TestStreamAnalysis r = LatencyAnalysis();
  const double expected_latency_mean = (100.0 + 36) / 2;
  EXPECT_DOUBLE_EQ(expected_latency_mean, r.mean);
  EXPECT_DOUBLE_EQ(expected_latency_mean, r.worst_mean.value);

  const double expected_latency_mean_square = (100.0 * 100 + 36 * 36) / 2;
  const double expected_latency_rms = std::sqrt(expected_latency_mean_square);
  EXPECT_NEAR_SQRT_APPROX(expected_latency_rms, r.rms);
  EXPECT_NEAR_SQRT_APPROX(expected_latency_rms, r.worst_rms.value);

  const double expected_latency_mean_root = (10.0 + 6) / 2;
  const double expected_latency_smr =
      expected_latency_mean_root * expected_latency_mean_root;
  EXPECT_NEAR_SQRT_APPROX(expected_latency_smr, r.smr);
  EXPECT_NEAR_SQRT_APPROX(expected_latency_smr, r.worst_smr.value);

  const double expected_latency_std_dev = (100.0 - 36) / 2;
  EXPECT_NEAR_SQRT_APPROX(expected_latency_std_dev, r.std_dev);

  const double expected_latency_std_dev_of_roots = (10.0 - 6) / 2;
  const double expected_latency_variance_of_roots =
      expected_latency_std_dev_of_roots * expected_latency_std_dev_of_roots;
  EXPECT_NEAR_SQRT_APPROX(expected_latency_variance_of_roots,
                          r.variance_of_roots);

  // Verify latency speed, where mean, RMS, SMR, etc. should be equal.
  r = SpeedAnalysis();
  const double expected_speed = 64;
  EXPECT_DOUBLE_EQ(expected_speed, r.mean);
  EXPECT_NEAR_SQRT_APPROX(expected_speed, r.rms);
  EXPECT_NEAR_SQRT_APPROX(expected_speed, r.smr);
  EXPECT_DOUBLE_EQ(0, r.std_dev);
  EXPECT_NEAR_SQRT_APPROX(0, r.variance_of_roots);
  EXPECT_DOUBLE_EQ(expected_speed, r.worst_mean.value);
  EXPECT_NEAR_SQRT_APPROX(expected_speed, r.worst_rms.value);
  EXPECT_NEAR_SQRT_APPROX(expected_speed, r.worst_smr.value);

  // Verify latency accelleration, where mean, RMS, SMR, etc. should be equal.
  // The slack is relatively large since the frame durations are so long, which
  // ends up in the divisor twice for acceleration; however, the slack is still
  // within an acceptable range.
  r = AccelerationAnalysis();
  const double expected_acceleration = expected_speed * 2;
  const double slack = 0.1;
  EXPECT_NEAR(expected_acceleration, r.mean, slack);
  EXPECT_NEAR(expected_acceleration, r.rms, slack);
  EXPECT_NEAR(expected_acceleration, r.smr, slack);
  EXPECT_NEAR(0, r.std_dev, slack);
  EXPECT_NEAR(0, r.variance_of_roots, slack);
  EXPECT_NEAR(expected_acceleration, r.worst_mean.value, slack);
  EXPECT_NEAR(expected_acceleration, r.worst_rms.value, slack);
  EXPECT_NEAR(expected_acceleration, r.worst_smr.value, slack);
}

// Makes sure rA and rB are equal.
void VerifySreamAnalysisValueEquality(const TestStreamAnalysis& rA,
                                      const TestStreamAnalysis& rB) {
  EXPECT_EQ(rA.mean, rB.mean);
  EXPECT_EQ(rA.rms, rB.rms);
  EXPECT_EQ(rA.smr, rB.smr);
  EXPECT_EQ(rA.std_dev, rB.std_dev);
  EXPECT_EQ(rA.variance_of_roots, rB.variance_of_roots);
  EXPECT_EQ(rA.worst_mean.value, rB.worst_mean.value);
  EXPECT_EQ(rA.worst_rms.value, rB.worst_rms.value);
  EXPECT_EQ(rA.worst_smr.value, rB.worst_smr.value);
}

// Verify that overflowing skips saturates instead of wraps,
// and that its saturation point is acceptable.
TEST_F(FrameMetricsTest, SkipSaturatesOnOverflow) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta skipA = base::TimeDelta::FromSeconds(66);
  const base::TimeDelta skipB = base::TimeDelta::FromSeconds(80);
  TestPattern({produced}, {skipA}, {latency});
  TestStreamAnalysis rA = SkipAnalysis();
  Reset();
  TestPattern({produced}, {skipB}, {latency});
  TestStreamAnalysis rB = SkipAnalysis();

  // Verify results are larger than a non-saturating value and smaller than
  // than a number just past the expected saturation point.
  EXPECT_LT(kSkipSaturationMin, rB.mean);
  EXPECT_GT(kSkipSaturationMax, rB.mean);
  // Verify the results are the same.
  // If they wrapped around, they would be different.
  VerifySreamAnalysisValueEquality(rA, rB);
}

// Verify that overflowing latency saturates instead of wraps,
// and that its saturation point is acceptable.
TEST_F(FrameMetricsTest, LatencySaturatesOnOverflow) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta skipped = base::TimeDelta();
  const base::TimeDelta latencyA = base::TimeDelta::FromSeconds(4295);
  const base::TimeDelta latencyB = base::TimeDelta::FromSeconds(5000);
  TestPattern({produced}, {skipped}, {latencyA});
  TestStreamAnalysis rA = LatencyAnalysis();
  Reset();
  TestPattern({produced}, {skipped}, {latencyB});
  TestStreamAnalysis rB = LatencyAnalysis();

  // Verify results are larger than a non-saturating value and smaller than
  // than a number just past the expected saturation point.
  EXPECT_LT(kLatencySaturationMin, rB.mean);
  EXPECT_GT(kLatencySaturationMax, rB.mean);
  // Verify the results are the same.
  // If they wrapped around, they would be different.
  VerifySreamAnalysisValueEquality(rA, rB);
}

// Verify that overflowing latency speed saturates instead of wraps,
// and that its saturation point is acceptable.
TEST_F(FrameMetricsTest, LatencySpeedSaturatesOnOverflow) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta skipped = base::TimeDelta();
  const base::TimeDelta latency0 = base::TimeDelta::FromSeconds(0);
  const base::TimeDelta latencyA = base::TimeDelta::FromSeconds(66);
  const base::TimeDelta latencyB = base::TimeDelta::FromSeconds(70);
  TestPattern({produced}, {skipped}, {latency0, latencyA});
  TestStreamAnalysis rA = SpeedAnalysis();
  Reset();
  TestPattern({produced}, {skipped}, {latency0, latencyB});
  TestStreamAnalysis rB = SpeedAnalysis();

  // Verify results are larger than a non-saturating value and smaller than
  // than a number just past the expected saturation point.
  EXPECT_LT(kSpeedSaturationMin, rB.mean);
  EXPECT_GT(kSpeedSaturationMax, rB.mean);
  // Verify the results are the same.
  // If they wrapped around, they would be different.
  VerifySreamAnalysisValueEquality(rA, rB);
}

// Verify that overflowing latency acceleration saturates instead of wraps,
// and that its saturation point is acceptable.
TEST_F(FrameMetricsTest, LatencyAccelerationSaturatesOnOverflow) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta skipped = base::TimeDelta();
  const base::TimeDelta latency0 = base::TimeDelta::FromSeconds(0);
  const base::TimeDelta latencyA = base::TimeDelta::FromSeconds(32);
  const base::TimeDelta latencyB = base::TimeDelta::FromSeconds(34);
  TestPattern({produced}, {skipped}, {latency0, latencyA});
  TestStreamAnalysis rA = AccelerationAnalysis();
  Reset();
  TestPattern({produced}, {skipped}, {latency0, latencyB});
  TestStreamAnalysis rB = AccelerationAnalysis();

  // Verify results are larger than a non-saturating value and smaller than
  // than a number just past the expected saturation point.
  EXPECT_LT(kAccelerationSaturationMin, rB.mean);
  EXPECT_GT(kAccelerationSaturationMax, rB.mean);
  // Verify the results are the same.
  // If they wrapped around, they would be different.
  VerifySreamAnalysisValueEquality(rA, rB);
}

// Helps verify that:
// 1) All thresholds with index less than |i| is 1.
// 2) All thresholds with index greater than |i| is 0.
// 3) The |i|'th threshold equals |straddle_fraction|.
void VerifyThresholds(TestStreamAnalysis analysis,
                      size_t count,
                      size_t i,
                      double straddle_fraction) {
  EXPECT_EQ(count, analysis.thresholds.size());
  EXPECT_EQ(straddle_fraction, analysis.thresholds[i].ge_fraction) << i;
  for (size_t j = 0; j < i; j++)
    EXPECT_EQ(1.0, analysis.thresholds[j].ge_fraction) << i << "," << j;
  for (size_t j = i + 1; j < count; j++)
    EXPECT_EQ(0.0, analysis.thresholds[j].ge_fraction) << i << "," << j;
}

// Iterates through skip patterns that straddle each skip threshold
// and verifies the reported fractions are correct.
TEST_F(FrameMetricsTest, SkipThresholds) {
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency = base::TimeDelta::FromMilliseconds(10);
  std::vector<base::TimeDelta> skips = {
      base::TimeDelta::FromMicroseconds(0),
      base::TimeDelta::FromMicroseconds(250),
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(2),
      base::TimeDelta::FromMilliseconds(4),
      base::TimeDelta::FromMilliseconds(8),
  };

  const size_t kThresholdCount = skips.size() - 2;

  TestPattern({produced}, {skips[0], skips[1]}, {latency});
  TestStreamAnalysis r = SkipAnalysis();
  EXPECT_EQ(kThresholdCount, r.thresholds.size());
  for (size_t j = 0; j < kThresholdCount; j++) {
    EXPECT_EQ(0, r.thresholds[j].ge_fraction);
  }

  for (size_t i = 0; i < kThresholdCount; i++) {
    Reset();
    TestPattern({produced}, {skips[i + 1], skips[i + 2]}, {latency});
    VerifyThresholds(SkipAnalysis(), kThresholdCount, i, 0.5);
  }
}

// Iterates through latency patterns that straddle each latency threshold
// and verifies the reported fractions are correct.
// To straddle a threshold it alternates frames above and below the threshold.
TEST_F(FrameMetricsTest, LatencyThresholds) {
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta skipped = base::TimeDelta();
  std::vector<base::TimeDelta> latencies = {
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(5),
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(20),
      base::TimeDelta::FromMilliseconds(40),
  };

  const size_t kThresholdCount = latencies.size() - 2;

  TestPattern({produced}, {skipped}, {latencies[0], latencies[1]});
  TestStreamAnalysis r = LatencyAnalysis();
  EXPECT_EQ(kThresholdCount, r.thresholds.size());
  for (size_t j = 0; j < kThresholdCount; j++) {
    EXPECT_EQ(0, r.thresholds[j].ge_fraction);
  }

  for (size_t i = 0; i < kThresholdCount; i++) {
    Reset();
    TestPattern({produced}, {skipped}, {latencies[i + 1], latencies[i + 2]});
    VerifyThresholds(LatencyAnalysis(), kThresholdCount, i, 0.5);
  }
}

// Iterates through latency patterns that straddle each latency threshold
// and verifies the reported fractions are correct.
// To straddle a threshold it alternates frames above and below the threshold.
TEST_F(FrameMetricsTest, SpeedThresholds) {
  base::TimeDelta skipped = base::TimeDelta();
  std::vector<base::TimeDelta> latencies = {
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(200),
  };
  std::vector<base::TimeDelta> produced = {
      base::TimeDelta::FromMilliseconds(1000),
      base::TimeDelta::FromMilliseconds(240),
      base::TimeDelta::FromMilliseconds(120),
      base::TimeDelta::FromMilliseconds(60),
      base::TimeDelta::FromMilliseconds(30),
      base::TimeDelta::FromMilliseconds(15),
  };
  const size_t kThresholdCount = produced.size() - 2;

  TestPattern({produced[0], produced[1]}, {skipped}, latencies, 1);
  TestStreamAnalysis r = SpeedAnalysis();
  EXPECT_EQ(kThresholdCount, r.thresholds.size());
  for (size_t j = 0; j < kThresholdCount; j++) {
    EXPECT_EQ(0, r.thresholds[j].ge_fraction);
  }

  for (size_t i = 0; i < kThresholdCount; i++) {
    Reset();
    TestPattern({produced[i + 1], produced[i + 2]}, {skipped}, latencies, 1);
    // The expected "straddle fraction" is 1/3 instead of 1/3 since we
    // varied the "produced" amound of each frame, which affects the weighting.
    VerifyThresholds(SpeedAnalysis(), kThresholdCount, i, 1.0 / 3);
  }
}

// Iterates through acceleration patterns that straddle each acceleration
// threshold and verifies the reported fractions are correct.
// To straddle a threshold it sends a set of frames under the threshold and
// then a second set of frames over the threshold.
TEST_F(FrameMetricsTest, AccelerationThresholds) {
  base::TimeDelta skipped = base::TimeDelta();
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency0 = base::TimeDelta::FromMilliseconds(10);
  std::vector<base::TimeDelta> latencies = {
      latency0 + base::TimeDelta::FromMicroseconds(100),
      latency0 + base::TimeDelta::FromMicroseconds(200),
      latency0 + base::TimeDelta::FromMicroseconds(500),
      latency0 + base::TimeDelta::FromMicroseconds(1000),
      latency0 + base::TimeDelta::FromMicroseconds(2000),
      latency0 + base::TimeDelta::FromMicroseconds(4000),
  };
  const size_t kThresholdCount = latencies.size() - 2;

  TestPattern({produced}, {skipped}, {latency0, latencies[0]}, 2);
  TestPattern({produced}, {skipped}, {latency0, latencies[1]}, 2);
  TestStreamAnalysis r = AccelerationAnalysis();
  EXPECT_EQ(kThresholdCount, r.thresholds.size());
  for (size_t j = 0; j < kThresholdCount; j++) {
    EXPECT_EQ(0, r.thresholds[j].ge_fraction);
  }

  for (size_t i = 0; i < kThresholdCount; i++) {
    Reset();
    TestPattern({produced}, {skipped}, {latency0, latencies[i + 1]}, 2);
    TestPattern({produced}, {skipped}, {latency0, latencies[i + 2]}, 2);
    VerifyThresholds(AccelerationAnalysis(), kThresholdCount, i, 0.5);
  }
}

// The percentile calcuation is an estimate, so make sure it is within an
// acceptable threshold. The offset is needed in case the expected value is 0.
void VerifyPercentiles(TestStreamAnalysis r, double expected, int source_line) {
  double kPercentileSlackScale = .5;
  double kPercentileSlackOffset = .02;
  for (size_t i = 0; i < PercentileResults::kCount; i++) {
    EXPECT_LT((1 - kPercentileSlackScale) * expected - kPercentileSlackOffset,
              r.percentiles.values[i])
        << i << ", " << source_line;
    EXPECT_GT(
        (1 + 2 * kPercentileSlackScale) * expected + kPercentileSlackOffset,
        r.percentiles.values[i])
        << i << ", " << source_line;
  }
}

// This is a basic test to verify percentiles for skips are hooked up correctly.
// The histogram unit tests already test bucketing and precision in depth,
// so we don't worry about that here.
TEST_F(FrameMetricsTest, PercentilesSkipBasic) {
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);

  // Everything fast.
  base::TimeDelta skipped = base::TimeDelta();
  base::TimeTicks displayed_timestamp = current_source_timestamp + latency;
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  VerifyPercentiles(SkipAnalysis(), 0, __LINE__);
  VerifyPercentiles(LatencyAnalysis(), latency.InSecondsF(), __LINE__);
  VerifyPercentiles(SpeedAnalysis(), 0, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), 0, __LINE__);

  // Bad skip.
  Reset();
  skipped = base::TimeDelta::FromSeconds(5);
  displayed_timestamp = current_source_timestamp + latency;
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  double expected_skip_fraction =
      skipped.InSecondsF() / (skipped.InSecondsF() + produced.InSecondsF());
  VerifyPercentiles(SkipAnalysis(), expected_skip_fraction, __LINE__);
  VerifyPercentiles(LatencyAnalysis(), latency.InSecondsF(), __LINE__);
  VerifyPercentiles(SpeedAnalysis(), 0, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), 0, __LINE__);
}

// This is a basic test to verify percentiles for latency, speed, and
// acceleration are hooked up correctly. It uses the property that latency,
// speed, and acceleration results are delayed until there are at least
// 1, 2, and 3 frames respectively.
// The histogram unit tests already test bucketing and precision in depth,
// so we don't worry about that here.
TEST_F(FrameMetricsTest, PercentilesLatencyBasic) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta skipped = base::TimeDelta();
  const base::TimeDelta latency0 = base::TimeDelta::FromMilliseconds(1);
  const base::TimeDelta latency_delta = base::TimeDelta::FromSeconds(5);
  const std::vector<base::TimeDelta> latencies = {
      latency0 + latency_delta, latency0, latency0 + latency_delta,
  };

  // Everything fast.
  base::TimeTicks displayed_timestamp = current_source_timestamp + latency0;
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  VerifyPercentiles(SkipAnalysis(), 0, __LINE__);
  VerifyPercentiles(LatencyAnalysis(), latency0.InSecondsF(), __LINE__);
  VerifyPercentiles(SpeedAnalysis(), 0, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), 0, __LINE__);

  // Bad latency.
  Reset();
  displayed_timestamp = current_source_timestamp + latencies[0];
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  double expected_latency = (latencies[0]).InSecondsF();
  VerifyPercentiles(SkipAnalysis(), 0, __LINE__);
  VerifyPercentiles(LatencyAnalysis(), expected_latency, __LINE__);
  VerifyPercentiles(SpeedAnalysis(), 0, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), 0, __LINE__);

  // Bad latency speed.
  displayed_timestamp = current_source_timestamp + latencies[1];
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  double expected_speed = latency_delta.InSecondsF() / produced.InSecondsF();
  VerifyPercentiles(SkipAnalysis(), 0, __LINE__);
  VerifyPercentiles(SpeedAnalysis(), expected_speed, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), 0, __LINE__);

  // Bad latency acceleration.
  double expected_acceleration = 2 * expected_speed / produced.InSecondsF();
  displayed_timestamp = current_source_timestamp + latencies[2];
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  VerifyPercentiles(SkipAnalysis(), 0, __LINE__);
  VerifyPercentiles(AccelerationAnalysis(), expected_acceleration, __LINE__);
}

// Applies a bunch of good frames followed by one bad frame.
// Then verifies all windows jump from the beginning (just before the bad frame)
// to the end (just after the bad frame).
TEST_F(FrameMetricsTest, WorstWindowsRangesUpdateCorrectly) {
  const base::TimeDelta produced = base::TimeDelta::FromMilliseconds(10);
  const base::TimeDelta skipped = base::TimeDelta();
  const base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);
  TestPattern({produced}, {skipped}, {latency});

  base::TimeTicks expected_begin, expected_end;

  // Verify windows for skips and latency start at the very beginning.
  expected_begin = source_timestamp_origin;
  expected_end =
      source_timestamp_origin + produced * (settings.max_window_size - 1);
  for (TestStreamAnalysis r : {SkipAnalysis(), LatencyAnalysis()}) {
    EXPECT_EQ(expected_begin, r.worst_mean.window_begin);
    EXPECT_EQ(expected_end, r.worst_mean.window_end);
    EXPECT_EQ(expected_begin, r.worst_rms.window_begin);
    EXPECT_EQ(expected_end, r.worst_rms.window_end);
    EXPECT_EQ(expected_begin, r.worst_smr.window_begin);
    EXPECT_EQ(expected_end, r.worst_smr.window_end);
  }

  // Verify windows for speed and acceleration start near the beginning.
  // We expect their windows to be delayed by 1 and 2 frames respectively
  // since their first results need to compare multiple frames.
  for (TestStreamAnalysis r : {SpeedAnalysis(), AccelerationAnalysis()}) {
    expected_begin += produced;
    expected_end += produced;
    EXPECT_EQ(expected_begin, r.worst_mean.window_begin);
    EXPECT_EQ(expected_end, r.worst_mean.window_end);
    EXPECT_EQ(expected_begin, r.worst_rms.window_begin);
    EXPECT_EQ(expected_end, r.worst_rms.window_end);
    EXPECT_EQ(expected_begin, r.worst_smr.window_begin);
    EXPECT_EQ(expected_end, r.worst_smr.window_end);
  }

  // Add a bad frame so the windows are updated for all the dimensions.
  base::TimeTicks displayed_timestamp =
      current_source_timestamp + (2 * latency);
  const base::TimeDelta skipped2 = base::TimeDelta::FromMilliseconds(1);
  frame_metrics->AddFrameProduced(current_source_timestamp, produced - skipped2,
                                  skipped2);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);

  // Verify all dimensions windows have updated.
  expected_begin =
      current_source_timestamp - produced * (settings.max_window_size - 1);
  expected_end = current_source_timestamp;
  for (TestStreamAnalysis r : {SkipAnalysis(), LatencyAnalysis(),
                               SpeedAnalysis(), AccelerationAnalysis()}) {
    EXPECT_EQ(expected_begin, r.worst_mean.window_begin);
    EXPECT_EQ(expected_end, r.worst_mean.window_end);
    EXPECT_EQ(expected_begin, r.worst_rms.window_begin);
    EXPECT_EQ(expected_end, r.worst_rms.window_end);
    EXPECT_EQ(expected_begin, r.worst_smr.window_begin);
    EXPECT_EQ(expected_end, r.worst_smr.window_end);
  }
}

// Accumulating samples for too long can result in overflow of the accumulators.
// This can happen if the system sleeps / hibernates for a long time.
// Make sure values are reported often enough to avoid overflow.
void FrameMetricsTest::StartNewReportPeriodAvoidsOverflowTest(
    base::TimeDelta produced,
    base::TimeDelta skipped,
    base::TimeDelta latency0,
    base::TimeDelta latency1,
    double threshold,
    AnalysisFunc analysis_method) {
  // We need one frame here so that we have 3 frames by the first time we call
  // AccelerationAnalysis. Before 3 frames, acceleration is not defined.
  base::TimeTicks displayed_timestamp = current_source_timestamp + latency1;
  frame_metrics->AddFrameProduced(current_source_timestamp, produced, skipped);
  frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                   displayed_timestamp);
  current_source_timestamp += produced + skipped;

  do {
    displayed_timestamp = current_source_timestamp + latency0;
    frame_metrics->AddFrameProduced(current_source_timestamp, produced,
                                    skipped);
    frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                     displayed_timestamp);
    current_source_timestamp += produced + skipped;

    displayed_timestamp = current_source_timestamp + latency1;
    frame_metrics->AddFrameProduced(current_source_timestamp, produced,
                                    skipped);
    frame_metrics->AddFrameDisplayed(current_source_timestamp,
                                     displayed_timestamp);
    current_source_timestamp += produced + skipped;

    TestStreamAnalysis r = (this->*analysis_method)();
    // If there's overflow, the result will be much less than the threshold.
    ASSERT_LT(threshold, r.mean);
    ASSERT_LT(threshold, r.rms);
    ASSERT_LT(threshold, r.smr);
  } while (!frame_metrics->AtStartOfNewReportPeriod());
}

// Make sure values are reported often enough to avoid skip overflow.
TEST_F(FrameMetricsTest, StartNewReportPeriodAvoidsOverflowForSkips) {
  base::TimeDelta produced = base::TimeDelta::FromMicroseconds(1);
  base::TimeDelta latency = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta skipped = base::TimeDelta::FromSeconds(2);

  frame_metrics->UseDefaultReportPeriodScaled(7);
  StartNewReportPeriodAvoidsOverflowTest(produced, skipped, latency, latency,
                                         kSkipSaturationMin,
                                         &FrameMetricsTest::SkipAnalysis);
}

// Make sure values are reported often enough to avoid latency overflow.
TEST_F(FrameMetricsTest, StartNewReportPeriodAvoidsOverflowForLatency) {
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency = base::TimeDelta::FromSeconds(5000);
  base::TimeDelta skipped = base::TimeDelta::FromSeconds(0);

  frame_metrics->UseDefaultReportPeriodScaled(2);
  StartNewReportPeriodAvoidsOverflowTest(produced, skipped, latency, latency,
                                         kLatencySaturationMin,
                                         &FrameMetricsTest::LatencyAnalysis);
}

// Make sure values are reported often enough to avoid speed overflow.
TEST_F(FrameMetricsTest, StartNewReportPeriodAvoidsOverflowForSpeed) {
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency0 = base::TimeDelta::FromSeconds(0);
  base::TimeDelta latency1 = base::TimeDelta::FromSeconds(70);
  base::TimeDelta skipped = base::TimeDelta::FromSeconds(0);

  frame_metrics->UseDefaultReportPeriodScaled(2);
  StartNewReportPeriodAvoidsOverflowTest(produced, skipped, latency0, latency1,
                                         kSpeedSaturationMin,
                                         &FrameMetricsTest::SpeedAnalysis);
}

// Make sure values are reported often enough to avoid acceleration overflow.
TEST_F(FrameMetricsTest, StartNewReportPeriodAvoidsOverflowForAcceleration) {
  frame_metrics->UseDefaultReportPeriodScaled(2);
  base::TimeDelta produced = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta latency0 = base::TimeDelta::FromSeconds(0);
  base::TimeDelta latency1 = base::TimeDelta::FromSeconds(33);
  base::TimeDelta skipped = base::TimeDelta::FromSeconds(0);

  frame_metrics->UseDefaultReportPeriodScaled(2);
  StartNewReportPeriodAvoidsOverflowTest(
      produced, skipped, latency0, latency1, kAccelerationSaturationMin,
      &FrameMetricsTest::AccelerationAnalysis);
}

// Test the accuracy of the Newton's approximate square root calculation.
// Since suqare_rooot is always used on small numbers in cc, this test only test
// accuracy of small |x| value. A random number |x| between (0 - 100) is
// generated, Test if the difference of square roots obtained from
// FastApproximateSqrt and std::sqrt is less than |error_rage| (0.0001);
TEST_F(FrameMetricsTest, SquareRootApproximation) {
  const double slack = 0.001;
  for (int i = 0; i < 3; i++) {
    int x = base::RandInt(0, 100);
    double sol1 = std::sqrt(x);
    double sol2 = FrameMetrics::FastApproximateSqrt(x);
    EXPECT_NEAR(sol1, sol2, slack)
        << "failed to give a good approximate square root of " << x;
  }

  for (int i = 0; i < 3; i++) {
    double x = double{base::RandUint64()} / base::RandomBitGenerator::max();
    double sol1 = std::sqrt(x);
    double sol2 = FrameMetrics::FastApproximateSqrt(x);
    EXPECT_NEAR(sol1, sol2, slack)
        << "failed to give a good approximate square root of " << x;
  }
}

}  // namespace
}  // namespace frame_metrics
}  // namespace ui
