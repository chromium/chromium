// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fraction_of_time_without_user_input_recorder.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"

namespace {

constexpr base::TimeDelta DEFAULT_WINDOW_SIZE = base::Seconds(10);
constexpr base::TimeDelta DEFAULT_IDLE_TIMEOUT = base::Seconds(0.05);

}  // namespace

namespace ui {

FractionOfTimeWithoutUserInputRecorder::FractionOfTimeWithoutUserInputRecorder()
    : window_size_(DEFAULT_WINDOW_SIZE), idle_timeout_(DEFAULT_IDLE_TIMEOUT) {}

void FractionOfTimeWithoutUserInputRecorder::RecordEventAtTime(
    base::TimeTicks start_time) {
  base::TimeTicks event_end_time = start_time + idle_timeout_;

  if (active_duration_start_time_.is_null())
    active_duration_start_time_ = start_time;
  if (previous_event_end_time_.is_null())
    previous_event_end_time_ = start_time;

  // The user is no longer interacting with the browser. Report the previous
  // active duration.
  if (previous_event_end_time_ < start_time) {
    RecordActiveInterval(active_duration_start_time_, previous_event_end_time_);
    active_duration_start_time_ = start_time;
  }

  previous_event_end_time_ = event_end_time;
}

void FractionOfTimeWithoutUserInputRecorder::RecordActiveInterval(
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  if (window_start_time_.is_null())
    window_start_time_ = start_time;

  base::TimeTicks window_end_time;

  while (true) {
    window_end_time = window_start_time_ + window_size_;
    base::TimeDelta interval_in_window_duration =
        std::min(end_time, window_end_time) -
        std::max(start_time, window_start_time_);
    interval_in_window_duration =
        std::max(interval_in_window_duration, base::TimeDelta());

    current_window_active_time_ += interval_in_window_duration;

    // If we haven't exceeded the window bounds, we're done.
    if (end_time < window_end_time)
      break;

    RecordToUma(current_window_active_time_ / window_size_);

    current_window_active_time_ = base::TimeDelta();
    window_start_time_ = window_end_time;
  }
}

void FractionOfTimeWithoutUserInputRecorder::RecordToUma(
    float fraction_active) const {
  UMA_HISTOGRAM_PERCENTAGE("Event.FractionOfTimeWithoutUserInput",
                           std::round((1 - fraction_active) * 100));
}

}  // namespace ui
