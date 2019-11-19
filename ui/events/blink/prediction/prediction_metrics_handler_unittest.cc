// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/prediction_metrics_handler.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"

using base::Bucket;
using testing::ElementsAre;

namespace ui {
namespace test {
namespace {

base::TimeTicks MillisecondsToTestTimeTicks(int64_t ms) {
  return blink::WebInputEvent::GetStaticTimeStampForTests() +
         base::TimeDelta::FromMilliseconds(ms);
}

}  // namespace

class PredictionMetricsHandlerTest : public testing::Test {
 public:
  explicit PredictionMetricsHandlerTest() {}

  void SetUp() override {
    metrics_handler_ = std::make_unique<PredictionMetricsHandler>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  DISALLOW_COPY_AND_ASSIGN(PredictionMetricsHandlerTest);

  int GetInterpolatedEventForPredictedEvent(const base::TimeTicks& timestamp,
                                            gfx::PointF* interpolated) {
    return metrics_handler_->GetInterpolatedEventForPredictedEvent(
        timestamp, interpolated);
  }

  ::testing::AssertionResult HistogramSizeEq(const char* histogram_name,
                                             int size) {
    uint64_t histogram_size =
        histogram_tester_->GetAllSamples(histogram_name).size();
    if (static_cast<uint64_t>(size) == histogram_size) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure()
             << histogram_name << " expected " << size << " entries, but had "
             << histogram_size;
    }
  }

  bool HasPredictionHistograms() {
    uint64_t histogram_size =
        histogram_tester_
            ->GetAllSamples("Event.InputEventPrediction.Scroll.WrongDirection")
            .size();
    return histogram_size > 0u;
  }

  void Reset() {
    histogram_tester_.reset(new base::HistogramTester());
    metrics_handler_->Reset();
  }

  const base::HistogramTester& histogram_tester() { return *histogram_tester_; }

