// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/mac/power/power_sampler/timer_sampling_event_source.h"

#include "base/check.h"

namespace power_sampler {

TimerSamplingEventSource::TimerSamplingEventSource(base::TimeDelta interval)
    : interval_(interval) {}

TimerSamplingEventSource::~TimerSamplingEventSource() = default;

bool TimerSamplingEventSource::Start(SamplingEventCallback callback) {
  DCHECK(callback);
  timer_.Start(FROM_HERE, interval_, std::move(callback));
  return true;
}

}  // namespace power_sampler
