// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/scoped_visibility_tracker.h"

#include <utility>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

class ScopedVisibilityTrackerTest : public testing::Test {};

TEST_F(ScopedVisibilityTrackerTest, NeverVisible) {
  auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
  ScopedVisibilityTracker tracker(tick_clock.get(), false /* is_shown */);

  tick_clock->Advance(base::Minutes(10));
  EXPECT_EQ(base::Minutes(0), tracker.GetForegroundDuration());
}

TEST_F(ScopedVisibilityTrackerTest, SimpleVisibility) {
  auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
  ScopedVisibilityTracker tracker(tick_clock.get(), true /* is_shown */);

  tick_clock->Advance(base::Minutes(10));
  EXPECT_EQ(base::Minutes(10), tracker.GetForegroundDuration());
}

TEST_F(ScopedVisibilityTrackerTest, HiddenThenShown) {
  auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
  ScopedVisibilityTracker tracker(tick_clock.get(), true /* is_shown */);

  tick_clock->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(1), tracker.GetForegroundDuration());

  tracker.OnHidden();
  tick_clock->Advance(base::Minutes(2));
  EXPECT_EQ(base::Minutes(1), tracker.GetForegroundDuration());

  tracker.OnShown();
  tick_clock->Advance(base::Minutes(3));
  EXPECT_EQ(base::Minutes(4), tracker.GetForegroundDuration());
}

TEST_F(ScopedVisibilityTrackerTest, InitiallyHidden) {
  auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
  ScopedVisibilityTracker tracker(tick_clock.get(), false /* is_shown */);

  tick_clock->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(0), tracker.GetForegroundDuration());

  tracker.OnShown();
  tick_clock->Advance(base::Minutes(2));
  EXPECT_EQ(base::Minutes(2), tracker.GetForegroundDuration());
}

// The object should be robust to double hidden and shown notification
TEST_F(ScopedVisibilityTrackerTest, DoubleNotifications) {
  auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
  ScopedVisibilityTracker tracker(tick_clock.get(), false /* is_shown */);

  tick_clock->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(0), tracker.GetForegroundDuration());

  tracker.OnHidden();
  tick_clock->Advance(base::Minutes(1));
  EXPECT_EQ(base::Minutes(0), tracker.GetForegroundDuration());

  tracker.OnShown();
  tick_clock->Advance(base::Minutes(2));
  EXPECT_EQ(base::Minutes(2), tracker.GetForegroundDuration());

  tracker.OnShown();
  tick_clock->Advance(base::Minutes(2));
  EXPECT_EQ(base::Minutes(4), tracker.GetForegroundDuration());
}

}  // namespace ui
