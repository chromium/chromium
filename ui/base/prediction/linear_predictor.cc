// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/linear_predictor.h"

#include <algorithm>

#include "ui/base/ui_base_features.h"

namespace ui {

LinearPredictor::LinearPredictor(EquationOrder order) {
  equation_order_ = order;
}

LinearPredictor::~LinearPredictor() {}

const char* LinearPredictor::GetName() const {
  return equation_order_ == EquationOrder::kFirstOrder
             ? features::kPredictorNameLinearFirst
             : features::kPredictorNameLinearSecond;
}

void LinearPredictor::Reset() {
  events_queue_.clear();
}

size_t LinearPredictor::NumberOfEventsNeeded() {
  return static_cast<size_t>(equation_order_);
}

void LinearPredictor::Update(const InputData& new_input) {
  // The last input received is at least kMaxDeltaTime away, we consider it
  // is a new trajectory
  if (!events_queue_.empty() &&
      new_input.time_stamp - events_queue_.back().time_stamp > kMaxTimeDelta) {
    Reset();
  }

  // Queue the new event and keep only the number of last events needed
  events_queue_.push_back(new_input);
  if (events_queue_.size() > static_cast<size_t>(equation_order_)) {
    events_queue_.pop_front();
  }

  // Compute the current velocity
  if (events_queue_.size() >= static_cast<size_t>(EquationOrder::kFirstOrder)) {
    // Even if cur_velocity is empty the first time, last_velocity is only
    // used when 3 events are in the queue, so it will be initialized
    last_velocity_.set_x(cur_velocity_.x());
    last_velocity_.set_y(cur_velocity_.y());

    // We have at least 2 events to compute the current velocity
    // Get delta time between the last 2 events
    // Note: this delta time is also used to compute the acceleration term
    events_dt_ = (events_queue_.at(events_queue_.size() - 1).time_stamp -
                  events_queue_.at(events_queue_.size() - 2).time_stamp)
                     .InMillisecondsF();

    // Get delta pos between the last 2 events
    gfx::Vector2dF delta_pos = events_queue_.at(events_queue_.size() - 1).pos -
                               events_queue_.at(events_queue_.size() - 2).pos;

    // Get the velocity
    if (events_dt_ > 0) {
      cur_velocity_.set_x(ScaleVector2d(delta_pos, 1.0 / events_dt_).x());
      cur_velocity_.set_y(ScaleVector2d(delta_pos, 1.0 / events_dt_).y());
    } else {
      cur_velocity_.set_x(0);
      cur_velocity_.set_y(0);
    }
  }
}

bool LinearPredictor::HasPrediction() const {
  // Even if the current equation is second order, we still can predict a
  // first order
  return events_queue_.size() >=
         static_cast<size_t>(EquationOrder::kFirstOrder);
}

std::unique_ptr<InputPredictor::InputData> LinearPredictor::GeneratePrediction(
    base::TimeTicks predict_time,
    base::TimeDelta frame_interval) {
  if (!HasPrediction())
    return nullptr;

  float pred_dt =
      (predict_time - events_queue_.back().time_stamp).InMillisecondsF();

  // Compute the prediction
  // Note : a first order prediction is computed when only 2 events are
  // available in the second order predictor
  if (equation_order_ == EquationOrder::kSecondOrder && events_dt_ > 0 &&
      events_queue_.size() ==
          static_cast<size_t>(EquationOrder::kSecondOrder)) {
    // Add the acceleration term to the current result
    return std::make_unique<InputData>(GeneratePredictionSecondOrder(pred_dt),
                                       predict_time);
  }
  return std::make_unique<InputData>(GeneratePredictionFirstOrder(pred_dt),
                                     predict_time);
}

gfx::PointF LinearPredictor::GeneratePredictionFirstOrder(float pred_dt) const {
  return events_queue_.back().pos + ScaleVector2d(cur_velocity_, pred_dt);
}

gfx::PointF LinearPredictor::GeneratePredictionSecondOrder(
    float pred_dt) const {
  DCHECK(equation_order_ == EquationOrder::kSecondOrder);

  gfx::Vector2dF acceleration =
      ScaleVector2d(cur_velocity_ - last_velocity_, 1.0 / events_dt_);
  return events_queue_.back().pos + ScaleVector2d(cur_velocity_, pred_dt) +
         ScaleVector2d(acceleration, 0.5 * pred_dt * pred_dt);
}

base::TimeDelta LinearPredictor::TimeInterval() const {
  if (events_queue_.size() > 1) {
    return std::max(kMinTimeInterval, (events_queue_.back().time_stamp -
                                       events_queue_.front().time_stamp) /
                                          (events_queue_.size() - 1));
  }
  return kTimeInterval;
}

}  // namespace ui
