// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_

#include "base/time/time.h"

namespace ui {

struct SharedPalmDetectionFilterState {
  // The latest stylus touch time. Note that this can include "hover".
  base::TimeTicks latest_stylus_touch_time;

  // Latest time that a finger touch was detected on a touchscreen. It may or
  // may not have been detected as a palm.
  base::TimeTicks latest_finger_touch_time;

  // If there is a palm in the latest touch frame.
  bool has_palm = false;

  // If we need to record the after stylus metrics when we handle the next touch
  // event.
  bool need_to_record_after_stylus_metrics = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_
