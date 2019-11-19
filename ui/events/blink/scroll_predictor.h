// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_
#define UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_

#include <vector>

#include "ui/events/base_event_utils.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/input_predictor.h"
#include "ui/events/blink/prediction/prediction_metrics_handler.h"

namespace ui {

namespace test {
class ScrollPredictorTest;
}

// This class handles resampling GestureScrollUpdate events on InputHandlerProxy
// at |BeginFrame| signal, before events been dispatched. The predictor use
// original events to update the prediction and align the aggregated event
// timestamp and delta_x/y to the VSync time.
class ScrollPredictor {
 public:
  // Select the predictor type from field trial params and initialize the
  // predictor.
  explicit ScrollPredictor();
  ~ScrollPredictor();

  // Reset the predictors on each GSB.
  void ResetOnGestureScrollBegin(const blink::WebGestureEvent& event);

  // Resampling GestureScrollUpdate events. Updates the prediction with events
  // in original events list, and apply the prediction to the aggregated GSU
  // event if enable_resampling is true.
  std::unique_ptr<EventWithCallback> ResampleScrollEvents(
      std::unique_ptr<EventWithCallback> event_with_callback,
      base::TimeTicks frame_time);

 private:
  friend class test::InputHandlerProxyEventQueueTest;
  friend class test::ScrollPredictorTest;

  // Reset predictor and clear accumulated delta. This should be called on
  // GestureScrollBegin.
  void Reset();

  // Update the prediction with GestureScrollUpdate deltaX and deltaY
  void UpdatePrediction(const WebScopedInputEvent& event,
                        base::TimeTicks frame_time);

  // Apply resampled deltaX/deltaY to gesture events
  void ResampleEvent(base::TimeTicks frame_time,
                     blink::WebInputEvent* event,
                     LatencyInfo* latency_info);

  // Reports metrics scores UMA histogram based on the metrics defined
  // in |PredictionMetricsHandler|
  void EvaluatePrediction();

  std::unique_ptr<InputPredictor> predictor_;
  std::unique_ptr<InputFilter> filter_;

  std::unique_ptr<FilterFactory> filter_factory_;

  // Whether predicted scroll events should be filtered or not
  bool filtering_enabled_ = false;

  // Total scroll delta from original scroll update events, used for calculating
  // predictions. Reset on GestureScrollBegin.
  gfx::PointF current_event_accumulated_delta_;
  // Predicted accumulated delta from last vsync, use for calculating delta_x
  // and delta_y for the resampled/predicted event.
  gfx::PointF last_predicted_accumulated_delta_;

  // Whether current scroll event should be resampled.
  bool should_resample_scroll_events_ = false;

  // Handler used for evaluating the prediction
  PredictionMetricsHandler metrics_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScrollPredictor);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_SCROLL_PREDICTOR_H_
