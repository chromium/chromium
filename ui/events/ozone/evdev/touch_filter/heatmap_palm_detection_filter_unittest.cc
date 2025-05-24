// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/heatmap_palm_detection_filter.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

using ::testing::ElementsAre;

namespace ui {

class HeatmapPalmDetectionFilterStrokeTest : public testing::Test {
 public:
  HeatmapPalmDetectionFilterStrokeTest() = default;

  HeatmapPalmDetectionFilterStrokeTest(
      const HeatmapPalmDetectionFilterStrokeTest&) = delete;
  HeatmapPalmDetectionFilterStrokeTest& operator=(
      const HeatmapPalmDetectionFilterStrokeTest&) = delete;

  void SetUp() override { model_config_.max_sample_count = 3; }

 protected:
  InProgressTouchEvdev touch_;
  HeatmapPalmDetectionFilterModelConfig model_config_;
  int tracking_id_ = 0;
};

TEST_F(HeatmapPalmDetectionFilterStrokeTest,
       HeatmapPalmFilterStrokeSampleSizeTest) {
  HeatmapPalmFilterStroke stroke(model_config_, tracking_id_);
  touch_.tracking_id = tracking_id_;
  base::TimeTicks time = base::TimeTicks() + base::Seconds(30);

  for (int i = 0; i < 10; ++i) {
    HeatmapPalmFilterSample sample = {touch_.tracking_id, time};
    EXPECT_EQ(static_cast<uint64_t>(i), stroke.samples_seen());
    stroke.ProcessSample(sample);
    ASSERT_EQ(std::min(3ul, 1ul + i), stroke.samples().size());

    time += base::Milliseconds(8);
  }
}

TEST_F(HeatmapPalmDetectionFilterStrokeTest, SampleTest) {
  HeatmapPalmFilterStroke stroke(model_config_, tracking_id_);

  // Add first sample at time = T
  touch_.tracking_id = tracking_id_;
  base::TimeTicks down_time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  HeatmapPalmFilterSample sample1 = {touch_.tracking_id, down_time};
  stroke.ProcessSample(sample1);
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1));

  // Add second sample at time = T + 2ms. All samples are stored.
  base::TimeTicks time = down_time + base::Milliseconds(4);
  HeatmapPalmFilterSample sample2 = {touch_.tracking_id, time};
  stroke.ProcessSample(sample2);
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1, sample2));

  // Add third sample at time = T + 12ms.
  time = down_time + base::Milliseconds(12);
  HeatmapPalmFilterSample sample3 = {touch_.tracking_id, time};
  stroke.ProcessSample(sample3);
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1, sample2, sample3));
}

class HeatmapPalmDetectionFilterTest : public testing::Test {
 public:
  HeatmapPalmDetectionFilterTest() = default;

  HeatmapPalmDetectionFilterTest(const HeatmapPalmDetectionFilterTest&) =
      delete;
  HeatmapPalmDetectionFilterTest& operator=(
      const HeatmapPalmDetectionFilterTest&) = delete;

  void SetUp() override {
    shared_palm_state_ = std::make_unique<SharedPalmDetectionFilterState>();
    auto model_config =
        std::make_unique<HeatmapPalmDetectionFilterModelConfig>();
    model_config->max_sample_count = 2;
    EventDeviceInfo rex_touchscreen;
    ASSERT_TRUE(
        CapabilitiesToDeviceInfo(kRexHeatmapTouchScreen, &rex_touchscreen));
    palm_detection_filter_ = std::make_unique<HeatmapPalmDetectionFilter>(
        rex_touchscreen, std::move(model_config), shared_palm_state_.get());
    touch_.resize(kNumTouchEvdevSlots);
    for (size_t i = 0; i < touch_.size(); ++i) {
      touch_[i].slot = i;
    }
  }

 protected:
  std::vector<InProgressTouchEvdev> touch_;
  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state_;
  std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;
  base::TimeDelta sample_period_ = base::Milliseconds(8);
};

TEST_F(HeatmapPalmDetectionFilterTest, EventDeviceCompatibilityTest) {
  EventDeviceInfo devinfo;
  std::vector<std::pair<DeviceCapabilities, bool>> devices = {
      {kRexHeatmapTouchScreen, true}, {kEveTouchScreen, false}};

  for (const auto& it : devices) {
    ASSERT_TRUE(CapabilitiesToDeviceInfo(it.first, &devinfo));
    EXPECT_EQ(
        it.second,
        HeatmapPalmDetectionFilter::CompatibleWithHeatmapPalmDetectionFilter(
            devinfo))
        << "Failed on " << it.first.name;
  }
}

TEST(HeatmapPalmDetectionFilterDeathTest, EventDeviceConstructionDeath) {
  EventDeviceInfo bad_devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kEveTouchScreen, &bad_devinfo));
  auto model_config = std::make_unique<HeatmapPalmDetectionFilterModelConfig>();
  auto shared_palm_state = std::make_unique<SharedPalmDetectionFilterState>();
  EXPECT_DCHECK_DEATH({
    HeatmapPalmDetectionFilter f(bad_devinfo, std::move(model_config),
                                 shared_palm_state.get());
  });
}

TEST_F(HeatmapPalmDetectionFilterTest, NameTest) {
  EXPECT_EQ("HeatmapPalmDetectionFilter",
            palm_detection_filter_->FilterNameForTesting());
}

TEST_F(HeatmapPalmDetectionFilterTest, CallFilterTest) {
  // Set up 2 touches as touching:
  // Touch 0 starts off and is sent twice and followed by touch 1.
  std::bitset<kNumTouchEvdevSlots> actual_held, actual_cancelled;

  // Start with touch 0.
  touch_[0].touching = true;
  touch_[0].tracking_id = 500;
  touch_[0].slot = 0;

  base::TimeTicks touch_time =
      base::TimeTicks::UnixEpoch() + base::Milliseconds(10.0);
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(actual_held.none());
  EXPECT_TRUE(actual_cancelled.none());

  // Now add touch 1.
  touch_[0].was_touching = true;
  touch_[1].touching = true;
  touch_[1].tracking_id = 501;
  touch_[1].slot = 1;

  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  auto* heatmap_palm_detection_filter =
      static_cast<HeatmapPalmDetectionFilter*>(palm_detection_filter_.get());
  EXPECT_FALSE(heatmap_palm_detection_filter->ShouldRunModel(500));
  EXPECT_TRUE(heatmap_palm_detection_filter->ShouldRunModel(501));

  // Touch 0 ended.
  touch_[1].was_touching = true;
  touch_[0].touching = false;
  touch_[0].tracking_id = -1;

  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(heatmap_palm_detection_filter->ShouldRunModel(500));
  EXPECT_FALSE(heatmap_palm_detection_filter->ShouldRunModel(501));

  // Mark touch 1 ended.
  touch_[1].touching = false;
  touch_[1].tracking_id = -1;

  touch_time += sample_period_;
  palm_detection_filter_->Filter(touch_, touch_time, &actual_held,
                                 &actual_cancelled);
  EXPECT_TRUE(heatmap_palm_detection_filter->ShouldRunModel(500));
  EXPECT_TRUE(heatmap_palm_detection_filter->ShouldRunModel(501));
}

}  // namespace ui
