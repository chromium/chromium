// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_SCOPED_EVENT_TEST_TICK_CLOCK_H_
#define UI_EVENTS_TEST_SCOPED_EVENT_TEST_TICK_CLOCK_H_

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/events/base_event_utils.h"

namespace ui {
namespace test {

// Helper class to mock ui events tick clock. On construction it registers a
// simple test tick clock for ui events. On destruction, it clears the test
// clock.
//
// Example:
//   TEST_F(SomeFixture, MyTest) {
//        ScopedEventTestTickClock clock;
//        clock.SetNowSeconds(1200);
//   }
class ScopedEventTestTickClock {
 public:
  ScopedEventTestTickClock() { ui::SetEventTickClockForTesting(&test_clock_); }

  ScopedEventTestTickClock(const ScopedEventTestTickClock&) = delete;
  ScopedEventTestTickClock& operator=(const ScopedEventTestTickClock&) = delete;

  ~ScopedEventTestTickClock() { ui::SetEventTickClockForTesting(nullptr); }

  void SetNowSeconds(int64_t seconds) {
    test_clock_.SetNowTicks(base::TimeTicks() + base::Seconds(seconds));
  }

  void SetNowTicks(base::TimeTicks ticks) { test_clock_.SetNowTicks(ticks); }

  void Advance(base::TimeDelta delta) { test_clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock test_clock_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_SCOPED_EVENT_TEST_TICK_CLOCK_H_
