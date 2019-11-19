// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/linear_resampling.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/input_predictor_unittest_helpers.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {
namespace test {

class LinearResamplingTest : public InputPredictorTest {
 public:
  explicit LinearResamplingTest() {}

  void SetUp() override {
    predictor_ = std::make_unique<ui::LinearResampling>();
  }

  DISALLOW_COPY_AND_ASSIGN(LinearResamplingTest);
};

// Test if the output name of the predictor is taking account of the
// equation order
TEST_F(LinearResamplingTest, GetName) {
  EXPECT_EQ(predictor_->GetName(),
            input_prediction::kScrollPredictorNameLinearResampling);
}

// Test that the number of events required to compute a prediction is correct
TEST_F(LinearResamplingTest, ShouldHavePrediction) {
  LinearResampling predictor;
  EXPECT_FALSE(predictor.HasPrediction());

  // 1st event.
  predictor.Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(0)}));
  EXPECT_FALSE(predictor.HasPrediction());

  // 2nd event.
  predictor.Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(8)}));
  EXPECT_TRUE(predictor.HasPrediction());

  predictor.Reset();
  EXPECT_FALSE(predictor.HasPrediction());
}

TEST_F(LinearResamplingTest, ResampleMinDelta) {
  EXPECT_FALSE(predictor_->HasPrediction());
  predictor_->Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(0)}));
  predictor_->Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(1)}));
  // No prediction when last_dt < kResampleMinDelta.
  EXPECT_FALSE(predictor_->HasPrediction());

  // Has prediction when last_dt >= kResampleMinDelta.
  predictor_->Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(3)}));
  EXPECT_TRUE(predictor_->HasPrediction());

  predictor_->Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(15)}));
  EXPECT_TRUE(predictor_->HasPrediction());

  // Predictor is reset when dt > kMaxTimeDelta.
  predictor_->Update(
      InputPredictor::InputData({gfx::PointF(0, 0), FromMilliseconds(36)}));
  EXPECT_FALSE(predictor_->HasPrediction());
}

TEST_F(LinearResamplingTest, ResamplingValue) {
  std::vector<double> x = {10, 20, 30};
  std::vector<double> y = {5, 25, 35};
  std::vector<double> t = {15, 24, 32};

  // Resample at frame_time = 33 ms, sample_time = 33-5 = 28ms.
  // Resample at frame_time = 41 ms, sample_time = 41-5 = 36ms.
  std::vector<double> pred_ts = {33, 41};
  std::vector<double> pred_x = {24.44, 35};
  std::vector<double> pred_y = {33.89, 40};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

TEST_F(LinearResamplingTest, ResamplingMaxPrediction) {
  std::vector<double> x = {10, 20};
  std::vector<double> y = {5, 10};
  std::vector<double> t = {10, 30};
  // Resample at frame_time = 45 ms, with max_prediction =
  // kResampleMaxPrediction, sample_time = 30 + 8ms = 38ms.
  std::vector<double> pred_ts = {45};
  std::vector<double> pred_x = {24};
  std::vector<double> pred_y = {12};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

TEST_F(LinearResamplingTest, ResamplingBoundLastDelta) {
  std::vector<double> x = {10, 20};
  std::vector<double> y = {5, 10};
  std::vector<double> t = {10, 14};
  // Resample at frame_time = 20 ms, sample time is bounded by 50% of the
  // last time delta, result in 14 + 2ms = 16ms.
  std::vector<double> pred_ts = {20};
  std::vector<double> pred_x = {22.5};
  std::vector<double> pred_y = {11.25};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

// Test time interval in first order
TEST_F(LinearResamplingTest, TimeInterval) {
  EXPECT_EQ(predictor_->TimeInterval(), kExpectedDefaultTimeInterval);
  std::vector<double> x = {10, 20};
  std::vector<double> y = {5, 25};
  std::vector<double> t = {17, 33};
  for (size_t i = 0; i < t.size(); i++) {
    predictor_->Update({gfx::PointF(x[i], y[i]), FromMilliseconds(t[i])});
  }
  EXPECT_EQ(predictor_->TimeInterval(),
            base::TimeDelta::FromMilliseconds(t[1] - t[0]));
}

}  // namespace test
}  // namespace ui
