// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/histograms.h"

#include <algorithm>

#include "base/metrics/bucket_ranges.h"
#include "base/metrics/sample_vector.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/latency/fixed_point.h"
#include "ui/latency/frame_metrics_test_common.h"

namespace ui {
namespace frame_metrics {

constexpr base::TimeDelta kTimeLimit = base::TimeDelta::FromSeconds(2);

// A version of RatioHistogram based on the default implementations
// of base::BucketRanges and base::SampleVector.
class RatioHistogramBaseline : public Histogram {
 public:
  RatioHistogramBaseline()
      : ratio_boundaries_(),
        bucket_ranges_(ratio_boundaries_.size()),
        sample_vector_(&bucket_ranges_) {
    size_t i = 0;
    for (const auto& b : ratio_boundaries_.boundaries) {
      bucket_ranges_.set_range(i++, std::min<uint64_t>(b, INT_MAX));
    }
  }

  ~RatioHistogramBaseline() override = default;

  void AddSample(uint32_t microseconds, uint32_t weight) override {
    sample_vector_.Accumulate(microseconds, weight);
  }

  PercentileResults ComputePercentiles() const override {
    return PercentileResults();
  }
  void Reset() override {}

 private:
  TestRatioBoundaries ratio_boundaries_;
  base::BucketRanges bucket_ranges_;
  base::SampleVector sample_vector_;

  DISALLOW_COPY_AND_ASSIGN(RatioHistogramBaseline);
};

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter("FrameMetricsHistograms", story_name);
  reporter.RegisterImportantMetric(".speedup", "score");
  return reporter;
}

TEST(FrameMetricsHistogramsPerfTest, RatioEntireRange) {
  const int kStride = 0x1000;

  RatioHistogramBaseline vh_base;
  RatioHistogram vh_impl;

  base::TimeDelta impl_time;
  base::TimeDelta base_time;

  base::TimeTicks finish_time = base::TimeTicks::Now() + kTimeLimit;
  while (base::TimeTicks::Now() < finish_time) {
    // Impl then Base
    for (int i = 0; i < INT_MAX - kStride; i += kStride) {
      int value = (i * 37) & 0x3FFFFFFF;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      impl_time += t1 - t0 - (t3 - t2);
      base_time += t2 - t1 - (t3 - t2);
    }

    // Base then Impl
    for (int i = 0; i < INT_MAX - kStride; i += kStride) {
      int value = (i * 37) & 0x3FFFFFFF;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      base_time += t1 - t0 - (t3 - t2);
      impl_time += t2 - t1 - (t3 - t2);
    }
  }

  double speedup = base_time.InSecondsF() / impl_time.InSecondsF();
  perf_test::PerfResultReporter reporter = SetUpReporter("RatioEntireRange");
  reporter.AddResult(".speedup", speedup);
}

TEST(FrameMetricsHistogramsPerfTest, RatioCommonRange) {
  const int kStride = 0x100;

  RatioHistogramBaseline vh_base;
  RatioHistogram vh_impl;

  base::TimeDelta impl_time;
  base::TimeDelta base_time;

  base::TimeTicks finish_time = base::TimeTicks::Now() + kTimeLimit;
  while (base::TimeTicks::Now() < finish_time) {
    // Impl then Base
    for (int i = 0; i < 4 * kFixedPointMultiplier; i += kStride) {
      int value = i;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      impl_time += t1 - t0 - (t3 - t2);
      base_time += t2 - t1 - (t3 - t2);
    }

    // Base then Impl
    for (int i = 0; i < 4 * kFixedPointMultiplier; i += kStride) {
      int value = i;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      base_time += t1 - t0 - (t3 - t2);
      impl_time += t2 - t1 - (t3 - t2);
    }
  }

  double speedup = base_time.InSecondsF() / impl_time.InSecondsF();
  perf_test::PerfResultReporter reporter = SetUpReporter("RatioCommonRange");
  reporter.AddResult(".speedup", speedup);
}

// A version of VSyncHistogram based on the default implementations
// of base::BucketRanges and base::SampleVector.
class VSyncHistogramBaseline : public Histogram {
 public:
  VSyncHistogramBaseline()
      : bucket_ranges_(kTestVSyncBoundries.size() + 1),
        sample_vector_(&bucket_ranges_) {
    size_t i = 0;
    for (const auto& b : kTestVSyncBoundries) {
      bucket_ranges_.set_range(i++, b);
    }
    // BucketRanges needs the last element set to INT_MAX.
    bucket_ranges_.set_range(i++, INT_MAX);
  }

