// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"

namespace ui {

class OpenPalmDetectionFilterTest : public testing::Test {
 public:
  OpenPalmDetectionFilterTest() = default;

  void SetUp() override {
    shared_palm_state = std::make_unique<SharedPalmDetectionFilterState>();
    palm_detection_filter_.reset(
        new OpenPalmDetectionFilter(shared_palm_state.get()));
  }

 protected:
  std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state;
  std::unique_ptr<PalmDetectionFilter> palm_detection_filter_;

  DISALLOW_COPY_AND_ASSIGN(OpenPalmDetectionFilterTest);
};

TEST_F(OpenPalmDetectionFilterTest, TestSetsToZero) {
  std::bitset<kNumTouchEvdevSlots> suppress, hold;
  suppress.set(kNumTouchEvdevSlots - 1, 1);
  hold.set(0, 1);
  std::vector<InProgressTouchEvdev> inputs;
  inputs.resize(kNumTouchEvdevSlots);
  palm_detection_filter_->Filter(inputs, base::TimeTicks::Now(), &hold,
                                 &suppress);
  EXPECT_TRUE(hold.none());
  EXPECT_TRUE(suppress.none());
}

}  // namespace ui
