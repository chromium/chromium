// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_LEAST_SQUARES_PREDICTOR_H_
#define UI_BASE_PREDICTION_LEAST_SQUARES_PREDICTOR_H_

#include <deque>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/base/prediction/input_predictor.h"
#include "ui/gfx/geometry/matrix3_f.h"

namespace ui {

// This class use a quadratic least square regression model:
// y = b0 + b1 * x + b2 * x ^ 2.
// See https://en.wikipedia.org/wiki/Linear_least_squares_(mathematics)
class COMPONENT_EXPORT(UI_BASE_PREDICTION) LeastSquaresPredictor
    : public InputPredictor {
 public:
  static constexpr size_t kSize = 3;

  explicit LeastSquaresPredictor();

  LeastSquaresPredictor(const LeastSquaresPredictor&) = delete;
  LeastSquaresPredictor& operator=(const LeastSquaresPredictor&) = delete;

  ~LeastSquaresPredictor() override;

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
      base::TimeTicks predict_time,
      base::TimeDelta frame_interval) override;

  // Return the averaged value of time intervals.
  base::TimeDelta TimeInterval() const override;

 private:
  // Generate X matrix from time_ queue.
  gfx::Matrix3F GetXMatrix() const;

  std::deque<double> x_queue_;
  std::deque<double> y_queue_;
  std::deque<base::TimeTicks> time_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_LEAST_SQUARES_PREDICTOR_H_
