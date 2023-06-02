// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

class MockNeuralModel : public NeuralStylusPalmDetectionFilterModel {
 public:
  MOCK_CONST_METHOD1(Inference, float(const std::vector<float>& features));
  MOCK_CONST_METHOD0(config,
                     const NeuralStylusPalmDetectionFilterModelConfig&());
};

class NeuralStylusPalmDetectionFilterTest
    : public testing::TestWithParam<float> {
 public:
  NeuralStylusPalmDetectionFilterTest() = default;

  NeuralStylusPalmDetectionFilterTest(
      const NeuralStylusPalmDetectionFilterTest&) = delete;
  NeuralStylusPalmDetectionFilterTest& operator=(
      const NeuralStylusPalmDetectionFilterTest&) = delete;

  void SetUp() override {
    shared_palm_state = std::make_unique<SharedPalmDetectionFilterState>();
    auto neural_model =
        std::make_unique<testing::StrictMock<MockNeuralModel>>();
    model_ = neural_model.get();
    model_config_.biggest_near_neighbor_count = 2;
    model_config_.min_sample_count = 2;
    model_config_.max_sample_count = 5;
    model_config_.max_neighbor_distance_in_mm = 20;
    model_config_.heuristic_palm_touch_limit = 40;
    model_config_.heuristic_palm_area_limit = 1000;
    model_config_.max_dead_neighbor_time = base::Milliseconds(100);
    const float resample_period = GetParam();
    if (resample_period != 0.0) {
      model_config_.resample_period = base::Milliseconds(resample_period);
      sample_period_ = *model_config_.resample_period;
    }
    EXPECT_CALL(*model_, config())
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::ReturnRef(model_config_));
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneTouchScreen, &nocturne_touchscreen_));
    palm_detection_filter_ = std::make_unique<NeuralStylusPalmDetectionFilter>(
        nocturne_touchscreen_, std::move(neural_model),
        shared_palm_state.get());
    touch_.resize(kNumTouchEvdevSlots);
    for (size_t i = 0; i < touch_.size(); ++i) {
      touch_[i].slot = i;
    }
  }

 protected:
  std::vector<InProgressTouchEvdev> touch_;
  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state;
  EventDeviceInfo nocturne_touchscreen_;
  NeuralStylusPalmDetectionFilterModelConfig model_config_;
  std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;
  raw_ptr<MockNeuralModel> model_;  // Owned by the filter.
  base::TimeDelta sample_period_ = base::Milliseconds(8);
};

INSTANTIATE_TEST_SUITE_P(ParametricFilterTest,
                         NeuralStylusPalmDetectionFilterTest,
                         ::testing::Values(0, 8.0),
                         [](const auto& paramInfo) {
                           return paramInfo.param != 0.0 ? "ResamplingEnabled"
                                                         : "ResamplingDisabled";
                         });

TEST_P(NeuralStylusPalmDetectionFilterTest, EventDeviceSimpleTest) {
  EventDeviceInfo devinfo;
  std::vector<std::pair<DeviceCapabilities, bool>> devices = {
      {kNocturneTouchScreen, true},
      {kEveTouchScreen, true},
      {kLinkTouchscreen, true},  // No ABS_MT_TOOL_TYPE
      {kNocturneStylus, false},
      {kKohakuTouchscreen, true},
      // The Wacom Intuos is external.
      {kWacomIntuosPtS_Finger, false}};
  for (const auto& it : devices) {
    EXPECT_TRUE(CapabilitiesToDeviceInfo(it.first, &devinfo));
    EXPECT_EQ(it.second,
              NeuralStylusPalmDetectionFilter::
                  CompatibleWithNeuralStylusPalmDetectionFilter(devinfo))
        << "Failed on " << it.first.name;
    EXPECT_EQ(false, NeuralStylusPalmDetectionFilter::
                         CompatibleWithNeuralStylusPalmDetectionFilter(
                             devinfo, "{\"touch-compatible\": \"false\"}"));
    EXPECT_EQ(false,
              NeuralStylusPalmDetectionFilter::
                  CompatibleWithNeuralStylusPalmDetectionFilter(devinfo, "{}"));
    if (it.second) {
      EXPECT_EQ(true, NeuralStylusPalmDetectionFilter::
                          CompatibleWithNeuralStylusPalmDetectionFilter(
                              devinfo, "{\"touch-compatible\": \"true\"}"));
    }
  }
}