  ~VSyncHistogramBaseline() override = default;

  void AddSample(uint32_t microseconds, uint32_t weight) override {
    sample_vector_.Accumulate(microseconds, weight);
  }

  PercentileResults ComputePercentiles() const override {
    return PercentileResults();
  }
  void Reset() override {}

 private:
  base::BucketRanges bucket_ranges_;
  base::SampleVector sample_vector_;

  DISALLOW_COPY_AND_ASSIGN(VSyncHistogramBaseline);
};

TEST(FrameMetricsHistogramsPerfTest, VSyncEntireRange) {
  const int kStride = 0x1000;

  VSyncHistogramBaseline vh_base;
  VSyncHistogram vh_impl;

  base::TimeDelta impl_time;
  base::TimeDelta base_time;

  base::TimeTicks finish_time = base::TimeTicks::Now() + kTimeLimit;
  while (base::TimeTicks::Now() < finish_time) {
    // Impl then Base
    for (int i = 0; i < INT_MAX - kStride; i += kStride) {
      int value = (i * 37) % 64000000;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      impl_time += t1 - t0 - (t3 - t2);
      base_time += t2 - t1 - (t3 - t2);
    }

    // Base then Impl
    for (int i = 0; i < INT_MAX - kStride; i += kStride) {
      int value = (i * 37) % 64000000;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      base_time += t1 - t0 - (t3 - t2);
      impl_time += t2 - t1 - (t3 - t2);
    }
  }

  double speedup = base_time.InSecondsF() / impl_time.InSecondsF();
  perf_test::PerfResultReporter reporter = SetUpReporter("VSyncEntireRange");
  reporter.AddResult(".speedup", speedup);
}

TEST(FrameMetricsHistogramsPerfTest, VSyncCommonRange) {
  const int kStride = 0x100;

  VSyncHistogramBaseline vh_base;
  VSyncHistogram vh_impl;

  base::TimeDelta impl_time;
  base::TimeDelta base_time;

  base::TimeTicks finish_time = base::TimeTicks::Now() + kTimeLimit;
  while (base::TimeTicks::Now() < finish_time) {
    // Impl then Base
    for (int i = 0; i < 100000; i += kStride) {
      int value = i;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      impl_time += t1 - t0 - (t3 - t2);
      base_time += t2 - t1 - (t3 - t2);
    }

    // Base then Impl
    for (int i = 0; i < 100000; i += kStride) {
      int value = i;
      base::TimeTicks t0 = base::TimeTicks::Now();
      vh_base.AddSample(value, 1);
      base::TimeTicks t1 = base::TimeTicks::Now();
      vh_impl.AddSample(value, 1);
      base::TimeTicks t2 = base::TimeTicks::Now();
      base::TimeTicks t3 = base::TimeTicks::Now();
      base_time += t1 - t0 - (t3 - t2);
      impl_time += t2 - t1 - (t3 - t2);
    }
  }

  double speedup = base_time.InSecondsF() / impl_time.InSecondsF();
  perf_test::PerfResultReporter reporter = SetUpReporter("VSyncCommonRange");
  reporter.AddResult(".speedup", speedup);
}

}  // namespace frame_metrics
}  // namespace ui
