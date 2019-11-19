// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_H_

#include <stdint.h>
#include <string>

#include "base/files/file_path.h"
#include "ui/events/devices/events_devices_export.h"

namespace ui {

enum InputDeviceType {
  INPUT_DEVICE_INTERNAL,   // Internally connected input device.
  INPUT_DEVICE_USB,        // Known externally connected usb input device.
  INPUT_DEVICE_BLUETOOTH,  // Known externally connected bluetooth input device.
  INPUT_DEVICE_UNKNOWN,    // Device that may or may not be an external device.
};

// Represents an input device state.
struct EVENTS_DEVICES_EXPORT InputDevice {
  static const int kInvalidId;

  // Creates an invalid input device.
  InputDevice();

  InputDevice(int id, InputDeviceType type, const std::string& name);
  InputDevice(int id,
              InputDeviceType type,
              const std::string& name,
              const std::string& phys,
              const base::FilePath& sys_path,
              uint16_t vendor,
              uint16_t product,
              uint16_t version);
  InputDevice(const InputDevice& other);
  virtual ~InputDevice();

  // ID of the device. This ID is unique between all input devices.
  int id;

  // The type of the input device.
  InputDeviceType type;

  // Name of the device.
  std::string name;

  // The physical location(port) associated with the input device. This is
  // (supposed to be) stable between reboots and hotplugs. However this may not
  // always be set and will change when the device is connected via a different
  // port.
  std::string phys;

  // If the device is enabled, and whether events should be dispatched to UI.
  bool enabled = true;

  // The path to the input device in the sysfs filesystem.
  base::FilePath sys_path;

  // USB-style device identifiers, where available, or 0 if unavailable.
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_H_