TEST(NeuralStylusPalmDetectionFilterDeathTest, EventDeviceConstructionDeath) {
  EventDeviceInfo bad_devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneStylus, &bad_devinfo));
  std::unique_ptr<NeuralStylusPalmDetectionFilterModel> model_(
      new testing::StrictMock<MockNeuralModel>);
  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state =
      std::make_unique<SharedPalmDetectionFilterState>();
  EXPECT_DCHECK_DEATH({
    NeuralStylusPalmDetectionFilter f(bad_devinfo, std::move(model_),
                                      shared_palm_state.get());
  });
}

TEST_P(NeuralStylusPalmDetectionFilterTest, NameTest) {
  EXPECT_EQ("NeuralStylusPalmDetectionFilter",
            palm_detection_filter_->FilterNameForTesting());
}

TEST_P(NeuralStylusPalmDetectionFilterTest, ShortTouchPalmAreaTest) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled,
      expected_cancelled;
  touch_[0].touching = true;
  touch_[0].tracking_id = 600;
  touch_[0].x = 50;
  touch_[0].y = 55;
  touch_[0].major = 34;  // 34 * 32 = 1088
  touch_[0].minor = 32;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());
  // end it
  touch_[0].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;
  touch_time += base::Milliseconds(8.0f);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  expected_cancelled.set(0, true);
  EXPECT_EQ(expected_cancelled, actual_cancelled);
}

TEST_P(NeuralStylusPalmDetectionFilterTest, ShortTouchPalmSizeTest) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  touch_[0].touching = true;
  touch_[0].tracking_id = 600;
  touch_[0].x = 50;
  touch_[0].y = 55;
  touch_[0].major = 25;
  touch_[0].minor = 17;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());
  // end it
  touch_[0].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());

  touch_time += base::Seconds(3600);
  touch_[0].touching = true;
  touch_[0].major = 57;
  touch_[0].tracking_id = 601;
  touch_[0].was_touching = false;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());
  touch_[0].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.test(0));
  actual_cancelled.set(0, false);
  EXPECT_TRUE(actual_cancelled.none());
}

