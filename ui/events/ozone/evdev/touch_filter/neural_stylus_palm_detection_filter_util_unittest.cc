// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_util.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace ui {

MATCHER_P(SampleTime, time, "Does the sample have given time.") {
  *result_listener << "Sample time" << arg.time << " is not " << time;
  return time == arg.time;
}

class NeuralStylusPalmDetectionFilterUtilTest
    : public testing::TestWithParam<bool> {
 public:
  NeuralStylusPalmDetectionFilterUtilTest() = default;

  NeuralStylusPalmDetectionFilterUtilTest(
      const NeuralStylusPalmDetectionFilterUtilTest&) = delete;
  NeuralStylusPalmDetectionFilterUtilTest& operator=(
      const NeuralStylusPalmDetectionFilterUtilTest&) = delete;

  void SetUp() override {
    EXPECT_TRUE(
        CapabilitiesToDeviceInfo(kNocturneTouchScreen, &nocturne_touchscreen_));
    touch_.major = 25;
    touch_.minor = 24;
    touch_.pressure = 23;
    touch_.tracking_id = 22;
    touch_.x = 21;
    touch_.y = 20;
    model_config_.max_sample_count = 3;
    const bool resample_touch = GetParam();
    if (resample_touch) {
      model_config_.resample_period = base::Milliseconds(8);
    }
  }

 protected:
  InProgressTouchEvdev touch_;
  EventDeviceInfo nocturne_touchscreen_;
  NeuralStylusPalmDetectionFilterModelConfig model_config_;
};

