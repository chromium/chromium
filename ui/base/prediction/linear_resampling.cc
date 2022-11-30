// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/linear_resampling.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
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
// Align events to a few milliseconds before frame_time. This is to make the
// resampling either doing interpolation or extrapolating a closer future time
// so that resampled result is more accurate and has less noise. This adds some
// latency during resampling but a few ms should be fine.
constexpr auto kResampleLatency = base::Milliseconds(-5);
// The optimal prediction anticipation from experimentation: In the study
// https://bit.ly/3iyQf8V we found that, on a machine with VSync at 60Hz, adding
// 1/2 * frame_interval (on top of kResampleLatency) minimizes the Lag on touch
// scrolling. + 1/2 * (1/60) - 5ms = 3.3ms.
constexpr auto kResampleLatencyExperimental = base::Milliseconds(3.3);

// Get position at |sample_time| by linear interpolate/extrapolate a and b.
inline gfx::PointF lerp(const InputPredictor::InputData& a,
                        const InputPredictor::InputData& b,
                        base::TimeTicks sample_time) {
  const float alpha =
      (sample_time - a.time_stamp) / (a.time_stamp - b.time_stamp);
  return a.pos + gfx::ScaleVector2d(a.pos - b.pos, alpha);
}

}  // namespace

LinearResampling::LinearResampling() {}

LinearResampling::~LinearResampling() {}

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

  base::TimeDelta resample_latency =
      latency_calculator_.GetResampleLatency(frame_interval);
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

base::TimeDelta LinearResampling::LatencyCalculator::GetResampleLatency(
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
  std::string prediction_type = GetFieldTrialParamValueByFeature(
      ::features::kResamplingScrollEventsExperimentalPrediction, "mode");

  if (prediction_type != ::features::kPredictionTypeTimeBased &&
      prediction_type != ::features::kPredictionTypeFramesBased)
    return kResampleLatency;

  std::string latency_value = GetFieldTrialParamValueByFeature(
      ::features::kResamplingScrollEventsExperimentalPrediction, "latency");
  double latency;
  if (base::StringToDouble(latency_value, &latency)) {
    return prediction_type == ::features::kPredictionTypeTimeBased
               ? base::Milliseconds(latency)
               : latency * frame_interval_ + kResampleLatency;
  }

  return prediction_type == ::features::kPredictionTypeTimeBased
             ? kResampleLatencyExperimental
             : 0.5 * frame_interval_ + kResampleLatency;
}

}  // namespace ui
