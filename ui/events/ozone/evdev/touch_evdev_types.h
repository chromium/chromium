// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_

#include <stddef.h>

#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

// Number of supported touch slots. ABS_MT_SLOT messages with
// value >= kNumTouchEvdevSlots are ignored.
const int kNumTouchEvdevSlots = 20;

// Contains information about an in progress touch.
struct EVENTS_OZONE_EVDEV_EXPORT InProgressTouchEvdev {
  InProgressTouchEvdev();
  InProgressTouchEvdev(const InProgressTouchEvdev& other);
  ~InProgressTouchEvdev();

  // Current touch major of this slot.
  int major = 0;

  // Current touch minor of this slot.
  int minor = 0;

  // Current tool type of this slot.
  int tool_type = 0;

  // Whether there is new information for the touch.
  bool altered = false;

  // Whether the touch was cancelled. Touch events should be ignored till a
  // new touch is initiated.
  bool was_cancelled = false;

  // Whether the touch is going to be canceled.
  bool cancelled = false;

  // Whether the touch is delayed at first appearance. Will not be reported yet.
  bool delayed = false;

  // Whether the touch was delayed before.
  bool was_delayed = false;

  // Whether the touch is held until end or no longer held.
  bool held = false;

  // Whether this touch was held before being sent.
  bool was_held = false;

  bool was_touching = false;
  bool touching = false;
  float x = 0;
  float y = 0;
  int tracking_id = -1;
  size_t slot = 0;
  float radius_x = 0;
  float radius_y = 0;
  float pressure = 0;
  int tool_code = 0;
  float tilt_x = 0;
  float tilt_y = 0;
  ui::EventPointerType reported_tool_type =
      ui::EventPointerType::POINTER_TYPE_TOUCH;
  bool stylus_button = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVDEV_TYPES_H_
