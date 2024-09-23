// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_KEYBOARD_DEVICE_H_
#define UI_EVENTS_DEVICES_KEYBOARD_DEVICE_H_

#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device.h"

namespace ui {

// Represents a keyboard device state.
struct EVENTS_DEVICES_EXPORT KeyboardDevice : public InputDevice {
  // Creates an invalid `KeyboardDevice`.
  KeyboardDevice();

  KeyboardDevice(int id,
                 InputDeviceType type,
                 const std::string& name,
                 bool has_assistant_key = false,
                 bool has_function_key = false);
  KeyboardDevice(int id,
                 InputDeviceType type,
                 const std::string& name,
                 const std::string& phys,
                 const base::FilePath& sys_path,
                 uint16_t vendor,
                 uint16_t product,
                 uint16_t version,
                 bool has_assistant_key = false,
                 bool has_function_key = false);
  explicit KeyboardDevice(InputDevice input_device,
                          bool has_assistant_key = false,
                          bool has_function_key = false);

  KeyboardDevice(const KeyboardDevice& other);
  ~KeyboardDevice() override;

  // Whether or not the keyboard claims to have the assistant key.
  bool has_assistant_key = false;

  // Whether or not the keyboard claims to have the function key.
  bool has_function_key = false;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_KEYBOARD_DEVICE_H_
