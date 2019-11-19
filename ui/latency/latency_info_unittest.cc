// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_info.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks ToTestTimeTicks(int64_t micros) {
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(micros);
}

}  // namespace

TEST(LatencyInfoTest, AddTwoSeparateEvent) {
  LatencyInfo info;
  info.set_trace_id(1);
  EXPECT_FALSE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                     ToTestTimeTicks(100));
  EXPECT_TRUE(info.began());
  info.AddLatencyNumberWithTimestamp(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                                     ToTestTimeTicks(1000));

  EXPECT_EQ(info.latency_components().size(), 2u);
  base::TimeTicks timestamp;
  EXPECT_FALSE(info.FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, &timestamp));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &timestamp));
  EXPECT_EQ(timestamp, ToTestTimeTicks(100));
  EXPECT_TRUE(
      info.FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, &timestamp));
  EXPECT_EQ(timestamp, ToTestTimeTicks(1000));
}

TEST(LatencyInfoTest, CoalesceTwoGSU) {
  LatencyInfo info1, info2;
  info1.set_trace_id(1);
  info1.AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
      ToTestTimeTicks(1234));
  info1.set_scroll_update_delta(-3);

  info2.set_trace_id(2);
  info2.AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
      ToTestTimeTicks(2345));
  info2.set_scroll_update_delta(5);

  info1.CoalesceScrollUpdateWith(info2);
  base::TimeTicks timestamp;
  EXPECT_TRUE(info1.FindLatency(
      INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, &timestamp));
  EXPECT_EQ(timestamp, ToTestTimeTicks(2345));
  EXPECT_EQ(info1.scroll_update_delta(), 2);
}

}  // namespace ui
