// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/prediction_metrics_handler.h"

#include "base/metrics/histogram_functions.h"

namespace ui {

PredictionMetricsHandler::PredictionMetricsHandler() {}
PredictionMetricsHandler::~PredictionMetricsHandler() {}

void PredictionMetricsHandler::AddRealEvent(const gfx::PointF& pos,
                                            const base::TimeTicks& time_stamp,
                                            const base::TimeTicks& frame_time,
                                            bool scrolling) {
  // Be sure real events are ordered over time
  DCHECK(events_queue_.empty() ||
         time_stamp >= events_queue_.back().time_stamp);
  EventData e;
  if (scrolling)
    e.pos = gfx::PointF(0, pos.y());
  else
    e.pos = pos;
  e.time_stamp = time_stamp;
  e.frame_time = frame_time;
  events_queue_.push_back(e);
}

void PredictionMetricsHandler::AddPredictedEvent(
    const gfx::PointF& pos,
    const base::TimeTicks& time_stamp,
    const base::TimeTicks& frame_time,
    bool scrolling) {
  // Be sure that the first real event is always anterior to the first
  // predicted event and that each predicted events are ordered over time
  DCHECK(!events_queue_.empty());
  DCHECK(time_stamp >= events_queue_.front().time_stamp);
  DCHECK(predicted_events_queue_.empty() ||
         time_stamp >= predicted_events_queue_.back().time_stamp);
  EventData e;
  if (scrolling)
    e.pos = gfx::PointF(0, pos.y());
  else
    e.pos = pos;
  e.time_stamp = time_stamp;
  e.frame_time = frame_time;
  predicted_events_queue_.push_back(e);
}

void PredictionMetricsHandler::EvaluatePrediction() {
  while (!predicted_events_queue_.empty()) {
    // Not enough events to compute the metrics, do not compute for now.
    if (events_queue_.size() < 2 ||
        events_queue_.back().time_stamp <=
            predicted_events_queue_.front().time_stamp ||
        events_queue_.back().time_stamp <=
            predicted_events_queue_.front().frame_time) {
      return;
    }

    ComputeMetrics();

    last_predicted_ = predicted_events_queue_.front().pos;
    last_interpolated_ = interpolated_;
    last_frame_interpolated_ = frame_interpolated_;
    predicted_events_queue_.pop_front();
  }
}

void PredictionMetricsHandler::Reset() {
  events_queue_.clear();
  predicted_events_queue_.clear();
  last_predicted_ = base::nullopt;
}

int PredictionMetricsHandler::GetInterpolatedEventForPredictedEvent(
    const base::TimeTicks& interpolation_timestamp,
    gfx::PointF* interpolated) {
  size_t idx = -1;
  while (idx + 1 < events_queue_.size() &&
         interpolation_timestamp >= events_queue_[idx + 1].time_stamp)
    idx++;

  DCHECK(idx >= 0);
  if (idx < 0 || idx + 1 >= events_queue_.size())
    return -1;

  float alpha =
      (interpolation_timestamp - events_queue_[idx].time_stamp)
          .InMillisecondsF() /
      (events_queue_[idx + 1].time_stamp - events_queue_[idx].time_stamp)
          .InMillisecondsF();
  *interpolated =
      events_queue_[idx].pos +
      ScaleVector2d(events_queue_[idx + 1].pos - events_queue_[idx].pos, alpha);
  return idx;
}

void PredictionMetricsHandler::ComputeMetrics() {
  // Compute interpolations at predicted time and frame time.
  int low_idx_interpolated = GetInterpolatedEventForPredictedEvent(
      predicted_events_queue_.front().time_stamp, &interpolated_);
  int low_idx_frame_interpolated = GetInterpolatedEventForPredictedEvent(
      predicted_events_queue_.front().frame_time, &frame_interpolated_);

  next_real_ = events_queue_[low_idx_interpolated + 1].pos;

  int first_needed_event =
      std::min(low_idx_interpolated, low_idx_frame_interpolated);
  // Return if any of the interpolation is not found.
  if (first_needed_event == -1)
    return;
  // Clean real events queue.
  for (int i = 0; i < first_needed_event - 1; i++)
    events_queue_.pop_front();

  std::string kPredictionMetrics = "Event.InputEventPrediction.Scroll.";

  double score = ComputeOverUnderPredictionMetric();
  if (score >= 0)
    base::UmaHistogramCounts1000(kPredictionMetrics + "OverPrediction", score);
  else
    base::UmaHistogramCounts1000(kPredictionMetrics + "UnderPrediction",
                                 -score);

  // Need |last_predicted_| to compute WrongDirection and Jitter metrics.
  if (!last_predicted_.has_value())
    return;

  base::UmaHistogramBoolean(kPredictionMetrics + "WrongDirection",
                            ComputeWrongDirectionMetric());
  base::UmaHistogramCounts1000(kPredictionMetrics + "PredictionJitter",
                               ComputePredictionJitterMetric());
  base::UmaHistogramCounts1000(kPredictionMetrics + "VisualJitter",
                               ComputeVisualJitterMetric());
}

double PredictionMetricsHandler::ComputeOverUnderPredictionMetric() {
  gfx::Vector2dF real_direction = next_real_ - interpolated_;
  gfx::Vector2dF relative_direction =
      predicted_events_queue_.front().pos - interpolated_;
  if (gfx::DotProduct(real_direction, relative_direction) >= 0)
    return relative_direction.Length();
  else
    return -relative_direction.Length();
}

bool PredictionMetricsHandler::ComputeWrongDirectionMetric() {
  gfx::Vector2dF real_direction = next_real_ - interpolated_;
  gfx::Vector2dF predicted_direction =
      predicted_events_queue_.front().pos - last_predicted_.value();
  return gfx::DotProduct(real_direction, predicted_direction) < 0;
}

double PredictionMetricsHandler::ComputePredictionJitterMetric() {
  gfx::Vector2dF delta = interpolated_ - predicted_events_queue_.front().pos;
  gfx::Vector2dF last_delta = last_interpolated_ - last_predicted_.value();
  return (delta - last_delta).Length();
}

double PredictionMetricsHandler::ComputeVisualJitterMetric() {
  gfx::Vector2dF delta =
      frame_interpolated_ - predicted_events_queue_.front().pos;
  gfx::Vector2dF last_delta =
      last_frame_interpolated_ - last_predicted_.value();
  return (delta - last_delta).Length();
}

}  // namespace ui