INSTANTIATE_TEST_SUITE_P(ParametricUtilTest,
                         NeuralStylusPalmDetectionFilterUtilTest,
                         ::testing::Bool(),
                         [](const auto& paramInfo) {
                           return paramInfo.param ? "ResamplingEnabled"
                                                  : "ResamplingDisabled";
                         });

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, DistilledNocturneTest) {
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  EXPECT_FLOAT_EQ(nocturne_distilled.max_x,
                  nocturne_touchscreen_.GetAbsMaximum(ABS_MT_POSITION_X));
  EXPECT_FLOAT_EQ(nocturne_distilled.max_y,
                  nocturne_touchscreen_.GetAbsMaximum(ABS_MT_POSITION_Y));
  EXPECT_FLOAT_EQ(nocturne_distilled.x_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_POSITION_X));
  EXPECT_FLOAT_EQ(nocturne_distilled.y_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_POSITION_Y));
  EXPECT_FLOAT_EQ(nocturne_distilled.major_radius_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_TOUCH_MAJOR));
  EXPECT_TRUE(nocturne_distilled.minor_radius_supported);
  EXPECT_FLOAT_EQ(nocturne_distilled.minor_radius_res,
                  nocturne_touchscreen_.GetAbsResolution(ABS_MT_TOUCH_MINOR));
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, NoMinorResTest) {
  // Nocturne has minor resolution, but let's pretend it doesn't. we should
  // recover "1" as the resolution.
  auto abs_info = nocturne_touchscreen_.GetAbsInfoByCode(ABS_MT_TOUCH_MINOR);
  abs_info.resolution = 0;
  nocturne_touchscreen_.SetAbsInfo(ABS_MT_TOUCH_MINOR, abs_info);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  EXPECT_EQ(1, nocturne_distilled.minor_radius_res);
  EXPECT_EQ(1, nocturne_distilled.major_radius_res);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, DistillerKohakuTest) {
  EventDeviceInfo kohaku_touchscreen;
  ASSERT_TRUE(
      CapabilitiesToDeviceInfo(kKohakuTouchscreen, &kohaku_touchscreen));
  const PalmFilterDeviceInfo kohaku_distilled =
      CreatePalmFilterDeviceInfo(kohaku_touchscreen);
  EXPECT_FALSE(kohaku_distilled.minor_radius_supported);
  EXPECT_EQ(1, kohaku_distilled.x_res);
  EXPECT_EQ(1, kohaku_distilled.y_res);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, DistilledLinkTest) {
  EventDeviceInfo link_touchscreen;
  ASSERT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &link_touchscreen));
  const PalmFilterDeviceInfo link_distilled =
      CreatePalmFilterDeviceInfo(link_touchscreen);
  EXPECT_FALSE(link_distilled.minor_radius_supported);
  EXPECT_FLOAT_EQ(1.f, link_distilled.major_radius_res);
  EXPECT_FLOAT_EQ(link_distilled.major_radius_res,
                  link_distilled.minor_radius_res);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, PalmFilterSampleTest) {
  base::TimeTicks time = base::TimeTicks() + base::Seconds(30);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  const PalmFilterSample sample =
      CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
  EXPECT_EQ(time, sample.time);
  EXPECT_EQ(25, sample.major_radius);
  EXPECT_EQ(24, sample.minor_radius);
  EXPECT_EQ(23, sample.pressure);
  EXPECT_EQ(22, sample.tracking_id);
  EXPECT_EQ(gfx::PointF(21 / 40.f, 20 / 40.f), sample.point);
  EXPECT_EQ(0.5, sample.edge);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, LinkTouchscreenSampleTest) {
  EventDeviceInfo link_touchscreen;
  base::TimeTicks time = base::TimeTicks() + base::Seconds(30);
  ASSERT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &link_touchscreen));
  const PalmFilterDeviceInfo link_distilled =
      CreatePalmFilterDeviceInfo(link_touchscreen);
  touch_.minor = 0;  // no minor from link.
  // use 40 as a base since model is trained on that kind of device.
  model_config_.radius_polynomial_resize = {
      link_touchscreen.GetAbsResolution(ABS_MT_POSITION_X) / 40.0f, 0.0};
  const PalmFilterSample sample =
      CreatePalmFilterSample(touch_, time, model_config_, link_distilled);
  EXPECT_FLOAT_EQ(12.5, sample.major_radius);
  EXPECT_FLOAT_EQ(12.5, sample.minor_radius);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, PalmFilterStrokeTest) {
  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 55);
  touch_.tracking_id = 55;
  // With no points, center is 0.
  EXPECT_EQ(gfx::PointF(0., 0.), stroke.GetCentroid());

  base::TimeTicks time = base::TimeTicks() + base::Seconds(30);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  // Deliberately long test to ensure floating point continued accuracy.
  for (int i = 0; i < 500000; ++i) {
    touch_.x = 15 + i;
    PalmFilterSample sample =
        CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
    time += base::Milliseconds(8);
    stroke.ProcessSample(std::move(sample));
    EXPECT_EQ(touch_.tracking_id, stroke.tracking_id());
    if (i < 3) {
      if (i == 0) {
        EXPECT_FLOAT_EQ(gfx::PointF(15 / 40.f, 0.5).x(),
                        stroke.GetCentroid().x());
      } else if (i == 1) {
        EXPECT_FLOAT_EQ(gfx::PointF((30 + 1) / (2 * 40.f), 0.5).x(),
                        stroke.GetCentroid().x());
      } else if (i == 2) {
        EXPECT_FLOAT_EQ(gfx::PointF((45 + 1 + 2) / (3 * 40.f), 0.5).x(),
                        stroke.GetCentroid().x());
      }
      continue;
    }
    float expected_x = (45 + 3 * i - 3) / (3 * 40.f);
    gfx::PointF expected_centroid = gfx::PointF(expected_x, 0.5);
    ASSERT_FLOAT_EQ(expected_centroid.x(), stroke.GetCentroid().x())
        << "failed at i " << i;
  }
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest,
       PalmFilterStrokeBiggestSizeTest) {
  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 0);
  PalmFilterStroke no_minor_stroke(model_config_, /*tracking_id*/ 0);
  touch_.tracking_id = stroke.tracking_id();
  EXPECT_EQ(0, stroke.BiggestSize());

  base::TimeTicks time = base::TimeTicks() + base::Seconds(30);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  for (int i = 0; i < 500; ++i) {
    touch_.major = 2 + i;
    touch_.minor = 1 + i;
    PalmFilterSample sample =
        CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
    EXPECT_EQ(static_cast<uint64_t>(i), stroke.samples_seen());
    stroke.ProcessSample(sample);
    EXPECT_FLOAT_EQ((1 + i) * (2 + i), stroke.BiggestSize());

    PalmFilterSample second_sample =
        CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
    second_sample.minor_radius = 0;
    no_minor_stroke.ProcessSample(std::move(second_sample));
    EXPECT_FLOAT_EQ((2 + i) * (2 + i), no_minor_stroke.BiggestSize());
    ASSERT_EQ(std::min(3ul, 1ul + i), stroke.samples().size());
    time += base::Milliseconds(8);
  }
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, UnscaledMajorMinorResolution) {
  model_config_.radius_polynomial_resize = {};
  PalmFilterDeviceInfo device_info;
  device_info.x_res = 2;
  device_info.y_res = 5;
  device_info.major_radius_res = 2;
  device_info.minor_radius_res = 5;
  device_info.minor_radius_supported = true;
  touch_.major = 20;
  touch_.minor = 10;
  touch_.orientation = 0;
  base::TimeTicks time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  PalmFilterSample sample =
      CreatePalmFilterSample(touch_, time, model_config_, device_info);
  EXPECT_EQ(20 / 2, sample.major_radius);
  EXPECT_EQ(10 / 5, sample.minor_radius);
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, StrokeGetMaxMajorTest) {
  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 0);
  touch_.tracking_id = stroke.tracking_id();
  EXPECT_FLOAT_EQ(0, stroke.MaxMajorRadius());
  base::TimeTicks time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  for (int i = 1; i < 50; ++i) {
    touch_.major = i;
    touch_.minor = i - 1;
    PalmFilterSample sample =
        CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
    time += base::Milliseconds(8);
    EXPECT_EQ(static_cast<uint64_t>(i - 1), stroke.samples_seen());
    stroke.ProcessSample(sample);
    EXPECT_FLOAT_EQ(i, stroke.MaxMajorRadius());
  }
}

