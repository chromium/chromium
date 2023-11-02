// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_INPUT_PREDICTOR_UNITTEST_HELPERS_H_
#define UI_BASE_PREDICTION_INPUT_PREDICTOR_UNITTEST_HELPERS_H_

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/prediction/input_predictor.h"
#include "ui/base/prediction/prediction_unittest_helpers.h"

namespace ui {

constexpr base::TimeDelta kExpectedDefaultTimeInterval = base::Milliseconds(8);

// Base class for predictor unit tests
class InputPredictorTest : public testing::Test {
 public:
  InputPredictorTest();

  InputPredictorTest(const InputPredictorTest&) = delete;
  InputPredictorTest& operator=(const InputPredictorTest&) = delete;

  ~InputPredictorTest() override;

  static base::TimeTicks FromMilliseconds(double ms) {
    return test::PredictionUnittestHelpers::GetStaticTimeStampForTests() +
           base::Milliseconds(ms);
  }

  void ValidatePredictor(const std::vector<double>& x,
                         const std::vector<double>& y,
                         const std::vector<double>& timestamp_ms);

  void ValidatePredictor(const std::vector<double>& events_x,
                         const std::vector<double>& events_y,
                         const std::vector<double>& events_ts_ms,
                         const std::vector<double>& prediction_ts_ms,
                         const std::vector<double>& predicted_x,
                         const std::vector<double>& predicted_y);

 protected:
  static constexpr double kEpsilon = 0.1;

  std::unique_ptr<InputPredictor> predictor_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_INPUT_PREDICTOR_UNITTEST_HELPERS_H_
