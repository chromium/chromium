// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/linear_resampling.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/prediction/input_predictor_unittest_helpers.h"
#include "ui/base/ui_base_features.h"

namespace ui {
namespace test {

class LinearResamplingTest : public InputPredictorTest {
 public:
  explicit LinearResamplingTest() {}

  LinearResamplingTest(const LinearResamplingTest&) = delete;
  LinearResamplingTest& operator=(const LinearResamplingTest&) = delete;

  void SetUp() override { predictor_ = std::make_unique<LinearResampling>(); }

  void ValidatePredictorFrameBased(
      const std::vector<double>& events_x,
      const std::vector<double>& events_y,
      const std::vector<double>& events_time_ms,
      const std::vector<double>& prediction_time_ms,
      const std::vector<double>& predicted_x,
      const std::vector<double>& predicted_y,
      const double vsync_frequency) {
    // LinearResampling* predictor =
    // dynamic_cast<LinearResampling*>(predictor_.get());
    base::TimeDelta frame_interval = base::Seconds(1.0f / vsync_frequency);

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
            FromMilliseconds(prediction_time_ms[current_prediction_index]),
            frame_interval);
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

  base::test::ScopedFeatureList feature_list;
};

// Test if the output name of the predictor is taking account of the
// equation order
TEST_F(LinearResamplingTest, GetName) {
  EXPECT_EQ(predictor_->GetName(), features::kPredictorNameLinearResampling);
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
  EXPECT_EQ(predictor_->TimeInterval(), base::Milliseconds(t[1] - t[0]));
}

// Tests resampling with the experimental latency if +3.3ms instead of
// the default -5ms.
TEST_F(LinearResamplingTest, ResamplingValueWithExperimentalLatencyTimeBased) {
  base::FieldTrialParams params;
  params["mode"] = ::features::kPredictionTypeTimeBased;
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      features::kResamplingScrollEventsExperimentalPrediction, params);

  std::vector<double> x = {10, 20, 30};
  std::vector<double> y = {5, 25, 35};
  std::vector<double> t = {15, 24, 32};

  // Resample at `frame_time` = 24.7 ms, `sample_time` = 24.7+3.3 = 28ms.
  // Resample at `frame_time` = 32.7 ms, `sample_time` = 32.7+3.3 = 36ms.
  std::vector<double> pred_ts = {24.7, 32.7};
  std::vector<double> pred_x = {24.44, 35};
  std::vector<double> pred_y = {33.89, 40};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

// Tests resampling with the experimental latency if +1ms (using switch) instead
// of the default -5ms.
TEST_F(LinearResamplingTest,
       ResamplingValueWithExperimentalLatencyTimeBasedSwitch) {
  base::FieldTrialParams params;
  params["mode"] = ::features::kPredictionTypeTimeBased;
  params["latency"] = "1.0";
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      features::kResamplingScrollEventsExperimentalPrediction, params);

  std::vector<double> x = {10, 20, 30};
  std::vector<double> y = {5, 25, 35};
  std::vector<double> t = {15, 24, 32};

  // Resample at `frame_time` = 27 ms, `sample_time` = 27+1 = 28ms.
  // Resample at `frame_time` = 35 ms, `sample_time` = 35+1 = 36ms.
  std::vector<double> pred_ts = {27, 35};
  std::vector<double> pred_x = {24.44, 35};
  std::vector<double> pred_y = {33.89, 40};
  ValidatePredictor(x, y, t, pred_ts, pred_x, pred_y);
}

// Tests resampling with the experimental latency if +0.5*`frame_interval`
// instead of the default -5ms.
TEST_F(LinearResamplingTest, ResamplingValueWithExperimentalLatencyFrameBased) {
  base::FieldTrialParams params;
  params["mode"] = ::features::kPredictionTypeFramesBased;
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      features::kResamplingScrollEventsExperimentalPrediction, params);

  std::vector<double> x = {10, 20, 30};
  std::vector<double> y = {5, 25, 35};
  std::vector<double> t = {15, 24, 32};

  // Using 100Hz frequency => `frame_interval` = 10ms
  // Resample at `frame_time` = 33 ms, `sample_time` = 28-5+0.5*10 = 28ms.
  // Resample at `frame_time` = 41 ms, `sample_time` = 36-5+0.5*10 = 36ms.
  std::vector<double> pred_ts = {28, 36};
  std::vector<double> pred_x = {24.44, 35};
  std::vector<double> pred_y = {33.89, 40};
  ValidatePredictorFrameBased(x, y, t, pred_ts, pred_x, pred_y, 100);
}

// Tests resampling with the experimental latency if +0.5*`frame_interval`
// (using switch) instead of the default -5ms.
TEST_F(LinearResamplingTest,
       ResamplingValueWithExperimentalLatencyFrameBasedSwitch) {
  base::FieldTrialParams params;
  params["mode"] = ::features::kPredictionTypeFramesBased;
  params["latency"] = "1.0";
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      features::kResamplingScrollEventsExperimentalPrediction, params);

  std::vector<double> x = {10, 20, 30};
  std::vector<double> y = {5, 25, 35};
  std::vector<double> t = {15, 24, 32};

  // Using 200Hz frequency => `frame_interval` = 5ms
  // Resample at `frame_time` = 33 ms, `sample_time` = 28-5+1*5 = 28ms.
  // Resample at `frame_time` = 41 ms, `sample_time` = 36-5+1*5 = 36ms.
  std::vector<double> pred_ts = {28, 36};
  std::vector<double> pred_x = {24.44, 35};
  std::vector<double> pred_y = {33.89, 40};
  ValidatePredictorFrameBased(x, y, t, pred_ts, pred_x, pred_y, 200);
}

}  // namespace test
}  // namespace ui