TEST_P(NeuralStylusPalmDetectionFilterUtilTest, SampleRadiusConversion) {
  // A single number: a _constant_.
  model_config_.radius_polynomial_resize = {71.3};
  base::TimeTicks time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  const PalmFilterDeviceInfo nocturne_distilled =
      CreatePalmFilterDeviceInfo(nocturne_touchscreen_);
  PalmFilterSample sample =
      CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
  EXPECT_FLOAT_EQ(71.3, sample.major_radius);
  EXPECT_FLOAT_EQ(71.3, sample.minor_radius);

  // 0.1*r^2 + 0.4*r - 5.0
  model_config_.radius_polynomial_resize = {0.1, 0.4, -5.0};
  sample =
      CreatePalmFilterSample(touch_, time, model_config_, nocturne_distilled);
  EXPECT_FLOAT_EQ(0.1 * 25 * 25 + 0.4 * 25 - 5.0, sample.major_radius);
  EXPECT_FLOAT_EQ(0.1 * 24 * 24 + 0.4 * 24 - 5.0, sample.minor_radius);
}

TEST(PalmFilterStrokeTest, NumberOfResampledValues) {
  NeuralStylusPalmDetectionFilterModelConfig model_config_;
  model_config_.max_sample_count = 3;
  model_config_.resample_period = base::Milliseconds(8);
  base::TimeTicks down_time = base::TimeTicks::UnixEpoch() + base::Seconds(30);

  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 0);
  const PalmFilterDeviceInfo device_info;

  // Initially, no samples
  ASSERT_THAT(stroke.samples(), IsEmpty());
  ASSERT_EQ(0u, stroke.samples_seen());

  // Add first sample at time = T
  InProgressTouchEvdev touch_;
  touch_.tracking_id = stroke.tracking_id();
  PalmFilterSample sample =
      CreatePalmFilterSample(touch_, down_time, model_config_, device_info);
  stroke.ProcessSample(sample);
  ASSERT_THAT(stroke.samples(), ElementsAre(SampleTime(down_time)));
  ASSERT_EQ(1u, stroke.samples_seen());

  // Add second sample at time = T + 4ms. All samples are stored, even if it's
  // before the next resample time.
  base::TimeTicks time = down_time + base::Milliseconds(4);
  sample = CreatePalmFilterSample(touch_, time, model_config_, device_info);
  stroke.ProcessSample(sample);
  ASSERT_THAT(stroke.samples(),
              ElementsAre(SampleTime(down_time), SampleTime(time)));
  ASSERT_EQ(2u, stroke.samples_seen());

  // Add third sample at time = T + 10ms.
  time = down_time + base::Milliseconds(10);
  sample = CreatePalmFilterSample(touch_, time, model_config_, device_info);
  stroke.ProcessSample(sample);
  ASSERT_THAT(stroke.samples(),
              ElementsAre(SampleTime(down_time),
                          SampleTime(down_time + base::Milliseconds(4)),
                          SampleTime(down_time + base::Milliseconds(10))));
  ASSERT_EQ(3u, stroke.samples_seen());
}

