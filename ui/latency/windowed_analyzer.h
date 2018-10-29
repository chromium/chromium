// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_WINDOWED_ANALYZER_H_
#define UI_LATENCY_WINDOWED_ANALYZER_H_

#include <cstdint>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "ui/latency/fixed_point.h"

namespace ui {

// FrameRegionResult encodes window of time where a metric was worst.
// The |sample_count| is the number of samples/frames within the time window
// used to calculate the result. It is reported in case the client wants to
// assess the confidence of the result.
struct FrameRegionResult {
  double value = 0;
  size_t sample_count = 0;
  base::TimeTicks window_begin;
  base::TimeTicks window_end;

  void AsValueInto(base::trace_event::TracedValue* state) const;
};

namespace frame_metrics {

// Client delegates that are specific to each WindowedAnalyzer.
class WindowedAnalyzerClient {
 public:
  // The WorstMean,RMS,SMR methods will give TransformResult() a chance to
  // modify the results via this delegate.
  // This can be used to undo any tranformations applied to values added
  // to AddSample, such as conversions to fixed point.
  virtual double TransformResult(double result) const = 0;

  // TODO(brianderson): Replace WindowedAnalyzer::window_queue_ with a client
  // interface here. All latency derived metrics should be able to share a
  // common history of values. http://crbug.com/822054
};

// Client delegates that can be shared by multiple WindowedAnalyzers.
// Tracks the current window of time that can be stored as the worst
// window of time if a metric detects it as such.
struct SharedWindowedAnalyzerClient {
  SharedWindowedAnalyzerClient() : max_window_size(0) {}

  explicit SharedWindowedAnalyzerClient(size_t max_window_size)
      : max_window_size(max_window_size) {}

  SharedWindowedAnalyzerClient(size_t max_window_size,
                               base::TimeTicks window_begin,
                               base::TimeTicks window_end)
      : max_window_size(max_window_size),
        window_begin(window_begin),
        window_end(window_end) {}

  // Maximum window size in number of samples.
  size_t max_window_size;

  // Current window of time for the samples being added.
  base::TimeTicks window_begin;
  base::TimeTicks window_end;
};

// Detects the worst windows of time for a metric.
// Tracks the current values of the current window of time for the
// mean, RMS, and SMR of a single metric. It maintains a history
// of the recent samples and, for each new sample, updates it's accumulators
// using the oldest and newest samples, without looking at any of the other
// samples in between.
class WindowedAnalyzer {
 public:
  WindowedAnalyzer(const WindowedAnalyzerClient* client,
                   const SharedWindowedAnalyzerClient* shared_client);
  virtual ~WindowedAnalyzer();

  // ResetWosrtValues only resets the memory of worst values encountered,
  // without resetting recent sample history.
  void ResetWorstValues();

  // ResetHistory only resets recent sample history without resetting memory
  // of the worst values ecnountered.
  void ResetHistory();

  // Callers of AddSample will already have calculated weighted values to
  // track cumulative results, so just let them pass in the values here
  // rather than re-calculating them.
  void AddSample(uint32_t value,
                 uint32_t weight,
                 uint64_t weighted_value,
                 uint64_t weighted_root,
                 const Accumulator96b& weighted_square);

  // Returns the worst regions encountered so far.
  FrameRegionResult ComputeWorstMean() const;
  FrameRegionResult ComputeWorstRMS() const;
  FrameRegionResult ComputeWorstSMR() const;

 protected:
  struct QueueEntry {
    uint32_t value = 0;
    uint32_t weight = 0;
  };

  // Updates the result with the current value, if it is worse than the
  // value in |result| or if |initialize| is true.
  template <typename AccumulatorT>
  void UpdateWorst(const AccumulatorT& accumulator,
                   FrameRegionResult* result,
                   bool initialize) const {
    double current_mean = AsDouble(accumulator) / total_weight_;
    if (initialize || current_mean > result->value) {
      result->value = current_mean;
      result->sample_count = window_queue_.size();
      result->window_begin = shared_client_->window_begin;
      result->window_end = shared_client_->window_end;
    }
  }

  const WindowedAnalyzerClient* const client_;
  const SharedWindowedAnalyzerClient* const shared_client_;

  // We need to maintain a history of values so we can
  // remove old samples from the accumulators.
  base::circular_deque<QueueEntry> window_queue_;

  uint64_t total_weight_ = 0;
  uint64_t accumulator_ = 0;
  uint64_t root_accumulator_ = 0;
  Accumulator96b square_accumulator_;

  // Internal results that track the worst region so far.
  // The time region is stored correctly, however the results are intermediate
  // and must be adjusted by result_transform_ and fixed_point_multipler before
  // exposure to the client. Furthermore, RMS needs to square root the result
  // and SMR needs to square the result.
  struct InternalResults {
    FrameRegionResult mean;
    FrameRegionResult root;
    FrameRegionResult square;
  };
  // Optional since they aren't valid until we've seen enough samples.
  // This delay prevents the first couple samples from dominating the result.
  base::Optional<InternalResults> results_;

  DISALLOW_COPY_AND_ASSIGN(WindowedAnalyzer);
};

}  // namespace frame_metrics
}  // namespace ui

#endif  // UI_LATENCY_WINDOWED_ANALYZER_H_
