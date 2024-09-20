// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_
#define UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_

#include <deque>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_base.h"
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
  explicit PredictionMetricsHandler(std::string histogram_name);
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
  double ComputeOverUnderPredictionMetric() const;

  // The score is the amount of pixels the predicted point is ahead/behind of
  // the real input curve. The curve point being an interpolation of the real
  // input points at the `frame_time` from the current predicted point.
  double ComputeFrameOverUnderPredictionMetric() const;

  // Compute the PredictionJitterMetric score.
  // The score is the euclidean distance between 2 successive variation of
  // prediction and the corresponding real events at the same timestamp. It is
  // an indicator of smoothness.
  double ComputePredictionJitterMetric();

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
  std::optional<gfx::PointF> last_predicted_ = std::nullopt;
  // The first real event position which time is later than the predicted time.
  gfx::PointF next_real_;

  // The first real event position which time is later than the frame time.
  gfx::PointF next_real_point_after_frame_;

  // Beginning of the full histogram name. It will have the various metrics'
  // names (.OverPrediction, .UnderPrediction, .PredictionJitter, .VisualJitter)
  // appended to it when counting the metric in a histogram.
  const std::string histogram_name_;

  // Histograms are never deleted we leak them at shutdown so it is fine to keep
  // a reference here.
  const raw_ref<base::HistogramBase> over_prediction_histogram_;
  const raw_ref<base::HistogramBase> under_prediction_histogram_;
  const raw_ref<base::HistogramBase> prediction_score_histogram_;
  const raw_ref<base::HistogramBase> frame_over_prediction_histogram_;
  const raw_ref<base::HistogramBase> frame_under_prediction_histogram_;
  const raw_ref<base::HistogramBase> frame_prediction_score_histogram_;
  const raw_ref<base::HistogramBase> prediction_jitter_histogram_;
  const raw_ref<base::HistogramBase> visual_jitter_histogram_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_PREDICTION_METRICS_HANDLER_H_
