// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_report_filter.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

class NeuralStylusReportFilterTest : public testing::Test {
 public:
  NeuralStylusReportFilterTest() = default;

  NeuralStylusReportFilterTest(const NeuralStylusReportFilterTest&) = delete;
  NeuralStylusReportFilterTest& operator=(const NeuralStylusReportFilterTest&) =
      delete;

  void SetUp() override {
    shared_palm_state = std::make_unique<SharedPalmDetectionFilterState>();
    palm_detection_filter_ =
        std::make_unique<NeuralStylusReportFilter>(shared_palm_state.get());
    EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneTouchScreen,
                                         &nocturne_touchscreen_info_));
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneStylus, &nocturne_stylus_info_));
  }

 protected:
  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state;
  std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;
  EventDeviceInfo nocturne_touchscreen_info_, nocturne_stylus_info_;
};

TEST_F(NeuralStylusReportFilterTest, TestSetsToZero) {
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  suppress.set(kNumTouchEvdevSlots - 1, true);
  hold.set(0, true);
  std::vector<InProgressTouchEvdev> inputs;
  inputs.resize(kNumTouchEvdevSlots);
  palm_detection_filter_->Filter(inputs, base::TimeTicks::Now(), &hold,
                                 &suppress);
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());
}

TEST_F(NeuralStylusReportFilterTest, CompatibleWithStylus) {
  EXPECT_FALSE(NeuralStylusReportFilter::CompatibleWithNeuralStylusReportFilter(
      nocturne_touchscreen_info_));
  EXPECT_TRUE(NeuralStylusReportFilter::CompatibleWithNeuralStylusReportFilter(
      nocturne_stylus_info_));
}

TEST_F(NeuralStylusReportFilterTest, TestNoUpdatesWhenNotTouching) {
  base::HistogramTester histogram_tester;
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  std::vector<InProgressTouchEvdev> inputs;
  inputs.resize(kNumTouchEvdevSlots);

  base::TimeTicks sample_t = base::TimeTicks::Now();

  palm_detection_filter_->Filter(inputs, sample_t, &hold, &suppress);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(NeuralStylusReportFilter::kNeuralPalmAge),
      ::testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  NeuralStylusReportFilter::kNeuralFingerAge),
              ::testing::IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  NeuralStylusReportFilter::kNeuralPalmTouchCount),
              ::testing::IsEmpty());

  shared_palm_state->active_palm_touches = 2;
  shared_palm_state->latest_stylus_touch_time =
      sample_t - base::Milliseconds(10.0);
  shared_palm_state->latest_palm_touch_time =
      sample_t - base::Milliseconds(15.0);
  shared_palm_state->latest_finger_touch_time =
      sample_t - base::Milliseconds(20.0);

  inputs[0].altered = true;
  inputs[0].stylus_button = true;

  // We should now get a bunch of updates.
  palm_detection_filter_->Filter(inputs, sample_t, &hold, &suppress);
  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralPalmAge,
                                    1);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralPalmAge, base::Milliseconds(15.0), 1);
  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralFingerAge,
                                    1);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralFingerAge, base::Milliseconds(20.0), 1);
  histogram_tester.ExpectTotalCount(
      NeuralStylusReportFilter::kNeuralPalmTouchCount, 1);
  histogram_tester.ExpectBucketCount(
      NeuralStylusReportFilter::kNeuralPalmTouchCount, 2, 1);
  // We should now get a bunch of updates.

  // Ensure no more updates.
  palm_detection_filter_->Filter(inputs, sample_t + base::Milliseconds(5.0),
                                 &hold, &suppress);
  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralPalmAge,
                                    1);
  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralFingerAge,
                                    1);
  histogram_tester.ExpectTotalCount(
      NeuralStylusReportFilter::kNeuralPalmTouchCount, 1);

  // Set to 0 again, filter, then set to touching again.
  inputs[0] = inputs[1];
  palm_detection_filter_->Filter(inputs, sample_t + base::Milliseconds(10.0),
                                 &hold, &suppress);

  inputs[0].altered = true;
  inputs[0].stylus_button = true;
  palm_detection_filter_->Filter(inputs, sample_t + base::Milliseconds(15.0),
                                 &hold, &suppress);
  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralPalmAge,
                                    2);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralPalmAge, base::Milliseconds(15.0), 1);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralPalmAge, base::Milliseconds(30.0), 1);

  histogram_tester.ExpectTotalCount(NeuralStylusReportFilter::kNeuralFingerAge,
                                    2);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralFingerAge, base::Milliseconds(20.0), 1);
  histogram_tester.ExpectTimeBucketCount(
      NeuralStylusReportFilter::kNeuralFingerAge, base::Milliseconds(35.0), 1);

  histogram_tester.ExpectTotalCount(
      NeuralStylusReportFilter::kNeuralPalmTouchCount, 2);
  histogram_tester.ExpectBucketCount(
      NeuralStylusReportFilter::kNeuralPalmTouchCount, 2, 2);
}

}  // namespace ui
