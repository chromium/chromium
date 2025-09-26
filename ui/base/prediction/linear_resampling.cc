// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/linear_resampling.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_features.h"

namespace ui {

namespace {
// Minimum time difference between last two consecutive events before attempting
// to resample.
constexpr auto kResampleMinDelta = base::Milliseconds(2);
// Maximum time to predict forward from the last event, to avoid predicting too
// far into the future. This time is further bounded by 50% of the last time
// delta.
constexpr auto kResampleMaxPrediction = base::Milliseconds(8);

// Get position at |sample_time| by linear interpolate/extrapolate a and b.
inline gfx::PointF lerp(const InputPredictor::InputData& a,
                        const InputPredictor::InputData& b,
                        base::TimeTicks sample_time) {
  const float alpha =
      (sample_time - a.time_stamp) / (a.time_stamp - b.time_stamp);
  return a.pos + gfx::ScaleVector2d(a.pos - b.pos, alpha);
}

// This value is related to kResamplingScrollEventsExperimentalPrediction and
// may be adjusted based on experimentation results. Currently, CalculateLatency
// relies on reading values off of the field trial (which won't exist when we
// ship). As such, we introduce the following constant which can be used for the
// latency calculation.
constexpr double kPredictFrameAheadBy = 0.375;

// Align events to a few milliseconds before frame_time. This is to make the
// resampling either doing interpolation or extrapolating a closer future time
// so that resampled result is more accurate and has less noise. This adds some
// latency during resampling but a few ms should be fine.
inline constexpr auto kResampleLatency = base::Milliseconds(-5);

// Returns the latency to be used for resampling. This can be controlled via the
// kResampleScrollEventsLatency feature and its parameters.
base::TimeDelta GetResampleScrollEventLatency(base::TimeDelta frame_interval) {
  std::string mode = features::kResampleLatencyModeParam.Get();
  double value = features::kResampleLatencyValueParam.Get();

  if (mode == features::kResampleLatencyModeFixedMs) {
    return base::Milliseconds(value);
  } else {  // kFractional
    return frame_interval * value;
  }
}

}  // namespace

LinearResampling::LinearResampling() = default;

LinearResampling::~LinearResampling() = default;

const char* LinearResampling::GetName() const {
  return features::kPredictorNameLinearResampling;
}

void LinearResampling::Reset() {
  events_queue_.clear();
}

void LinearResampling::Update(const InputData& new_input) {
  // The last input received is at least kMaxDeltaTime away, we consider it
  // is a new trajectory
  if (!events_queue_.empty() &&
      new_input.time_stamp - events_queue_.front().time_stamp > kMaxTimeDelta) {
    Reset();
  }

  // Queue the new event.
  events_queue_.push_front(new_input);
  if (events_queue_.size() > kNumEventsForResampling)
    events_queue_.pop_back();
  DCHECK(events_queue_.size() <= kNumEventsForResampling);

  if (events_queue_.size() == kNumEventsForResampling)
    events_dt_ = events_queue_[0].time_stamp - events_queue_[1].time_stamp;
}

bool LinearResampling::HasPrediction() const {
  return events_queue_.size() == kNumEventsForResampling &&
         events_dt_ >= kResampleMinDelta;
}

std::unique_ptr<InputPredictor::InputData> LinearResampling::GeneratePrediction(
    base::TimeTicks frame_time,
    base::TimeDelta frame_interval) {
  if (!HasPrediction())
    return nullptr;

  base::TimeDelta resample_latency = ResampleLatency(frame_interval);
  base::TimeTicks sample_time = frame_time + resample_latency;

  // Clamping shouldn't affect prediction experiment, as we're predicting
  // further in the future.
  if (!base::FeatureList::IsEnabled(
          ::features::kResamplingScrollEventsExperimentalPrediction)) {
    base::TimeDelta max_prediction =
        std::min(kResampleMaxPrediction, events_dt_ / 2.0);

    sample_time =
        std::min(sample_time, events_queue_[0].time_stamp + max_prediction);
  }

  return std::make_unique<InputData>(
      lerp(events_queue_[0], events_queue_[1], sample_time), sample_time);
}

base::TimeDelta LinearResampling::TimeInterval() const {
  if (events_queue_.size() == kNumEventsForResampling) {
    return events_dt_;
  }
  return kTimeInterval;
}

base::TimeDelta LinearResampling::ResampleLatency(
    base::TimeDelta frame_interval) const {
  return latency_calculator_.GetResampleLatencyInternal(frame_interval);
}

base::TimeDelta LinearResampling::LatencyCalculator::GetResampleLatencyInternal(
    base::TimeDelta frame_interval) {
  // Cache |resample_latency_| and recalculate only when |frame_interval|
  // changes.
  if (frame_interval != frame_interval_ || resample_latency_.is_zero()) {
    frame_interval_ = frame_interval;
    resample_latency_ = CalculateLatency();
  }
  return resample_latency_;
}

base::TimeDelta LinearResampling::LatencyCalculator::CalculateLatency() {
  base::TimeDelta resample_latency;
  // If kResampleScrollEventsLatency is enabled, it takes precedence over
  // kResamplingScrollEventsExperimentalPrediction for latency calculation.
  if (base::FeatureList::IsEnabled(features::kResampleScrollEventsLatency)) {
    resample_latency = GetResampleScrollEventLatency(frame_interval_);
    TRACE_EVENT2("ui", "LatencyCalculator::CalculateLatency", "type",
                 "kResampleScrollEventsLatency", "resample_latency",
                 resample_latency.InMillisecondsF());
    return resample_latency;
  }

  // Otherwise, use kResamplingScrollEventsExperimentalPrediction settings.
  std::string prediction_type = GetFieldTrialParamValueByFeature(
      ::features::kResamplingScrollEventsExperimentalPrediction, "mode");

  if (prediction_type != ::features::kPredictionTypeFramesBased) {
    const bool feature_enabled = base::FeatureList::IsEnabled(
        ::features::kResamplingScrollEventsExperimentalPrediction);
    // If the feature is enabled and no field trial is active, default to using
    // kPredictFrameAheadBy. Tests that set up field trials need not hit this
    // path since they are testing specific latency values.
    resample_latency =
        kResampleLatency + (feature_enabled
                                ? (kPredictFrameAheadBy * frame_interval_)
                                : base::Milliseconds(0));
    TRACE_EVENT2("ui", "LatencyCalculator::CalculateLatency", "prediction_type",
                 (feature_enabled ? "frames" : "default"),
                 "predicting ahead by (in ms)",
                 resample_latency.InMillisecondsF());
    return resample_latency;
  }

  double latency = 0;
  if (!base::StringToDouble(
          GetFieldTrialParamValueByFeature(
              ::features::kResamplingScrollEventsExperimentalPrediction,
              "latency"),
          &latency)) {
    latency = 0.5;
  }

  resample_latency = latency * frame_interval_ + kResampleLatency;
  TRACE_EVENT2("ui", "LatencyCalculator::CalculateLatency", "prediction_type",
               prediction_type, "predicting ahead by (in ms)",
               resample_latency.InMillisecondsF());
  return resample_latency;
}

}  // namespace ui
