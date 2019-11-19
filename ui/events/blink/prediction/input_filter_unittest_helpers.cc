// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/input_filter_unittest_helpers.h"

#include "base/rand_util.h"

namespace ui {
namespace test {

InputFilterTest::InputFilterTest() = default;
InputFilterTest::~InputFilterTest() = default;

// Check if the filter is well cloned. We send random values to the filter and
// then we clone it. If we send the same new random values to both filters,
// we should have the same filtered results
void InputFilterTest::TestCloneFilter() {
  gfx::PointF point;
  base::TimeTicks ts = blink::WebInputEvent::GetStaticTimeStampForTests();
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(8);

  for (int i = 0; i < 100; i++) {
    point.SetPoint(base::RandDouble(), base::RandDouble());
    EXPECT_TRUE(filter_->Filter(ts, &point));  // We just feed the filter
    ts += delta;
  }

  std::unique_ptr<InputFilter> fork_filter;
  fork_filter.reset(filter_->Clone());

  gfx::PointF filtered_point, fork_filtered_point;
  for (int i = 0; i < 100; i++) {
    point.SetPoint(base::RandDouble(), base::RandDouble());
    filtered_point = point;
    fork_filtered_point = point;
    EXPECT_TRUE(filter_->Filter(ts, &filtered_point));
    EXPECT_TRUE(fork_filter->Filter(ts, &fork_filtered_point));
    EXPECT_NEAR(filtered_point.x(), fork_filtered_point.x(), kEpsilon);
    EXPECT_NEAR(filtered_point.y(), fork_filtered_point.y(), kEpsilon);
    ts += delta;
  }
}

// Check if the filter is well reset. We send random values, save the values and
// results, then we reset the filter. We send again the same values and see if
// we have the same results, which would be statistically impossible with 100
// random without a proper resetting.
void InputFilterTest::TestResetFilter() {
  std::vector<gfx::PointF> points;
  std::vector<base::TimeTicks> timestamps;
  std::vector<gfx::PointF> results;
  gfx::PointF point;
  base::TimeTicks ts = blink::WebInputEvent::GetStaticTimeStampForTests();
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(8);

  for (int i = 0; i < 100; i++) {
    point.SetPoint(base::RandDouble(), base::RandDouble());
    points.push_back(point);
    timestamps.push_back(ts);
    EXPECT_TRUE(filter_->Filter(ts, &point));
    results.push_back(point);
    ts += delta;
  }

  filter_->Reset();

  EXPECT_EQ((int)points.size(), 100);
  EXPECT_EQ((int)timestamps.size(), 100);
  EXPECT_EQ((int)results.size(), 100);

  for (int i = 0; i < 100; i++) {
    point = points[i];
    EXPECT_TRUE(filter_->Filter(timestamps[i], &point));
    EXPECT_NEAR(results[i].x(), point.x(), kEpsilon);
    EXPECT_NEAR(results[i].y(), point.y(), kEpsilon);
  }
}

}  // namespace test
}  // namespace ui