TEST(PalmFilterStrokeTest, ResamplingTest) {
  NeuralStylusPalmDetectionFilterModelConfig model_config_;
  model_config_.max_sample_count = 3;
  model_config_.resample_period = base::Milliseconds(8);

  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 0);
  PalmFilterDeviceInfo device_info;
  device_info.minor_radius_supported = true;

  // Add first sample at time = T
  InProgressTouchEvdev touch_;
  touch_.tracking_id = stroke.tracking_id();
  touch_.x = 1;
  touch_.y = 2;
  touch_.major = 4;
  touch_.minor = 3;
  base::TimeTicks down_time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  PalmFilterSample sample1 =
      CreatePalmFilterSample(touch_, down_time, model_config_, device_info);
  stroke.ProcessSample(sample1);
  // First sample should not be modified
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1));

  // Add second sample at time = T + 2ms. It's not yet time for the new frame,
  // so no new sample should be generated.
  base::TimeTicks time = down_time + base::Milliseconds(4);
  touch_.x = 100;
  touch_.y = 20;
  touch_.major = 12;
  touch_.minor = 11;
  PalmFilterSample sample2 =
      CreatePalmFilterSample(touch_, time, model_config_, device_info);
  stroke.ProcessSample(sample2);
  // The samples should remain unchanged
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1, sample2));

  // Add third sample at time = T + 12ms. A resampled event at time = T + 8ms
  // should be generated.
  time = down_time + base::Milliseconds(12);
  touch_.x = 200;
  touch_.y = 24;
  touch_.major = 14;
  touch_.minor = 13;
  PalmFilterSample sample3 =
      CreatePalmFilterSample(touch_, time, model_config_, device_info);
  stroke.ProcessSample(sample3);
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1, sample2, sample3));
}

/**
 * There should always be at least (max_sample_count - 1) * resample_period
 * worth of samples. However, that's not sufficient. In the cases where the gap
 * between samples is very large (larger than the sample horizon), there needs
 * to be another sample in order to calculate resampled values throughout the
 * entire range.
 */
TEST(PalmFilterStrokeTest, MultipleResampledValues) {
  NeuralStylusPalmDetectionFilterModelConfig model_config_;
  model_config_.max_sample_count = 3;
  model_config_.resample_period = base::Milliseconds(8);

  PalmFilterStroke stroke(model_config_, /*tracking_id*/ 0);
  PalmFilterDeviceInfo device_info;
  device_info.minor_radius_supported = true;

  // Add first sample at time = T
  InProgressTouchEvdev touch_;
  touch_.tracking_id = stroke.tracking_id();
  touch_.x = 0;
  touch_.y = 10;
  touch_.major = 200;
  touch_.minor = 100;
  base::TimeTicks down_time = base::TimeTicks::UnixEpoch() + base::Seconds(30);
  PalmFilterSample sample1 =
      CreatePalmFilterSample(touch_, down_time, model_config_, device_info);
  stroke.ProcessSample(sample1);
  // First sample should go in as is
  ASSERT_THAT(stroke.samples(), ElementsAre(sample1));

  // Add second sample at time = T + 20ms.
  base::TimeTicks time = down_time + base::Milliseconds(20);
  touch_.x = 20;
  touch_.y = 30;
  touch_.major = 220;
  touch_.minor = 120;
  PalmFilterSample sample2 =
      CreatePalmFilterSample(touch_, time, model_config_, device_info);
  stroke.ProcessSample(sample2);

  ASSERT_THAT(stroke.samples(), ElementsAre(sample1, sample2));

  // Verify resampled sample : time = T + 8ms
  PalmFilterSample resampled_sample =
      stroke.GetSampleAt(down_time + base::Milliseconds(8));
  EXPECT_EQ(8, resampled_sample.point.x());
  EXPECT_EQ(18, resampled_sample.point.y());
  EXPECT_EQ(220, resampled_sample.major_radius);
  EXPECT_EQ(120, resampled_sample.minor_radius);

  // Verify resampled sample : time = T + 16ms
  resampled_sample = stroke.GetSampleAt(down_time + base::Milliseconds(16));
  EXPECT_EQ(16, resampled_sample.point.x());
  EXPECT_EQ(26, resampled_sample.point.y());
  EXPECT_EQ(220, resampled_sample.major_radius);
  EXPECT_EQ(120, resampled_sample.minor_radius);
}

}  // namespace ui
