// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/timer_based_tick_provider.h"
#include "base/task/sequenced_task_runner.h"

namespace blink {

// static
base::TimeTicks TimerBasedTickProvider::TimeSnappedToNextTick(
    base::TimeTicks time,
    base::TimeDelta period) {
  return time.SnappedToNextTick(base::TimeTicks(), period);
}

TimerBasedTickProvider::TimerBasedTickProvider(base::TimeDelta tick_period)
    : tick_period_(tick_period) {}

void TimerBasedTickProvider::RequestCallOnNextTick(base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE, std::move(callback),
      TimeSnappedToNextTick(base::TimeTicks::Now(), tick_period_),
      base::subtle::DelayPolicy::kPrecise);
}

base::TimeDelta TimerBasedTickProvider::TickPeriod() {
  return tick_period_;
}

}  // namespace blink
