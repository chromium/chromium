// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/empty_filter.h"
#include "ui/events/blink/prediction/filter_factory.h"
#include "ui/events/blink/prediction/input_filter_unittest_helpers.h"

namespace ui {
namespace test {

class EmptyFilterTest : public InputFilterTest {
 public:
  explicit EmptyFilterTest() {}

  void SetUp() override { filter_ = std::make_unique<ui::EmptyFilter>(); }

  DISALLOW_COPY_AND_ASSIGN(EmptyFilterTest);
};

// Test the Clone function of the filter
TEST_F(EmptyFilterTest, TestClone) {
  TestCloneFilter();
}

// Test the Reset function of the filter
TEST_F(EmptyFilterTest, TestReset) {
  TestResetFilter();
}

// Test the empty filter gives the same values
TEST_F(EmptyFilterTest, filteringValues) {
  base::TimeTicks ts = blink::WebInputEvent::GetStaticTimeStampForTests();
  gfx::PointF point, filtered_point;
  for (int i = 0; i < 100; i++) {
    point.SetPoint(base::RandDouble(), base::RandDouble());
    filtered_point = point;
    EXPECT_TRUE(filter_->Filter(ts, &filtered_point));
    EXPECT_EQ(point.x(), filtered_point.x());
    EXPECT_EQ(point.y(), filtered_point.y());
  }
}

}  // namespace test
}  // namespace ui