// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/prediction/one_euro_filter.h"

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/prediction/prediction_unittest_helpers.h"
namespace ui {
namespace test {

class OneEuroFilterTest : public testing::Test {
 public:
  explicit OneEuroFilterTest() = default;
  ~OneEuroFilterTest() override = default;

  OneEuroFilterTest(const OneEuroFilterTest&) = delete;
  OneEuroFilterTest& operator=(const OneEuroFilterTest&) = delete;

  void SetUp() override { filter_ = std::make_unique<OneEuroFilter>(1, 1); }

 protected:
  std::unique_ptr<InputFilter> filter_;
};

// Check if sending values between 0 and 1 keeps filtered values between 0 and 1
TEST_F(OneEuroFilterTest, filteringValues) {
  base::TimeTicks ts = PredictionUnittestHelpers::GetStaticTimeStampForTests();
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
