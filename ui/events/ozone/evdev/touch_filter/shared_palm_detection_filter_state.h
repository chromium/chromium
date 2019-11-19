// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_FILTER_SHARED_PALM_DETECTION_FILTER_STATE_H_

#include "base/time/time.h"

namespace ui {

struct SharedPalmDetectionFilterState {
  // The latest stylus touch time. Note that this can include "hover".
  base::TimeTicks latest_stylus_touch_time_;
};

}  // namespace ui

#endif
