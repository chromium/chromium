// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/linear_predictor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/input_predictor_unittest_helpers.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {
namespace test {

class LinearPredictorFirstOrderTest : public InputPredictorTest {
 public:
  explicit LinearPredictorFirstOrderTest() {}

  void SetUp() override {
    predictor_ = std::make_unique<ui::LinearPredictor>(
        LinearPredictor::EquationOrder::kFirstOrder);
  }

  DISALLOW_COPY_AND_ASSIGN(LinearPredictorFirstOrderTest);
};

class LinearPredictorSecondOrderTest : public InputPredictorTest {
 public:
  explicit LinearPredictorSecondOrderTest() {}

  void SetUp() override {
    predictor_ = std::make_unique<ui::LinearPredictor>(
        LinearPredictor::EquationOrder::kSecondOrder);
  }

  DISALLOW_COPY_AND_ASSIGN(LinearPredictorSecondOrderTest);
};

// Test if the output name of the predictor is taking account of the
// equation order
TEST_F(LinearPredictorFirstOrderTest, GetName) {
  EXPECT_EQ(predictor_->GetName(),
            input_prediction::kScrollPredictorNameLinearFirst);
}

// Test if the output name of the predictor is taking account of the
// equation order
TEST_F(LinearPredictorSecondOrderTest, GetName) {
  EXPECT_EQ(predictor_->GetName(),
            input_prediction::kScrollPredictorNameLinearSecond);
}

// Test that the number of events required to compute a prediction is correct
TEST_F(LinearPredictorFirstOrderTest, ShouldHavePrediction) {
  LinearPredictor predictor(LinearPredictor::EquationOrder::kFirstOrder);
  size_t n = static_cast<size_t>(LinearPredictor::EquationOrder::kFirstOrder);
  for (size_t i = 0; i < n; i++) {
    EXPECT_FALSE(predictor.HasPrediction());
    predictor.Update(InputPredictor::InputData());
  }
  EXPECT_TRUE(predictor.HasPrediction());
  predictor.Reset();
  EXPECT_FALSE(predictor.HasPrediction());
}

// Test that the number of events required to compute a prediction is correct
TEST_F(LinearPredictorSecondOrderTest, ShouldHavePrediction) {
  LinearPredictor predictor(LinearPredictor::EquationOrder::kSecondOrder);
  size_t n1 = static_cast<size_t>(LinearPredictor::EquationOrder::kFirstOrder);
  size_t n2 = static_cast<size_t>(LinearPredictor::EquationOrder::kSecondOrder);
  for (size_t i = 0; i < n2; i++) {
    if (i < n1)
      EXPECT_FALSE(predictor.HasPrediction());
    else
      EXPECT_TRUE(predictor.HasPrediction());
    predictor.Update(InputPredictor::InputData());
  }
  EXPECT_TRUE(predictor.HasPrediction());
  predictor.Reset();
  EXPECT_FALSE(predictor.HasPrediction());
}

TEST_F(LinearPredictorFirstOrderTest, PredictedValue) {
  std::vector<double> x = {10, 20};
  std::vector<double> y = {5, 25};
  std::vector<double> t = {17, 33};
  // Compensating 23 ms
  // 1st order prediction at 33 + 23 = 56 ms

  std::vector<double> pred_ts = {56};
  std::vector<double> pred_x = {34.37};
  std::vector<double> pred_y = {53.75};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

TEST_F(LinearPredictorSecondOrderTest, PredictedValue) {
  std::vector<double> x = {0, 10, 20};
  std::vector<double> y = {0, 5, 25};
  std::vector<double> t = {0, 17, 33};
  // Compensating 23 ms in both results
  // 1st order prediction at 17 + 23 = 40 ms
  // 2nd order prediction at 33 + 23 = 56 ms
  std::vector<double> pred_ts = {40, 56};
  std::vector<double> pred_x = {23.52, 34.98};
  std::vector<double> pred_y = {11.76, 69.55};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

// Test constant position and constant velocity
TEST_F(LinearPredictorSecondOrderTest, ConstantPositionAndVelocity) {
  std::vector<double> x = {10, 10, 10, 10, 10};  // constant position
  std::vector<double> y = {0, 5, 10, 15, 20};    // constant velocity
  std::vector<double> t = {0, 7, 14, 21, 28};    // regular interval
  // since velocity is constant, acceleration should be 0 which simplifies
  // computations
  // Compensating 10 ms in all results
  std::vector<double> pred_ts = {17, 24, 31, 38};
  std::vector<double> pred_x = {10, 10, 10, 10};
  std::vector<double> pred_y = {12.14, 17.14, 22.14, 27.14};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

// Test time interval in first order
TEST_F(LinearPredictorFirstOrderTest, TimeInterval) {
  EXPECT_EQ(predictor_->TimeInterval(), kExpectedDefaultTimeInterval);
  std::vector<double> x = {10, 20};
  std::vector<double> y = {5, 25};
  std::vector<double> t = {17, 33};
  size_t n = static_cast<size_t>(LinearPredictor::EquationOrder::kFirstOrder);
  for (size_t i = 0; i < n; i++) {
    predictor_->Update({gfx::PointF(x[i], y[i]), FromMilliseconds(t[i])});
  }
  EXPECT_EQ(predictor_->TimeInterval(),
            base::TimeDelta::FromMilliseconds(t[1] - t[0]));
}

// Test time interval in second order
TEST_F(LinearPredictorSecondOrderTest, TimeInterval) {
  EXPECT_EQ(predictor_->TimeInterval(), kExpectedDefaultTimeInterval);
  std::vector<double> x = {0, 10, 20};
  std::vector<double> y = {0, 5, 25};
  std::vector<double> t = {0, 17, 33};
  size_t n = static_cast<size_t>(LinearPredictor::EquationOrder::kSecondOrder);
  for (size_t i = 0; i < n; i++) {
    predictor_->Update({gfx::PointF(x[i], y[i]), FromMilliseconds(t[i])});
  }
  EXPECT_EQ(predictor_->TimeInterval(),
            base::TimeDelta::FromMillisecondsD((t[2] - t[0]) / 2));
}

}  // namespace test
}  // namespace ui