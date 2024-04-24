// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/prediction_metrics_handler.h"

#include <string_view>
#include <utility>

#include "base/cpu_reduction_experiment.h"
#include "base/metrics/histogram.h"
#include "base/strings/strcat.h"

namespace ui {
namespace {
base::HistogramBase* GetHistogram(std::string_view name,
                                  std::string_view suffix) {
  return base::Histogram::FactoryGet(
      base::StrCat({name, ".", suffix}), 1, 1000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}
}  // namespace

PredictionMetricsHandler::PredictionMetricsHandler(std::string histogram_name)
    : histogram_name_(std::move(histogram_name)),
      over_prediction_histogram_(
          *GetHistogram(histogram_name_, "OverPrediction")),
      under_prediction_histogram_(
          *GetHistogram(histogram_name_, "UnderPrediction")),
      prediction_score_histogram_(
          *GetHistogram(histogram_name_, "PredictionScore")),
      frame_over_prediction_histogram_(
          *GetHistogram(histogram_name_, "FrameOverPrediction")),
      frame_under_prediction_histogram_(
          *GetHistogram(histogram_name_, "FrameUnderPrediction")),
      frame_prediction_score_histogram_(
          *GetHistogram(histogram_name_, "FramePredictionScore")),
      prediction_jitter_histogram_(
          *GetHistogram(histogram_name_, "PredictionJitter")),
      visual_jitter_histogram_(*GetHistogram(histogram_name_, "VisualJitter")) {
}

PredictionMetricsHandler::~PredictionMetricsHandler() = default;

void PredictionMetricsHandler::AddRealEvent(const gfx::PointF& pos,
                                            const base::TimeTicks& time_stamp,
                                            const base::TimeTicks& frame_time,
                                            bool scrolling) {
  // Real events should arrive in order over time, and if they aren't then just
  // bail. Early out instead of DCHECKing in order to handle delegated ink
  // trails. Delegated ink trails may submit points out of order in a situation
  // such as three points with timestamps = 1, 2, and 3 making up the trail on
  // one frame, and then on the next frame only the points with timestamp 2 and
  // 3 make up the trail. In this case, 2 would be added as a real point again,
  // but it has a timestamp earlier than 3, so a DCHECK would fail. Early out
  // here will not impact correctness since 2 already exists in |events_queue_|.
  if (!events_queue_.empty() && time_stamp <= events_queue_.back().time_stamp) {
    // There can be situations where the metadata does not arrive in time for
    // the vsync. Rather than skipping drawing for that frame, the metadata is
    // kept and the trail is drawn from the metadata point to the latest
    // point in the trail. However, the metadata and points relatively near it
    // can be cleared from events_queue_ during ComputeMetrics(). Therefore the
    // following DCHECK is hit when the older points are re-added as real
    // events. Since those points are not relevant to the front of the trail,
    // where the prediction happens, they can safely be exempt from the
    // following DCHECK. Only points that are at or later than the front of the
    // events_queue_ need to be verified.
    if (time_stamp < events_queue_.front().time_stamp)
      return;

    // Confirm that the above assertion is true, and that timestamp 2 (from
    // the above example) exists in |events_queue_|.
    bool event_exists = false;
    for (uint64_t i = 0; i < events_queue_.size() && !event_exists; ++i) {
      if (events_queue_[i].time_stamp == time_stamp)
        event_exists = true;
    }
    DCHECK(event_exists);
    return;
  }

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
  DCHECK(!events_queue_.empty());
  // If the predicted event is prior to the first real event, ignore it as we
  // don't have enough data for interpolation.
  if (time_stamp < events_queue_.front().time_stamp)
    return;
  // TODO(nzolghadr): The following DCHECK is commented out due to
  // crbug.com/1017661. More investigation needs to be done as why this happens.
  // DCHECK(predicted_events_queue_.empty() ||
  //       time_stamp >= predicted_events_queue_.back().time_stamp);
  bool needs_sorting = false;
  if (!predicted_events_queue_.empty() &&
      time_stamp < predicted_events_queue_.back().time_stamp)
    needs_sorting = true;

  EventData e;
  if (scrolling)
    e.pos = gfx::PointF(0, pos.y());
  else
    e.pos = pos;
  e.time_stamp = time_stamp;
  e.frame_time = frame_time;
  predicted_events_queue_.push_back(e);

  // TODO(nzolghadr): This should never be needed. Something seems to be wrong
  // in the tests. See crbug.com/1017661.
  if (needs_sorting) {
    std::sort(predicted_events_queue_.begin(), predicted_events_queue_.end(),
              [](const EventData& a, const EventData& b) {
                return a.time_stamp < b.time_stamp;
              });
  }
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
  last_predicted_ = std::nullopt;
}

int PredictionMetricsHandler::GetInterpolatedEventForPredictedEvent(
    const base::TimeTicks& interpolation_timestamp,
    gfx::PointF* interpolated) {
  size_t idx = 0;
  while (idx < events_queue_.size() &&
         interpolation_timestamp >= events_queue_[idx].time_stamp)
    idx++;

  if (idx == 0 || idx == events_queue_.size())
    return -1;

  const float alpha =
      (interpolation_timestamp - events_queue_[idx - 1].time_stamp) /
      (events_queue_[idx].time_stamp - events_queue_[idx - 1].time_stamp);
  *interpolated =
      events_queue_[idx - 1].pos +
      ScaleVector2d(events_queue_[idx].pos - events_queue_[idx - 1].pos, alpha);
  return idx - 1;
}

void PredictionMetricsHandler::ComputeMetrics() {
  // Compute interpolations at predicted time and frame time.
  int low_idx_interpolated = GetInterpolatedEventForPredictedEvent(
      predicted_events_queue_.front().time_stamp, &interpolated_);
  int low_idx_frame_interpolated = GetInterpolatedEventForPredictedEvent(
      predicted_events_queue_.front().frame_time, &frame_interpolated_);

  next_real_ = events_queue_[low_idx_interpolated + 1].pos;
  next_real_point_after_frame_ =
      events_queue_[low_idx_frame_interpolated + 1].pos;

  int first_needed_event =
      std::min(low_idx_interpolated, low_idx_frame_interpolated);
  // Return if any of the interpolation is not found.
  if (first_needed_event == -1)
    return;
  // Clean real events queue.
  for (int i = 0; i < first_needed_event - 1; i++)
    events_queue_.pop_front();

  double score = ComputeOverUnderPredictionMetric();
  if (score >= 0) {
    over_prediction_histogram_->Add(score);
  } else {
    under_prediction_histogram_->Add(-score);
  }
  prediction_score_histogram_->Add(std::abs(score));

  double frame_score = ComputeFrameOverUnderPredictionMetric();
  if (frame_score >= 0) {
    frame_over_prediction_histogram_->Add(frame_score);
  } else {
    frame_under_prediction_histogram_->Add(-frame_score);
  }
  frame_prediction_score_histogram_->Add(std::abs(frame_score));

  // Need |last_predicted_| to compute Jitter metrics.
  if (!last_predicted_.has_value())
    return;

  prediction_jitter_histogram_->Add(ComputePredictionJitterMetric());
  visual_jitter_histogram_->Add(ComputeVisualJitterMetric());
}

double PredictionMetricsHandler::ComputeOverUnderPredictionMetric() const {
  gfx::Vector2dF real_direction = next_real_ - interpolated_;
  gfx::Vector2dF relative_direction =
      predicted_events_queue_.front().pos - interpolated_;
  if (gfx::DotProduct(real_direction, relative_direction) >= 0)
    return relative_direction.Length();
  else
    return -relative_direction.Length();
}

double PredictionMetricsHandler::ComputeFrameOverUnderPredictionMetric() const {
  gfx::Vector2dF real_direction =
      next_real_point_after_frame_ - frame_interpolated_;
  gfx::Vector2dF relative_direction =
      predicted_events_queue_.front().pos - frame_interpolated_;
  if (gfx::DotProduct(real_direction, relative_direction) >= 0)
    return relative_direction.Length();
  else
    return -relative_direction.Length();
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
