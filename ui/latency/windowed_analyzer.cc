// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/windowed_analyzer.h"

#include "ui/latency/frame_metrics.h"

namespace ui {

void FrameRegionResult::AsValueInto(
    base::trace_event::TracedValue* state) const {
  // We don't report sample_count, since that is reported at a higher level.
  state->SetDouble("value", value);
  state->SetDouble("start", window_begin.since_origin().InMillisecondsF());
  state->SetDouble("duration", (window_end - window_begin).InMillisecondsF());
}

namespace frame_metrics {

WindowedAnalyzer::WindowedAnalyzer(
    const WindowedAnalyzerClient* client,
    const SharedWindowedAnalyzerClient* shared_client)
    : client_(client), shared_client_(shared_client) {
  window_queue_.reserve(shared_client->max_window_size);
}

WindowedAnalyzer::~WindowedAnalyzer() = default;

void WindowedAnalyzer::ResetWorstValues() {
  results_.reset();
}

void WindowedAnalyzer::ResetHistory() {
  total_weight_ = 0;
  accumulator_ = 0;
  root_accumulator_ = 0;
  square_accumulator_ = Accumulator96b();
  window_queue_.resize(0);
}

void WindowedAnalyzer::AddSample(uint32_t value,
                                 uint32_t weight,
                                 uint64_t weighted_value,
                                 uint64_t weighted_root,
                                 const Accumulator96b& weighted_square) {
  DCHECK_GT(weight, 0u);
  DCHECK_EQ(weighted_value, static_cast<uint64_t>(weight) * value);

  // Remove old values from the accumulators.
  if (window_queue_.size() >= shared_client_->max_window_size) {
    const uint32_t old_value = window_queue_.front().value;
    const uint32_t old_weight = window_queue_.front().weight;
    window_queue_.pop_front();

    // Re-calculate some of the old values here. Although squared and root are
    // passed in, we've only stored the original value to reduce memory usage.
    total_weight_ -= old_weight;
    accumulator_ -= static_cast<uint64_t>(old_weight) * old_value;
    // Casting the whole rhs is important to ensure rounding happens at a place
    // equivalent to when it was added.
    root_accumulator_ -=
        static_cast<uint64_t>(old_weight * FrameMetrics::FastApproximateSqrt(
                                               static_cast<uint64_t>(old_value)
                                               << kFixedPointRootShift));
    square_accumulator_.Subtract(Accumulator96b(old_value, old_weight));
  }

  // Verify overflow isn't an issue.
  // square_accumulator_ has DCHECKs internally, so we don't worry about
  // checking that here.
  DCHECK_LT(weighted_value,
            std::numeric_limits<decltype(accumulator_)>::max() - accumulator_);
  DCHECK_LT(weighted_root,
            std::numeric_limits<decltype(root_accumulator_)>::max() -
                root_accumulator_);
  DCHECK_LT(weight, std::numeric_limits<decltype(total_weight_)>::max() -
                        total_weight_);

  window_queue_.push_back({value, weight});
  total_weight_ += weight;
  accumulator_ += weighted_value;
  root_accumulator_ += weighted_root;
  square_accumulator_.Add(weighted_square);
  if (window_queue_.size() >= shared_client_->max_window_size) {
    bool initialize_results = !results_;
    if (initialize_results)
      results_.emplace();
    UpdateWorst(accumulator_, &results_->mean, initialize_results);
    UpdateWorst(root_accumulator_, &results_->root, initialize_results);
    UpdateWorst(square_accumulator_, &results_->square, initialize_results);
  }
}

FrameRegionResult WindowedAnalyzer::ComputeWorstMean() const {
  FrameRegionResult result;
  if (results_) {
    result = results_->mean;
  } else {
    UpdateWorst(accumulator_, &result, true);
  }
  result.value = client_->TransformResult(result.value);
  return result;
}

FrameRegionResult WindowedAnalyzer::ComputeWorstRMS() const {
  FrameRegionResult result;
  if (results_) {
    result = results_->square;
  } else {
    UpdateWorst(square_accumulator_, &result, true);
  }
  result.value =
      client_->TransformResult(FrameMetrics::FastApproximateSqrt(result.value));
  return result;
}

FrameRegionResult WindowedAnalyzer::ComputeWorstSMR() const {
  FrameRegionResult result;
  if (results_) {
    result = results_->root;
  } else {
    UpdateWorst(root_accumulator_, &result, true);
  }
  result.value = client_->TransformResult((result.value * result.value) /
                                          kFixedPointRootMultiplier);
  return result;
}

}  // namespace frame_metrics
}  // namespace ui
