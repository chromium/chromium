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
      ui::InputPredictor::InputData result;
      EXPECT_TRUE(predictor_->GeneratePrediction(
          FromMilliseconds(timestamp_ms[i]), &result));
      EXPECT_NEAR(result.pos.x(), x[i], kEpsilon);
      EXPECT_NEAR(result.pos.y(), y[i], kEpsilon);
    }
    InputPredictor::InputData data = {gfx::PointF(x[i], y[i]),
                                      FromMilliseconds(timestamp_ms[i])};
    predictor_->Update(data);
  }
}

}  // namespace ui
