// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/linear_resampling.h"

#include <algorithm>

#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

namespace {
// Minimum time difference between last two consecutive events before attempting
// to resample.
constexpr base::TimeDelta kResampleMinDelta =
    base::TimeDelta::FromMilliseconds(2);
// Maximum time to predict forward from the last event, to avoid predicting too
// far into the future. This time is further bounded by 50% of the last time
// delta.
constexpr base::TimeDelta kResampleMaxPrediction =
    base::TimeDelta::FromMilliseconds(8);
// Align events to a few milliseconds before frame_time. This is to make the
// resampling either doing interpolation or extrapolating a closer future time
// so that resampled result is more accurate and has less noise. This adds some
// latency during resampling but a few ms should be fine.
constexpr base::TimeDelta kResampleLatency =
    base::TimeDelta::FromMilliseconds(5);

// Get position at |sample_time| by linear interpolate/extrapolate a and b.
inline gfx::PointF lerp(const InputPredictor::InputData& a,
                        const InputPredictor::InputData& b,
                        base::TimeTicks sample_time) {
  float alpha = (sample_time - a.time_stamp).InMillisecondsF() /
                (a.time_stamp - b.time_stamp).InMillisecondsF();
  return a.pos + gfx::ScaleVector2d(a.pos - b.pos, alpha);
}

}  // namespace

LinearResampling::LinearResampling() {}

LinearResampling::~LinearResampling() {}

const char* LinearResampling::GetName() const {
  return input_prediction::kScrollPredictorNameLinearResampling;
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
    base::TimeTicks frame_time) const {
  if (!HasPrediction())
    return nullptr;

  base::TimeTicks sample_time = frame_time - kResampleLatency;

  base::TimeDelta max_prediction =
      std::min(kResampleMaxPrediction, events_dt_ / 2.0);

  sample_time =
      std::min(sample_time, events_queue_[0].time_stamp + max_prediction);

  return std::make_unique<InputData>(
      lerp(events_queue_[0], events_queue_[1], sample_time), sample_time);
}

base::TimeDelta LinearResampling::TimeInterval() const {
  if (events_queue_.size() == kNumEventsForResampling) {
    return events_dt_;
  }
  return kTimeInterval;
}

}  // namespace ui
