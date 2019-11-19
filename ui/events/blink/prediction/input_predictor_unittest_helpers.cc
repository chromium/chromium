// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/input_predictor_unittest_helpers.h"

namespace ui {

InputPredictorTest::InputPredictorTest() = default;
InputPredictorTest::~InputPredictorTest() = default;

void InputPredictorTest::ValidatePredictor(
    const std::vector<double>& x,
    const std::vector<double>& y,
    const std::vector<double>& timestamp_ms) {
  predictor_->Reset();
  for (size_t i = 0; i < timestamp_ms.size(); i++) {
    if (predictor_->HasPrediction()) {
      auto result =
          predictor_->GeneratePrediction(FromMilliseconds(timestamp_ms[i]));
      EXPECT_TRUE(result);
      EXPECT_NEAR(result->pos.x(), x[i], kEpsilon);
      EXPECT_NEAR(result->pos.y(), y[i], kEpsilon);
    }
    InputPredictor::InputData data = {gfx::PointF(x[i], y[i]),
                                      FromMilliseconds(timestamp_ms[i])};
    predictor_->Update(data);
  }
}

void InputPredictorTest::ValidatePredictor(
    const std::vector<double>& events_x,
    const std::vector<double>& events_y,
    const std::vector<double>& events_time_ms,
    const std::vector<double>& prediction_time_ms,
    const std::vector<double>& predicted_x,
    const std::vector<double>& predicted_y) {
  predictor_->Reset();
  std::vector<double> computed_x;
  std::vector<double> computed_y;
  size_t current_prediction_index = 0;
  for (size_t i = 0; i < events_time_ms.size(); i++) {
    InputPredictor::InputData data = {gfx::PointF(events_x[i], events_y[i]),
                                      FromMilliseconds(events_time_ms[i])};
    predictor_->Update(data);

    if (predictor_->HasPrediction()) {
      auto result = predictor_->GeneratePrediction(
          FromMilliseconds(prediction_time_ms[current_prediction_index]));
      EXPECT_TRUE(result);
      computed_x.push_back(result->pos.x());
      computed_y.push_back(result->pos.y());
      EXPECT_GT(result->time_stamp, base::TimeTicks());
      current_prediction_index++;
    }
  }

  EXPECT_TRUE(computed_x.size() == predicted_x.size());
  if (computed_x.size() == predicted_x.size()) {
    for (size_t i = 0; i < predicted_x.size(); i++) {
      EXPECT_NEAR(computed_x[i], predicted_x[i], kEpsilon);
      EXPECT_NEAR(computed_y[i], predicted_y[i], kEpsilon);
    }
  }
}

}  // namespace ui
