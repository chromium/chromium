// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/average_lag_tracker.h"

#include "base/metrics/histogram_functions.h"

namespace ui {

AverageLagTracker::AverageLagTracker() = default;

AverageLagTracker::~AverageLagTracker() = default;

void AverageLagTracker::AddLatencyInFrame(
    const ui::LatencyInfo& latency,
    base::TimeTicks gpu_swap_begin_timestamp,
    const std::string& scroll_name) {
  base::TimeTicks event_timestamp;
  bool found_component = latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
      &event_timestamp);
  DCHECK(found_component);
  // Skip if no event timestamp.
  if (!found_component)
    return;

  if (scroll_name == "ScrollBegin") {
    // Flush all unfinished frames.
    while (!frame_lag_infos_.empty()) {
      frame_lag_infos_.front().lag_area += LagForUnfinishedFrame(
          frame_lag_infos_.front().rendered_accumulated_delta);
      frame_lag_infos_.front().lag_area_no_prediction += LagForUnfinishedFrame(
          frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);

      // Record UMA when it's the last item in queue.
      CalculateAndReportAverageLagUma(frame_lag_infos_.size() == 1);
    }
    // |accumulated_lag_| should be cleared/reset.
    DCHECK(accumulated_lag_ == 0);

    // Create ScrollBegin report, with report time equals to gpu swap time.
    LagAreaInFrame first_frame(gpu_swap_begin_timestamp);
    frame_lag_infos_.push_back(first_frame);

    // Reset fields.
    last_reported_time_ = event_timestamp;
    last_finished_frame_time_ = event_timestamp;
    last_event_accumulated_delta_ = 0;
    last_rendered_accumulated_delta_ = 0;
    is_begin_ = true;
  } else if (scroll_name == "ScrollUpdate" &&
             !last_event_timestamp_.is_null()) {
    // Only accept events in nondecreasing order.
    if ((event_timestamp - last_event_timestamp_).InMilliseconds() < 0)
      return;

    // Pop all frames where frame_time <= event_timestamp.
    while (!frame_lag_infos_.empty() &&
           frame_lag_infos_.front().frame_time <= event_timestamp) {
      base::TimeTicks front_time =
          std::max(last_event_timestamp_, last_finished_frame_time_);
      base::TimeTicks back_time = frame_lag_infos_.front().frame_time;
      frame_lag_infos_.front().lag_area +=
          LagBetween(front_time, back_time, latency, event_timestamp,
                     frame_lag_infos_.front().rendered_accumulated_delta);
      frame_lag_infos_.front().lag_area_no_prediction += LagBetween(
          front_time, back_time, latency, event_timestamp,
          frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);

      CalculateAndReportAverageLagUma();
    }

    // Initialize a new LagAreaInFrame when current_frame_time > frame_time.
    if (frame_lag_infos_.empty() ||
        gpu_swap_begin_timestamp > frame_lag_infos_.back().frame_time) {
      LagAreaInFrame new_frame(gpu_swap_begin_timestamp,
                               last_rendered_accumulated_delta_,
                               last_event_accumulated_delta_);
      frame_lag_infos_.push_back(new_frame);
    }

    // last_frame_time <= event_timestamp < frame_time
    if (!frame_lag_infos_.empty()) {
      // The front element in queue (if any) must satisfy frame_time >
      // event_timestamp, otherwise it would be popped in the while loop.
      DCHECK(last_finished_frame_time_ <= event_timestamp &&
             event_timestamp <= frame_lag_infos_.front().frame_time);
      base::TimeTicks front_time =
          std::max(last_finished_frame_time_, last_event_timestamp_);
      base::TimeTicks back_time = event_timestamp;

      frame_lag_infos_.front().lag_area +=
          LagBetween(front_time, back_time, latency, event_timestamp,
                     frame_lag_infos_.front().rendered_accumulated_delta);

      frame_lag_infos_.front().lag_area_no_prediction += LagBetween(
          front_time, back_time, latency, event_timestamp,
          frame_lag_infos_.front().rendered_accumulated_delta_no_prediction);
    }
  }

  last_event_timestamp_ = event_timestamp;
  last_event_accumulated_delta_ += latency.scroll_update_delta();
  last_rendered_accumulated_delta_ += latency.predicted_scroll_update_delta();
}

