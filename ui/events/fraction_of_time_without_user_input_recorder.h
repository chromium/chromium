// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FRACTION_OF_TIME_WITHOUT_USER_INPUT_RECORDER_H_
#define UI_EVENTS_FRACTION_OF_TIME_WITHOUT_USER_INPUT_RECORDER_H_

#include "base/time/time.h"
#include "ui/events/events_base_export.h"

namespace ui {

// Receives as input a set of timestamps indicating when events were
// received. Reports via UMA the fraction of the time per |window_size_| that
// the user was interacting.
class EVENTS_BASE_EXPORT FractionOfTimeWithoutUserInputRecorder {
 public:
  FractionOfTimeWithoutUserInputRecorder();
  void RecordEventAtTime(base::TimeTicks start_time);

 protected:
  virtual void RecordActiveInterval(base::TimeTicks start_time,
                                    base::TimeTicks end_time);
  void RecordToUma(float idle_fraction) const;
  void set_window_size(base::TimeDelta window_size) {
    window_size_ = window_size;
  }
  void set_idle_timeout(base::TimeDelta idle_timeout) {
    idle_timeout_ = idle_timeout;
  }

 private:
  // Within the current period of length |window_size_|, how long has the user
  // been active?
  base::TimeDelta current_window_active_time_;
  // If the user is currently active, when did they start being active?
  base::TimeTicks active_duration_start_time_;
  base::TimeTicks window_start_time_;
  base::TimeTicks previous_event_end_time_;

  // We report the fraction of the time we were idle once per |window_size_|.
  base::TimeDelta window_size_;

  // Two events within |idle_timeout_| of one another are considered to be in
  // the same period of user activity.
  base::TimeDelta idle_timeout_;
};

}  // namespace ui

#endif  // UI_EVENTS_FRACTION_OF_TIME_WITHOUT_USER_INPUT_RECORDER_H_
