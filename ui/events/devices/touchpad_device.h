// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_TOUCHPAD_DEVICE_H_
#define UI_EVENTS_DEVICES_TOUCHPAD_DEVICE_H_

#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device.h"

namespace ui {

// Represents a touchpad device state.
struct EVENTS_DEVICES_EXPORT TouchpadDevice : public InputDevice {
  // Creates an invalid `TouchpadDevice`.
  TouchpadDevice();

  TouchpadDevice(int id,
                 InputDeviceType type,
                 const std::string& name,
                 bool is_haptic = false);
  TouchpadDevice(int id,
                 InputDeviceType type,
                 const std::string& name,
                 const std::string& phys,
                 const base::FilePath& sys_path,
                 uint16_t vendor,
                 uint16_t product,
                 uint16_t version,
                 bool is_haptic = false);
  explicit TouchpadDevice(const InputDevice& input_device,
                          bool is_haptic = false);

  TouchpadDevice(const TouchpadDevice& other);
  ~TouchpadDevice() override;

  // Whether the touchpad is considered a "haptic" touchpad.
  bool is_haptic = false;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_TOUCHPAD_DEVICE_H_
