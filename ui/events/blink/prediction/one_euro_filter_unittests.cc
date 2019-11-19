// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/input_filter_unittest_helpers.h"
#include "ui/events/blink/prediction/one_euro_filter.h"

namespace ui {
namespace test {

class OneEuroFilterTest : public InputFilterTest {
 public:
  explicit OneEuroFilterTest() {}

  void SetUp() override { filter_ = std::make_unique<ui::OneEuroFilter>(1, 1); }

  DISALLOW_COPY_AND_ASSIGN(OneEuroFilterTest);
};

TEST_F(OneEuroFilterTest, TestClone) {
  TestCloneFilter();
}

TEST_F(OneEuroFilterTest, TestReset) {
  TestResetFilter();
}

// Check if sending values between 0 and 1 keeps filtered values between 0 and 1
TEST_F(OneEuroFilterTest, filteringValues) {
  base::TimeTicks ts = blink::WebInputEvent::GetStaticTimeStampForTests();
  gfx::PointF point;
  for (int i = 0; i < 100; i++) {
    point.SetPoint(base::RandDouble(), base::RandDouble());
    EXPECT_TRUE(filter_->Filter(ts, &point));
    EXPECT_LT(point.x(), 1.0);
    EXPECT_LT(point.y(), 1.0);
    EXPECT_GT(point.x(), 0.0);
    EXPECT_GT(point.y(), 0.0);
  }
}

}  // namespace test
}  // namespace ui