// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_LINEAR_PREDICTOR_H_
#define UI_EVENTS_BLINK_PREDICTION_LINEAR_PREDICTOR_H_

#include <deque>

#include "ui/events/blink/prediction/input_predictor.h"

namespace ui {

// This class use a linear model for prediction
// You can choose between a first order equation:
// pred_p = last_p + velocity*pred_time
// and a second order equation:
// pred_p = last_p + velocity*pred_dt + 0.5*acceleration*pred_dt^2

class LinearPredictor : public InputPredictor {
 public:
  // Used to dissociate the order of the equation used but also used to
  // define the number of events needed by each model
  enum class EquationOrder : size_t { kFirstOrder = 2, kSecondOrder = 3 };

  explicit LinearPredictor(EquationOrder order);
  ~LinearPredictor() override;

  const char* GetName() const override;

  // Reset the predictor to initial state.
  void Reset() override;

  // Store current input in queue.
  void Update(const InputData& new_input) override;

  // Return if there is enough data in the queue to generate prediction.
  bool HasPrediction() const override;

  // Generate the prediction based on stored points and given time_stamp.
  // Return false if no prediction available.
  std::unique_ptr<InputData> GeneratePrediction(
      base::TimeTicks predict_time) const override;

  // Return the average time delta in the event queue.
  base::TimeDelta TimeInterval() const override;

  // Return the number of events needed to compute a prediction
  size_t NumberOfEventsNeeded();

 private:
  gfx::PointF GeneratePredictionFirstOrder(float pred_dt) const;

  gfx::PointF GeneratePredictionSecondOrder(float pred_dt) const;

  // Store the last events received
  std::deque<InputData> events_queue_;

  // Store the equation order of the predictor
  // The enum value also represents the number of events needed to compute the
  // prediction
  EquationOrder equation_order_;

  // Store the current velocity of the 2 last events
  gfx::Vector2dF cur_velocity_;

  // Store the last velocity of the 2 last past events
  gfx::Vector2dF last_velocity_;

  // Store the current delta time between the last 2 events
  float events_dt_;

  DISALLOW_COPY_AND_ASSIGN(LinearPredictor);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_LINEAR_PREDICTOR_H_
