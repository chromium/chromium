// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_
#define UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_

#include <stdint.h>

#include "ui/display/types/display_constants.h"
#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

struct EVENTS_DEVICES_EXPORT TouchDeviceTransform {
  TouchDeviceTransform();
  ~TouchDeviceTransform();

  int64_t display_id = display::kInvalidDisplayId;
  int32_t device_id = InputDevice::kInvalidId;
  gfx::Transform transform;
  // Amount to scale the touch radius by.
  double radius_scale = 1;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_TOUCH_DEVICE_TRANSFORM_H_