TEST_P(NeuralStylusPalmDetectionFilterTest, CallFilterTest) {
  // Set up 3 touches as touching:
  // Touch 0 starts off and is sent twice
  // Touch 1 and 2 are then added on: 2 is far away, 1 is nearby. We expect a
  // single call to filter, with only 1 neighbor!
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  std::bitset<kNumTouchEvdevSlots> expected_cancelled;

  model_config_.max_blank_time = base::Milliseconds(100);
  model_config_.use_tracking_id_count = true;
  model_config_.use_active_tracking_id_count = true;

  touch_[0].touching = true;
  touch_[0].tracking_id = 500;
  touch_[0].major = 15;
  touch_[0].minor = 10;
  touch_[0].x = 15;
  touch_[0].y = 10;
  touch_[0].slot = 0;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());

  // And now, let's add touches 1 and 2.
  touch_[0].x = 17;
  touch_[0].major = 14;
  touch_[0].was_touching = true;

  touch_[1].touching = true;
  touch_[1].major = 11;
  touch_[1].minor = 9;
  touch_[1].x = 30;
  touch_[1].y = 25;
  touch_[1].tracking_id = 501;
  touch_[1].slot = 1;

  touch_[2].touching = true;
  touch_[2].major = 10;
  touch_[2].minor = 8;
  touch_[2].x = 5500;
  touch_[2].y = 2942;
  touch_[2].tracking_id = 502;
  touch_[2].slot = 2;

  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());

  touch_[3] = touch_[2];
  touch_[3].slot = 3;
  touch_[3].x = 8000;
  touch_[3].tracking_id = 504;
  touch_[1].was_touching = true;
  touch_[2].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;
  std::vector<float> features = {
      15, 10, 0, 0.25,  1, 14, 10, 0.05, 0.25,  1, 0,   0,    0, 0, 0,
      0,  0,  0, 0,     0, 0,  0,  0,    0,     0, 0.4, 0.05, 0, 1, 0.512957,
      11, 9,  0, 0.625, 1, 11, 9,  0,    0.625, 1, 0,   0,    0, 0, 0,
      0,  0,  0, 0,     0, 0,  0,  0,    0,     0, 0.4, 0,    0, 0, 0,
      0,  0,  0, 0,     0, 0,  0,  0,    0,     0, 0,   0,    0, 0, 0,
      0,  0,  0, 0,     0, 0,  0,  0,    0,     0, 0,   0,    0, 4, 4};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  expected_cancelled.set(0, true);
  EXPECT_EQ(actual_cancelled, expected_cancelled);

  // Touch 0 already ended in last report, now we mark touch 2 ended, its last
  // two features should be 4 and 3 (tracking_id_count and
  // active_tracking_id_count).
  touch_[0].was_touching = false;
  touch_[2].tracking_id = -1;
  touch_[3].was_touching = true;
  features = {10, 8, 0, 73.55, 1, 10, 8, 0,   73.55, 1, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0.4, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 4, 3};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  expected_cancelled.set(2, true);
  EXPECT_EQ(actual_cancelled, expected_cancelled);
}

