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

  // Latest time that a finger was detected on a touchscreen. It may or may not
  // have been converted into a palm later.
  base::TimeTicks latest_finger_touch_time = base::TimeTicks::UnixEpoch();

  // From a touch screen, the number of active touches on the screen that aren't
  // palms.
  uint32_t active_finger_touches = 0;

  // From a touch screen, the number of active palms on the screen.
  uint32_t active_palm_touches = 0;

  // Latest time that a palm was detected on a touchscreen. the palm may or may
  // not still be on the touchscreen.
  base::TimeTicks latest_palm_touch_time = base::TimeTicks::UnixEpoch();
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_
