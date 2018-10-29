// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scroll_predictor.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/kalman_predictor.h"
#include "ui/events/blink/prediction/least_squares_predictor.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace ui {

namespace {

constexpr char kPredictor[] = "predictor";
constexpr char kScrollPredictorTypeLsq[] = "lsq";
constexpr char kScrollPredictorTypeKalman[] = "kalman";

}  // namespace

ScrollPredictor::ScrollPredictor(bool enable_resampling)
    : enable_resampling_(enable_resampling) {
  // When resampling is enabled, set predictor type by resampling flag params;
  // otherwise, get predictor type parameters from kPredictorTypeChoice flag.
  std::string predictor_type =
      enable_resampling_
          ? GetFieldTrialParamValueByFeature(features::kResamplingScrollEvents,
                                             kPredictor)
          : GetFieldTrialParamValueByFeature(
                features::kScrollPredictorTypeChoice, kPredictor);

  if (predictor_type == kScrollPredictorTypeLsq)
    predictor_ = std::make_unique<LeastSquaresPredictor>();
  else if (predictor_type == kScrollPredictorTypeKalman)
    predictor_ = std::make_unique<KalmanPredictor>();
  else
    predictor_ = std::make_unique<EmptyPredictor>();
}

ScrollPredictor::~ScrollPredictor() = default;

void ScrollPredictor::ResetOnGestureScrollBegin(const WebGestureEvent& event) {
  DCHECK(event.GetType() == WebInputEvent::kGestureScrollBegin);
  // Only do resampling for scroll on touchscreen.
  if (event.SourceDevice() == blink::kWebGestureDeviceTouchscreen) {
    should_resample_scroll_events_ = true;
    Reset();
  }
}

void ScrollPredictor::ResampleScrollEvents(
    const EventWithCallback::OriginalEventList& original_events,
    base::TimeTicks frame_time,
    WebInputEvent* event) {
  if (!should_resample_scroll_events_)
    return;

  if (event->GetType() == WebInputEvent::kGestureScrollUpdate) {
    TRACE_EVENT_BEGIN0("input", "ScrollPredictor::ResampleScrollEvents");

    // TODO(eirage): When scroll events are coalesced with pinch, we can have
    // empty original event list. In that case, we can't use the original events
    // to update the prediction. We don't want to use the aggregated event to
    // update because of the event time stamp, so skip the prediction for now.
    if (original_events.empty())
      return;

    temporary_accumulated_delta_ = current_accumulated_delta_;
    for (auto& coalesced_event : original_events)
      ComputeAccuracy(coalesced_event.event_);

    for (auto& coalesced_event : original_events)
      UpdatePrediction(coalesced_event.event_, frame_time);

    if (enable_resampling_ && should_resample_scroll_events_)
      ResampleEvent(frame_time, event);

    TRACE_EVENT_END2("input", "ScrollPredictor::ResampleScrollEvents",
                     "OriginalPosition", current_accumulated_delta_.ToString(),
                     "PredictedPosition", last_accumulated_delta_.ToString());
  } else if (event->GetType() == WebInputEvent::kGestureScrollEnd) {
    should_resample_scroll_events_ = false;
  }
}

void ScrollPredictor::Reset() {
  predictor_->Reset();
  current_accumulated_delta_ = gfx::PointF();
  last_accumulated_delta_ = gfx::PointF();
}

void ScrollPredictor::UpdatePrediction(const WebScopedInputEvent& event,
                                       base::TimeTicks frame_time) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(*event);
  // When fling, GSU is sending per frame, resampling is not needed.
  if (gesture_event.data.scroll_update.inertial_phase ==
      WebGestureEvent::kMomentumPhase) {
    should_resample_scroll_events_ = false;
    return;
  }

  current_accumulated_delta_.Offset(gesture_event.data.scroll_update.delta_x,
                                    gesture_event.data.scroll_update.delta_y);
  InputPredictor::InputData data = {current_accumulated_delta_,
                                    gesture_event.TimeStamp()};

  predictor_->Update(data);
  last_event_timestamp_ = gesture_event.TimeStamp();
}

void ScrollPredictor::ResampleEvent(base::TimeTicks time_stamp,
                                    WebInputEvent* event) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  WebGestureEvent* gesture_event = static_cast<WebGestureEvent*>(event);

  gfx::PointF predicted_accumulated_delta = current_accumulated_delta_;
  InputPredictor::InputData result;
  if (predictor_->HasPrediction() &&
      predictor_->GeneratePrediction(time_stamp, &result)) {
    predicted_accumulated_delta = result.pos;
    gesture_event->SetTimeStamp(time_stamp);
  }

  // If the last resampled GSU over predict the delta, new GSU might try to
  // scroll back to make up the difference, which cause the scroll to jump back.
  // So we set the new delta to 0 when predicted delta is in different direction
  // to the original event.
  gfx::Vector2dF new_delta =
      predicted_accumulated_delta - last_accumulated_delta_;
  gesture_event->data.scroll_update.delta_x =
      (new_delta.x() * gesture_event->data.scroll_update.delta_x < 0)
          ? 0
          : new_delta.x();
  gesture_event->data.scroll_update.delta_y =
      (new_delta.y() * gesture_event->data.scroll_update.delta_y < 0)
          ? 0
          : new_delta.y();

  last_accumulated_delta_.Offset(gesture_event->data.scroll_update.delta_x,
                                 gesture_event->data.scroll_update.delta_y);
}

void ScrollPredictor::ComputeAccuracy(const WebScopedInputEvent& event) {
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(*event);

  base::TimeDelta time_delta = event->TimeStamp() - last_event_timestamp_;
  std::string suffix;
  if (time_delta < base::TimeDelta::FromMilliseconds(10))
    suffix = "Short";
  else if (time_delta < base::TimeDelta::FromMilliseconds(20))
    suffix = "Middle";
  else if (time_delta < base::TimeDelta::FromMilliseconds(35))
    suffix = "Long";
  else
    return;

  InputPredictor::InputData predict_result;
  temporary_accumulated_delta_.Offset(gesture_event.data.scroll_update.delta_x,
                                      gesture_event.data.scroll_update.delta_y);
  if (predictor_->HasPrediction() &&
      predictor_->GeneratePrediction(event->TimeStamp(), &predict_result)) {
    float distance =
        (predict_result.pos - gfx::PointF(temporary_accumulated_delta_))
            .Length();
    base::UmaHistogramCounts1000(
        "Event.InputEventPrediction.Accuracy.Scroll." + suffix,
        static_cast<int>(distance));
  }
}

}  // namespace ui