TEST_P(NeuralStylusPalmDetectionFilterTest, CallFilterTestWithAdaptiveHold) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  std::bitset<kNumTouchEvdevSlots> expected_held, expected_cancelled;

  // Enable early stage predictions to support adaptive hold.
  model_config_.nn_delay_start_if_palm = true;
  model_config_.early_stage_sample_counts = {2};

  // Only one touch in slot 0, nothing happens.
  touch_[0].touching = true;
  touch_[0].tracking_id = 500;
  touch_[0].major = 15;
  touch_[0].minor = 10;
  touch_[0].x = 15;
  touch_[0].y = 10;
  touch_[0].slot = 0;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());

  // And now, let's add touches 1 and 2.
  touch_[0].x = 17;
  touch_[0].major = 14;
  touch_[0].was_touching = true;

  touch_[1].touching = true;
  touch_[1].major = 11;
  touch_[1].minor = 9;
  touch_[1].x = 30;
  touch_[1].y = 25;
  touch_[1].tracking_id = 501;
  touch_[1].slot = 1;

  touch_[2].touching = true;
  touch_[2].major = 10;
  touch_[2].minor = 8;
  touch_[2].x = 5500;
  touch_[2].y = 2942;
  touch_[2].tracking_id = 502;
  touch_[2].slot = 2;

  // Slot 0 now has 2 reports, ready for an early stage prediction.
  std::vector<float> features = {
      15, 10, 0, 0.25, 1, 14, 10, 0.05, 0.25, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0, 0,    0, 0,  0,  0.4,  0.05, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0, 0,    0, 0,  0,  0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0, 0,    0, 0,  0,  0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,  0,  0, 0,    0, 0,  0,  0,    0,    0, 0, 0, 0, 0, 0, 0};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  // Slot 0 is held.
  expected_held.set(0, true);
  EXPECT_EQ(actual_held, expected_held);
  EXPECT_TRUE(actual_cancelled.none());

  // Slot 1 and 2 have 2 reports now, do early stage prediction on them.
  // Slot 0 ends and have 3 reports (more than min_sample_count), do final stage
  // prediction on it.
  touch_[3] = touch_[2];
  touch_[3].slot = 3;
  touch_[3].x = 8000;
  touch_[3].tracking_id = 504;
  touch_[1].was_touching = true;
  touch_[2].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;
  // Early stage for slot 1.
  features = {11, 9, 0, 0.625,    1,    11, 9, 0,    0.625, 1,  0,  0,    0,
              0,  0, 0, 0,        0,    0,  0, 0,    0,     0,  0,  0,    0.4,
              0,  0, 1, 0.512957, 15,   10, 0, 0.25, 1,     14, 10, 0.05, 0.25,
              1,  0, 0, 0,        0,    0,  0, 0,    0,     0,  0,  0,    0,
              0,  0, 0, 0.4,      0.05, 0,  0, 0,    0,     0,  0,  0,    0,
              0,  0, 0, 0,        0,    0,  0, 0,    0,     0,  0,  0,    0,
              0,  0, 0, 0,        0,    0,  0, 0,    0,     0};

  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));
  // Early stage for slot 2.
  features = {10, 8, 0, 73.55, 1, 10, 8, 0,   73.55, 1, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0.4, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));

  // Final stage for slot 0.
  features = {15,   10, 0, 0.25,     1,  14, 10, 0.05,  0.25, 1,  0, 0, 0,
              0,    0,  0, 0,        0,  0,  0,  0,     0,    0,  0, 0, 0.4,
              0.05, 0,  1, 0.512957, 11, 9,  0,  0.625, 1,    11, 9, 0, 0.625,
              1,    0,  0, 0,        0,  0,  0,  0,     0,    0,  0, 0, 0,
              0,    0,  0, 0.4,      0,  0,  0,  0,     0,    0,  0, 0, 0,
              0,    0,  0, 0,        0,  0,  0,  0,     0,    0,  0, 0, 0,
              0,    0,  0, 0,        0,  0,  0,  0,     0,    0};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));

  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);

  // Slot 1 and 2 are held, slot 0 is cancelled.
  expected_held.reset();
  expected_held.set(1, true);
  expected_held.set(2, true);
  EXPECT_EQ(actual_held, expected_held);

  expected_cancelled.set(0, true);
  EXPECT_EQ(actual_cancelled, expected_cancelled);

  // At this update, slot 3 has 2 reports, do an early prediction on it.
  // Slot 2 ends and have 3 reports, do final prediction.
  touch_[0].was_touching = false;
  touch_[2].tracking_id = -1;
  touch_[3].was_touching = true;
  // Early stage for slot 3.
  features = {10, 8, 0, 60.1, 1, 10, 8, 0,   60.1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,    0, 0,  0, 0.4, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,    0, 0,  0, 0,   0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,    0, 0,  0, 0,   0,    0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,    0, 0,  0, 0,   0,    0, 0, 0, 0, 0, 0, 0};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));

  // Final stage for slot 2.
  features = {10, 8, 0, 73.55, 1, 10, 8, 0,   73.55, 1, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0.4, 0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0, 0, 0,
              0,  0, 0, 0,     0, 0,  0, 0,   0,     0, 0, 0, 0, 0, 0, 0};
  EXPECT_CALL(*model_,
              Inference(testing::Pointwise(testing::FloatEq(), features)))
      .Times(1)
      .WillOnce(testing::Return(0.5));
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);

  // Slot 1 was held and at this update it neither ends nor reaches
  // max_sample_count, so it keeps held. Slot 3 is newly held.
  expected_held.reset();
  expected_held.set(1, true);
  expected_held.set(3, true);
  EXPECT_EQ(actual_held, expected_held);

  // Slot 0 was cancelled and no new touch comes to this slot, it keeps
  // cancelled. Slot 2 is newly cancelled.
  expected_cancelled.reset();
  expected_cancelled.set(0, true);
  expected_cancelled.set(2, true);
  EXPECT_EQ(actual_cancelled, expected_cancelled);
}

