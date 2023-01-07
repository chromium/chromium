// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_
#define UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_

#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {

// Find sysfs device path for this device.
EVENTS_DEVICES_EXPORT base::FilePath
GetInputPathInSys(const base::FilePath& path);

// Finds device type (internal or external) based on device path.
EVENTS_DEVICES_EXPORT InputDeviceType
GetInputDeviceTypeFromPath(const base::FilePath& path);

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_
