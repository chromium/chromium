// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"

#include <linux/input.h>

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

class HeuristicStylusPalmDetectionFilterTest : public testing::Test {
 public:
  HeuristicStylusPalmDetectionFilterTest() = default;

  HeuristicStylusPalmDetectionFilterTest(
      const HeuristicStylusPalmDetectionFilterTest&) = delete;
  HeuristicStylusPalmDetectionFilterTest& operator=(
      const HeuristicStylusPalmDetectionFilterTest&) = delete;

  void SetUp() override {
    shared_palm_state = std::make_unique<SharedPalmDetectionFilterState>();
    palm_detection_filter_ =
        std::make_unique<HeuristicStylusPalmDetectionFilter>(
            shared_palm_state.get(), hold_sample_count, hold_time,
            suppress_time);
    touches_.resize(kNumTouchEvdevSlots);
    test_start_time_ = base::TimeTicks::Now();
  }

 protected:
  const int hold_sample_count = 5;
  const base::TimeDelta hold_time = base::Seconds(1.0);
  const base::TimeDelta suppress_time = base::Seconds(0.4);

  const base::TimeDelta sample_interval = base::Milliseconds(7.5);

  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state;
  std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;
  std::vector<InProgressTouchEvdev> touches_;
  base::TimeTicks test_start_time_;
};

class HeuristicStylusPalmDetectionFilterDeathTest
    : public HeuristicStylusPalmDetectionFilterTest {};

TEST_F(HeuristicStylusPalmDetectionFilterDeathTest, TestDCheck) {
  // We run with a time where hold_time < suppress_time, which should DCHECK.
  EXPECT_DCHECK_DEATH(palm_detection_filter_ =
                          std::make_unique<HeuristicStylusPalmDetectionFilter>(
                              shared_palm_state.get(), hold_sample_count,
                              hold_time, hold_time + base::Milliseconds(0.1)));
}

TEST_F(HeuristicStylusPalmDetectionFilterTest, TestSetsToZero) {
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  suppress.set(kNumTouchEvdevSlots - 1, true);
  hold.set(0, true);
  palm_detection_filter_->Filter(touches_, test_start_time_, &hold, &suppress);
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());
}

TEST_F(HeuristicStylusPalmDetectionFilterTest, TestCancelAfterStylus) {
  touches_[0].touching = true;
  touches_[0].tool_code = BTN_TOOL_PEN;
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  // Set Palm as test_start_time_;
  palm_detection_filter_->Filter(touches_, test_start_time_, &hold, &suppress);
  shared_palm_state->latest_stylus_touch_time = test_start_time_;
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());

  // Now, lets start two touches 7.5ms afterwards.
  touches_[0].tool_code = 0;
  touches_[1].touching = true;
  base::TimeTicks start_time = test_start_time_ + sample_interval;
  palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
  // expect none held, 0 and 1 cancelled, and others untouched.
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.test(0));
  EXPECT_TRUE(suppress.test(1));
  suppress.reset(0);
  suppress.reset(1);
  EXPECT_TRUE(suppress.none());

  // Now, what if we keep going with these strokes for a long time.
  for (; start_time < test_start_time_ + base::Milliseconds(1000);
       start_time += sample_interval) {
    palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
    EXPECT_TRUE(hold.none());
    EXPECT_TRUE(suppress.test(0));
    EXPECT_TRUE(suppress.test(1));
    suppress.reset(0);
    suppress.reset(1);
    EXPECT_TRUE(suppress.none());
  }
}

TEST_F(HeuristicStylusPalmDetectionFilterTest, TestHoldAfterStylus) {
  touches_[0].touching = true;
  touches_[0].tool_code = BTN_TOOL_PEN;
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  // Set Palm as test_start_time_;
  palm_detection_filter_->Filter(touches_, test_start_time_, &hold, &suppress);
  shared_palm_state->latest_stylus_touch_time = test_start_time_;
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());

  // Now, lets start two touches a little before end of hold time.
  touches_[0].tool_code = 0;
  touches_[1].touching = true;
  base::TimeTicks start_time =
      test_start_time_ + hold_time - (hold_sample_count - 2) * sample_interval;
  palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
  EXPECT_TRUE(suppress.none());
  EXPECT_TRUE(hold.test(0));
  EXPECT_TRUE(hold.test(1));
  hold.reset(0);
  hold.reset(1);
  EXPECT_TRUE(hold.none());
  for (int i = 0; i < 10; ++i) {
    start_time += sample_interval;
    palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
    EXPECT_TRUE(suppress.none());
    // We've already held one item, so we expect 1 - the sample count to get
    // held. Note that 1 of these falls _after_ the overall hold time, but we
    // hold it since we depend on touch start.
    if (i < hold_sample_count - 1) {
      EXPECT_TRUE(hold.test(0)) << "Failed at i = " << i;
      EXPECT_TRUE(hold.test(1)) << "Failed at i = " << i;
      hold.reset(0);
      hold.reset(1);
      EXPECT_TRUE(hold.none());
    } else {
      EXPECT_TRUE(hold.none());
    }
  }
}

TEST_F(HeuristicStylusPalmDetectionFilterTest, TestNothingLongAfterStylus) {
  touches_[0].touching = true;
  touches_[0].tool_code = BTN_TOOL_PEN;
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  // Set Palm as test_start_time_;
  palm_detection_filter_->Filter(touches_, test_start_time_, &hold, &suppress);
  shared_palm_state->latest_stylus_touch_time = test_start_time_;
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());
  touches_[0].tool_code = 0;
  touches_[1].touching = true;
  base::TimeTicks start_time =
      test_start_time_ + hold_time + base::Milliseconds(1e-2);
  palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());
}

TEST_F(HeuristicStylusPalmDetectionFilterTest, TestHover) {
  touches_[0].touching = false;
  touches_[0].tool_code = BTN_TOOL_PEN;
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  // Set Palm as test_start_time_;
  palm_detection_filter_->Filter(touches_, test_start_time_, &hold, &suppress);
  shared_palm_state->latest_stylus_touch_time = test_start_time_;
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());

  // Now, do we filter a finger?
  touches_[0].tool_code = 0;
  touches_[0].touching = true;
  base::TimeTicks start_time =
      test_start_time_ + hold_time - base::Milliseconds(1e-2);
  palm_detection_filter_->Filter(touches_, start_time, &hold, &suppress);
  EXPECT_TRUE(hold.test(0));
  EXPECT_TRUE(suppress.none());
  hold.reset(0);
  EXPECT_TRUE(hold.none());
}

}  // namespace ui
