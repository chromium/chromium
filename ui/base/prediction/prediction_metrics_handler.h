// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_
#define UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_

#include <deque>
#include <unordered_map>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

namespace test {
class PredictionMetricsHandlerTest;
}

// Class used for evaluating input prediction.
// The basic idea is to buffer predicted and real events to be able to compare
// a predicted position to its corresponding interpolated real position with
// few metrics.
class COMPONENT_EXPORT(UI_BASE_PREDICTION) PredictionMetricsHandler {
 public:
  explicit PredictionMetricsHandler(const char* histogram_name);
  ~PredictionMetricsHandler();

  // Struct used to store predicted and real event information.
  struct EventData {
    // Position of the event
    gfx::PointF pos;
    // Timestamp of the event
    base::TimeTicks time_stamp;
    // frame_time of the event
    base::TimeTicks frame_time;
  };

  // Buffers a real event.
  void AddRealEvent(const gfx::PointF& pos,
                    const base::TimeTicks& time_stamp,
                    const base::TimeTicks& frame_time,
                    bool scrolling = false);

  // Buffers a predicted event.
  void AddPredictedEvent(const gfx::PointF& pos,
                         const base::TimeTicks& time_stamp,
                         const base::TimeTicks& frame_time,
                         bool scrolling = false);

  void EvaluatePrediction();

  // Cleans all events buffers
  void Reset();

 private:
  friend class test::PredictionMetricsHandlerTest;

  // Computes necessary interpolations used for computing the metrics
  void ComputeMetrics();

  // Compute the OverUnderPredictionMetric score.
  // The score is the amount of pixels the predicted point is ahead of
  // the real point. If the score is positive, the prediction is OverPredicting,
  // otherwise UnderPredicting.
  double ComputeOverUnderPredictionMetric();

  // Compute the PredictionJitterMetric score.
  // The score is the euclidean distance between 2 successive variation of
  // prediction and the corresponding real events at the same timestamp. It is
  // an indicator of smoothness.
  double ComputePredictionJitterMetric();

  // Compute the WrongDirectionMetric score.
  // The score is a boolean (as double) indicating whether the prediction is
  // in the same direction as the real trajectory..
  bool ComputeWrongDirectionMetric();

  // Compute the VisualJitterMetric score.
  // The score is the euclidean distance between 2 successive variation of
  // prediction and the corresponding real events at frame time. It is
  // an indicator of smoothness.
  double ComputeVisualJitterMetric();

  // Get the interpolated position from the real events at a given timestamp.
  // Returns the index of the last real event which timestamp is smaller than
  // the |interpolation_timestamp|. Returns -1 if not found.
  int GetInterpolatedEventForPredictedEvent(
      const base::TimeTicks& interpolation_timestamp,
      gfx::PointF* interpolated);

  // Queues used for buffering real and predicted events.
  std::deque<EventData> events_queue_;
  std::deque<EventData> predicted_events_queue_;

  // Interpolated points of real events.
  gfx::PointF interpolated_, frame_interpolated_;
  gfx::PointF last_interpolated_, last_frame_interpolated_;
  // Last predicted point that pop from predicted_event_queue_. Use for
  // computing Jitter metrics.
  base::Optional<gfx::PointF> last_predicted_ = base::nullopt;
  // The first real event position which time is later than the predicted time.
  gfx::PointF next_real_;

  // Beginning of the full histogram name. It will have the various metrics'
  // names (.OverPrediction, .UnderPrediction, .WrongDirection,
  // .PredictionJitter, .VisualJitter) appended to it when counting the metric
  // in a histogram.
  const char* const histogram_name_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_
