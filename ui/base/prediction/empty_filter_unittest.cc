// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/empty_filter.h"

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/prediction/prediction_unittest_helpers.h"

namespace ui {
namespace test {

class EmptyFilterTest : public testing::Test {
 public:
  explicit EmptyFilterTest() = default;
  ~EmptyFilterTest() override = default;

  EmptyFilterTest(const EmptyFilterTest&) = delete;
  EmptyFilterTest& operator=(const EmptyFilterTest&) = delete;

  void SetUp() override { filter_ = std::make_unique<EmptyFilter>(); }

 protected:
  std::unique_ptr<InputFilter> filter_;
};

// Test the empty filter gives the same values
TEST_F(EmptyFilterTest, filteringValues) {
  base::TimeTicks ts = PredictionUnittestHelpers::GetStaticTimeStampForTests();
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