float AverageLagTracker::LagBetween(base::TimeTicks front_time,
                                    base::TimeTicks back_time,
                                    const LatencyInfo& latency,
                                    base::TimeTicks event_timestamp,
                                    float rendered_accumulated_delta) {
  // In some tests, we use const event time. return 0 to avoid divided by 0.
  if (event_timestamp == last_event_timestamp_)
    return 0;

  float front_delta =
      (last_event_accumulated_delta_ +
       (latency.scroll_update_delta() *
        ((front_time - last_event_timestamp_).InMillisecondsF() /
         (event_timestamp - last_event_timestamp_).InMillisecondsF()))) -
      rendered_accumulated_delta;

  float back_delta =
      (last_event_accumulated_delta_ +
       latency.scroll_update_delta() *

           ((back_time - last_event_timestamp_).InMillisecondsF() /
            (event_timestamp - last_event_timestamp_).InMillisecondsF())) -
      rendered_accumulated_delta;

  // Calculate the trapezoid area.
  if (front_delta * back_delta >= 0) {
    return 0.5f * std::abs(front_delta + back_delta) *
           (back_time - front_time).InMillisecondsF();
  }

  // Corner case that rendered_accumulated_delta is in between of front_pos
  // and back_pos.
  return 0.5f *
         std::abs((front_delta * front_delta + back_delta * back_delta) /
                  (back_delta - front_delta)) *
         (back_time - front_time).InMillisecondsF();
}

float AverageLagTracker::LagForUnfinishedFrame(
    float rendered_accumulated_delta) {
  base::TimeTicks last_time =
      std::max(last_event_timestamp_, last_finished_frame_time_);
  return std::abs(last_event_accumulated_delta_ - rendered_accumulated_delta) *
         (frame_lag_infos_.front().frame_time - last_time).InMillisecondsF();
}

void AverageLagTracker::CalculateAndReportAverageLagUma(bool send_anyway) {
  DCHECK(!frame_lag_infos_.empty());
  const LagAreaInFrame& frame_lag = frame_lag_infos_.front();

  DCHECK(frame_lag.lag_area >= 0.f);
  DCHECK(frame_lag.lag_area_no_prediction >= 0.f);
  accumulated_lag_ += frame_lag.lag_area;
  accumulated_lag_no_prediction_ += frame_lag.lag_area_no_prediction;

  if (is_begin_) {
    DCHECK(accumulated_lag_ == accumulated_lag_no_prediction_);
  }

  // |send_anyway| is true when we are flush all remaining frames on next
  // |ScrollBegin|. Otherwise record UMA when it's ScrollBegin, or when
  // reaching the 1 second gap.
  if (send_anyway || is_begin_ ||
      (frame_lag.frame_time - last_reported_time_).InSecondsF() >= 1.0f) {
    std::string scroll_name = is_begin_ ? "ScrollBegin" : "ScrollUpdate";
    const float time_delta =
        (frame_lag.frame_time - last_reported_time_).InMillisecondsF();
    const float scaled_lag = accumulated_lag_ / time_delta;
    base::UmaHistogramCounts1000(
        "Event.Latency." + scroll_name + ".Touch.AverageLag", scaled_lag);

    const float prediction_effect =
        (accumulated_lag_no_prediction_ - accumulated_lag_) / time_delta;
    // Log positive and negative prediction effects. ScrollBegin currently
    // doesn't take prediction into account so don't log for it.
    // Positive effect means that the prediction reduced the perceived lag,
    // where negative means prediction made lag worse (most likely due to
    // misprediction).
    if (!is_begin_) {
      if (prediction_effect >= 0.f) {
        base::UmaHistogramCounts1000(
            "Event.Latency.ScrollUpdate.Touch.AverageLag.PredictionPositive",
            prediction_effect);
      } else {
        base::UmaHistogramCounts1000(
            "Event.Latency.ScrollUpdate.Touch.AverageLag.PredictionNegative",
            -prediction_effect);
      }
    }
    accumulated_lag_ = 0;
    accumulated_lag_no_prediction_ = 0;
    last_reported_time_ = frame_lag.frame_time;
    is_begin_ = false;
  }

  last_finished_frame_time_ = frame_lag.frame_time;
  frame_lag_infos_.pop_front();
}

}  // namespace ui
