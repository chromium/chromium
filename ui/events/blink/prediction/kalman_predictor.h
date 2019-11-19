// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_KALMAN_PREDICTOR_H_
#define UI_EVENTS_BLINK_PREDICTION_KALMAN_PREDICTOR_H_

#include <deque>
#include <vector>

#include "ui/events/blink/prediction/input_predictor.h"
#include "ui/events/blink/prediction/kalman_filter.h"

namespace ui {

// Class to perform kalman filter prediction inherited from InputPredictor.
// This predictor uses kalman filters to predict the current status of the
// motion. Then it predict the future points using <current_position,
// predicted_velocity, predicted_acceleration>. Each kalman_filter will only
// be used to predict one dimension (x, y).
class KalmanPredictor : public InputPredictor {
 public:
  // Heuristic option enables changing the influence of acceleration based on
  // change of direction. Direction cut off enables discarding the prediction if
  // the predicted direction is opposite from the current direction.
  enum PredictionOptions {
    kNone = 0x0,
    kHeuristicsEnabled = 0x1,
    kDirectionCutOffEnabled = 0x2
  };

  explicit KalmanPredictor(unsigned int prediction_options);
  ~KalmanPredictor() override;

  const char* GetName() const override;

  // Reset the predictor to initial state.
  void Reset() override;

  // Store current input in queue.
  void Update(const InputData& cur_input) override;

  // Return if there is enough data in the queue to generate prediction.
  bool HasPrediction() const override;

  // Generate the prediction based on stored points and given time_stamp.
  // Return false if no prediction available.
  std::unique_ptr<InputData> GeneratePrediction(
      base::TimeTicks predict_time) const override;

  // Return the filtered value of time intervals.
  base::TimeDelta TimeInterval() const override;

 private:
  // The following functions get the predicted values from kalman filters.
  gfx::Vector2dF PredictPosition() const;
  gfx::Vector2dF PredictVelocity() const;
  gfx::Vector2dF PredictAcceleration() const;

  // Predictor for each axis.
  KalmanFilter x_predictor_;
  KalmanFilter y_predictor_;

  // Filter to smooth time intervals.
  KalmanFilter time_filter_;

  // Most recent input data.
  std::deque<InputData> last_points_;

  // Maximum time interval between first and last events in last points queue.
  static constexpr base::TimeDelta kMaxTimeInQueue =
      base::TimeDelta::FromMilliseconds(40);

  // Flags to determine the enabled prediction options.
  const unsigned int prediction_options_;

  DISALLOW_COPY_AND_ASSIGN(KalmanPredictor);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_KALMAN_PREDICTOR_H_
