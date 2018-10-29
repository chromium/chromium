// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_STREAM_ANALYZER_H_
#define UI_LATENCY_STREAM_ANALYZER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/trace_event/traced_value.h"
#include "ui/latency/fixed_point.h"
#include "ui/latency/histograms.h"
#include "ui/latency/windowed_analyzer.h"

namespace ui {

// Used to communicate fraction of time the value of a metric was greater than
// or equal to the threshold.
struct ThresholdResult {
  double threshold = 0.0;
  double ge_fraction = 0.0;
};

struct StreamAnalysis {
  StreamAnalysis();
  ~StreamAnalysis();

  double mean;
  double rms;
  double smr;

  double std_dev;
  double variance_of_roots;

  std::vector<ThresholdResult> thresholds;
  PercentileResults percentiles;

  size_t worst_sample_count = 0;
  FrameRegionResult worst_mean;
  FrameRegionResult worst_rms;
  FrameRegionResult worst_smr;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  DISALLOW_COPY_AND_ASSIGN(StreamAnalysis);
};

namespace frame_metrics {

// The StreamAnalyzerClient interface is currently the same as
// WindowedAnalyzerClient and can rely on the same implementation.
using StreamAnalyzerClient = WindowedAnalyzerClient;

// Tracks the overall mean, RMS, and SMR for a metric and also owns
// the Histogram and WindowedAnalyzer.
class StreamAnalyzer {
 public:
  StreamAnalyzer(const StreamAnalyzerClient* client,
                 const SharedWindowedAnalyzerClient* shared_client,
                 std::vector<uint32_t> thresholds,
                 std::unique_ptr<Histogram> histogram);
  ~StreamAnalyzer();

  // Resets all statistics and history.
  void Reset();

  // Resets the statistics without throwing away recent sample history in the
  // WindowedAnalyzer.
  void StartNewReportPeriod();

  // To play well with the histogram range, |value| should be within the
  // range [0,64000000]. If the units are milliseconds, that's 64 seconds.
  // Otherwise, the histogram will clip the result.
  // |weight| may be the duration the frame was active in microseconds
  //          or it may be 1 in case every frame is to be weighed equally.
  void AddSample(const uint32_t value, const uint32_t weight);

  // The mean, root-mean-squared, and squared-mean-root of all samples
  // received since the last call to StartNewReportPeriod().
  // The units are the same as the values added in AddSample().
  double ComputeMean() const;
  double ComputeRMS() const;
  double ComputeSMR() const;

  // StdDev calculates the standard deviation of all values in the stream.
  // The units are the same as the values added in AddSample().
  // The work to track this is the same as RMS, so we effectively get this for
  // free. Given two of the Mean, RMS, and StdDev, we can calculate the third.
  double ComputeStdDev() const;

  // VarianceOfRoots calculates the variance of all square roots of values.
  // The units end up being the same as the values added in AddSample().
  // The work to track this is the same as SMR.
  // Given two of the Mean, SMR, and VarianceOfRoots, we can calculate the
  // third. Note: We don't track something like RootStdDevOfSquares since it
  // would be difficult to track values raised to the fourth power.
  // TODO(brianderon): Remove VarianceOfRoots if it's not useful.
  double ComputeVarianceOfRoots() const;

  // Thresholds returns a percentile for threshold values given to the
  // constructor. This is useful for tracking improvements in really good
  // sources, but it's dynamic range is limited, which prevents it from
  // detecting improvements in sources where most of the frames are "bad".
  std::vector<ThresholdResult> ComputeThresholds() const;

  // CalculatePercentiles returns a value for certain percentiles.
  // It is only an estimate, since the values are calculated from a histogram
  // rather than from the entire history of actual values.
  // This is useful for tracking improvements even in really bad sources
  // since it's dynamic range includes all possible values.
  PercentileResults ComputePercentiles() const;

  // Expose the WindowedAnalyzer as const to make it's accessors
  // available directly.
  const WindowedAnalyzer& window() const { return windowed_analyzer_; }

  void ComputeSummary(StreamAnalysis* results) const;

 protected:
  double VarianceHelper(double accum, double square_accum) const;

  struct ThresholdState {
    explicit ThresholdState(uint32_t value) : threshold(value) {}
    void ResetAccumulators();

    uint32_t threshold;
    uint32_t ge_weight = 0;
    uint32_t lt_weight = 0;
  };

  const StreamAnalyzerClient* const client_;

  std::vector<ThresholdState> thresholds_;
  std::unique_ptr<Histogram> histogram_;
  WindowedAnalyzer windowed_analyzer_;

  uint64_t total_weight_ = 0;
  uint64_t accumulator_ = 0;
  uint64_t root_accumulator_ = 0;
  Accumulator96b square_accumulator_;

  DISALLOW_COPY_AND_ASSIGN(StreamAnalyzer);
};

}  // namespace frame_metrics
}  // namespace ui

#endif  // UI_LATENCY_STREAM_ANALYZER_H_