 protected:
  std::unique_ptr<PredictionMetricsHandler> metrics_handler_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PredictionMetricsHandlerTest, CanComputeMetricsTest) {
  base::TimeTicks start_time = EventTimeForNow();
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(8);

  // Need at least 2 real events to start comput metrics.
  {
    metrics_handler_->AddRealEvent(gfx::PointF(0, 0), start_time + 3 * dt,
                                   start_time);
    metrics_handler_->AddPredictedEvent(gfx::PointF(0, 0), start_time + 3 * dt,
                                        start_time);
    metrics_handler_->EvaluatePrediction();
    EXPECT_FALSE(HasPredictionHistograms());
  }

  // Need at least a real event strictly after the second predicted event.
  Reset();
  {
    metrics_handler_->AddRealEvent(gfx::PointF(0, 0), start_time, start_time);
    metrics_handler_->AddRealEvent(gfx::PointF(0, 0), start_time + dt,
                                   start_time);
    metrics_handler_->AddPredictedEvent(gfx::PointF(0, 0), start_time + 2 * dt,
                                        start_time);
    metrics_handler_->AddPredictedEvent(gfx::PointF(0, 0), start_time + 3 * dt,
                                        start_time);
    metrics_handler_->EvaluatePrediction();
    EXPECT_FALSE(HasPredictionHistograms());

    metrics_handler_->AddRealEvent(gfx::PointF(0, 0), start_time + 3 * dt,
                                   start_time);
    metrics_handler_->EvaluatePrediction();
    EXPECT_FALSE(HasPredictionHistograms());

    metrics_handler_->AddRealEvent(gfx::PointF(0, 0), start_time + 3.1 * dt,
                                   start_time);
    metrics_handler_->EvaluatePrediction();
    EXPECT_TRUE(HasPredictionHistograms());
  }
}

TEST_F(PredictionMetricsHandlerTest, InterpolationTest) {
  base::TimeTicks start_time = EventTimeForNow();
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(8);
  gfx::PointF interpolated;

  metrics_handler_->AddRealEvent(gfx::PointF(2, 2), start_time + 1 * dt,
                                 start_time);
  metrics_handler_->AddRealEvent(gfx::PointF(3, 3), start_time + 2 * dt,
                                 start_time);
  metrics_handler_->AddRealEvent(gfx::PointF(5, 5), start_time + 3 * dt,
                                 start_time);
  metrics_handler_->AddRealEvent(gfx::PointF(8, 8), start_time + 4 * dt,
                                 start_time);

  EXPECT_EQ(0, GetInterpolatedEventForPredictedEvent(start_time + 1.5 * dt,
                                                     &interpolated));
  EXPECT_EQ(interpolated, gfx::PointF(2.5, 2.5));

  EXPECT_EQ(2, GetInterpolatedEventForPredictedEvent(start_time + 3.5 * dt,
                                                     &interpolated));
  EXPECT_EQ(interpolated, gfx::PointF(6.5, 6.5));
}
// For test purpose and simplify, we are predicted in the middle of 2 real
// events, which is also the frame time (i.e. a prediction of 4 ms)
void AddEvents(PredictionMetricsHandler* metrics_handler) {
  metrics_handler->AddRealEvent(gfx::PointF(1, 1),
                                MillisecondsToTestTimeTicks(8),
                                MillisecondsToTestTimeTicks(12));  // R0
  metrics_handler->AddRealEvent(gfx::PointF(2, 2),
                                MillisecondsToTestTimeTicks(16),
                                MillisecondsToTestTimeTicks(20));  // R1
  metrics_handler->AddRealEvent(gfx::PointF(4, 4),
                                MillisecondsToTestTimeTicks(24),
                                MillisecondsToTestTimeTicks(28));  // R2
  metrics_handler->AddRealEvent(gfx::PointF(7, 7),
                                MillisecondsToTestTimeTicks(32),
                                MillisecondsToTestTimeTicks(36));  // R3
  metrics_handler->AddRealEvent(gfx::PointF(5, 5),
                                MillisecondsToTestTimeTicks(40),
                                MillisecondsToTestTimeTicks(44));  // R4
  metrics_handler->AddRealEvent(gfx::PointF(3, 3),
                                MillisecondsToTestTimeTicks(48),
                                MillisecondsToTestTimeTicks(54));  // R5

  // P0 | Interpolation from R0-R1 is (1.5,1.5)
  // UnderPrediction
  metrics_handler->AddPredictedEvent(gfx::PointF(1, 1),
                                     MillisecondsToTestTimeTicks(12),
                                     MillisecondsToTestTimeTicks(12));
  // P1 | Interpolation from R1-R2 is (3,3)
  // OverPrediction | RightDirection
  metrics_handler->AddPredictedEvent(gfx::PointF(3.5, 3.5),
                                     MillisecondsToTestTimeTicks(20),
                                     MillisecondsToTestTimeTicks(20));
  // P2 | Interpolation from R2-R3 is (5.5,5.5)
  // UnderPrediction | RightDirection
  metrics_handler->AddPredictedEvent(gfx::PointF(5, 5),
                                     MillisecondsToTestTimeTicks(28),
                                     MillisecondsToTestTimeTicks(28));
  // P3 | Interpolation from R3-R4 is (6,6)
  // UnderPrediction | WrongDirection
  metrics_handler->AddPredictedEvent(gfx::PointF(7, 7),
                                     MillisecondsToTestTimeTicks(36),
                                     MillisecondsToTestTimeTicks(36));
  // P4 | Interpolation from R4-R5 is (4,4)
  // OverPrediction | RightDirection
  metrics_handler->AddPredictedEvent(gfx::PointF(3, 3),
                                     MillisecondsToTestTimeTicks(44),
                                     MillisecondsToTestTimeTicks(44));
}

TEST_F(PredictionMetricsHandlerTest, PredictionMetricTest) {
  AddEvents(metrics_handler_.get());
  metrics_handler_->EvaluatePrediction();

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.InputEventPrediction.Scroll.OverPrediction"),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.InputEventPrediction.Scroll.UnderPrediction"),
              ElementsAre(Bucket(0, 2), Bucket(1, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.InputEventPrediction.Scroll.WrongDirection"),
              ElementsAre(Bucket(0, 3), Bucket(1, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.InputEventPrediction.Scroll.PredictionJitter"),
              ElementsAre(Bucket(1, 2), Bucket(2, 2)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.InputEventPrediction.Scroll.VisualJitter"),
              ElementsAre(Bucket(1, 2), Bucket(2, 2)));
}

}  // namespace test
}  // namespace ui