TEST_P(NeuralStylusPalmDetectionFilterTest, InferenceOnceNotPalm) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);

  touch_[0].touching = true;
  touch_[0].tracking_id = 600;
  touch_[0].x = 50;
  touch_[0].y = 55;
  touch_[0].major = 25;
  touch_[0].minor = 17;
  EXPECT_CALL(*model_, Inference(testing::_))
      .Times(1)
      .WillOnce(testing::Return(-0.5));
  for (int i = 0; i < 5000; ++i) {
    if (i != 0) {
      touch_[0].was_touching = true;
    }
    touch_time += sample_period_;
    palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                   &actual_cancelled);
    ASSERT_TRUE(actual_held.none()) << " Failed at " << i;
    ASSERT_TRUE(actual_cancelled.none()) << " Failed at " << i;
  }
}

TEST_P(NeuralStylusPalmDetectionFilterTest, InferenceOncePalm) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  std::bitset<kNumTouchEvdevSlots> expected_cancelled;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  expected_cancelled.set(0, true);
  touch_[0].touching = true;
  touch_[0].tracking_id = 600;
  touch_[0].x = 50;
  touch_[0].y = 55;
  touch_[0].major = 25;
  touch_[0].minor = 17;
  EXPECT_CALL(*model_, Inference(testing::_))
      .Times(1)
      .WillOnce(testing::Return(0.5));

  for (size_t i = 0; i < 5000; ++i) {
    if (i != 0) {
      touch_[0].was_touching = true;
    }
    touch_time += sample_period_;
    palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                   &actual_cancelled);

    ASSERT_TRUE(actual_held.none()) << " Failed at " << i;
    if (i >= model_config_.max_sample_count) {
      ASSERT_EQ(expected_cancelled, actual_cancelled) << " Failed at " << i;
    }
  }
}

TEST_P(NeuralStylusPalmDetectionFilterTest, DelayShortFingerTouch) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  std::bitset<kNumTouchEvdevSlots> expected_held, expected_cancelled;
  model_config_.heuristic_delay_start_if_palm = true;
  touch_[0].touching = true;
  touch_[0].tracking_id = 605;
  touch_[0].x = 50;
  touch_[0].y = 55;
  // small touch! 39*21 = 819, which is < 1000.
  touch_[0].major = 39;
  touch_[0].minor = 21;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);

  EXPECT_EQ(expected_held, actual_held);
  EXPECT_EQ(expected_cancelled, actual_cancelled);
}

TEST_P(NeuralStylusPalmDetectionFilterTest, DelayShortPalmTouch) {
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;
  std::bitset<kNumTouchEvdevSlots> expected_held, expected_cancelled;
  model_config_.heuristic_delay_start_if_palm = true;
  touch_[0].touching = true;
  touch_[0].tracking_id = 605;
  touch_[0].x = 50;
  touch_[0].y = 55;
  // big touch! 39*30 = 1170, which is > 1000.
  touch_[0].major = 39;
  touch_[0].minor = 30;
  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);

  expected_held.set(0, true);
  EXPECT_EQ(expected_held, actual_held);
  EXPECT_EQ(expected_cancelled, actual_cancelled);

  // Delay continues even afterwards, until inference time: then it's off.
  for (uint32_t i = 1; i < model_config_.max_sample_count - 1; ++i) {
    touch_[0].was_touching = true;
    touch_time += sample_period_;
    touch_[0].major = 15;
    touch_[0].minor = 15;
    touch_[0].x += 1;
    touch_[0].y += 1;
    palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                   &actual_cancelled);
    EXPECT_EQ(expected_held, actual_held) << " failed at " << i;
    EXPECT_EQ(expected_cancelled, actual_cancelled) << " failed at " << i;
  }
  // When running inference, turn delay to false.
  EXPECT_CALL(*model_, Inference(testing::_))
      .Times(1)
      .WillOnce(testing::Return(-0.1));
  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  expected_held.set(0, false);
  EXPECT_EQ(expected_held, actual_held);
  EXPECT_EQ(expected_cancelled, actual_cancelled);
}

}  // namespace ui
