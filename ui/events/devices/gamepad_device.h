// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_GAMEPAD_DEVICE_H_
#define UI_EVENTS_DEVICES_GAMEPAD_DEVICE_H_

#include <vector>

#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/input_device.h"

namespace ui {

// Represents a gamepad device state.
struct EVENTS_DEVICES_EXPORT GamepadDevice : public InputDevice {
  // Represents an axis of a gamepad e.g. an analog thumb stick.
  struct Axis {
    // Gamepad axis index. Corresponds to |raw_code_| of GamepadEvent.
    uint16_t code = 0;

    // See input_absinfo for the definition of these variables.
    int32_t min_value = 0;
    int32_t max_value = 0;
    int32_t flat = 0;
    int32_t fuzz = 0;
    int32_t resolution = 0;
  };

  GamepadDevice(const InputDevice& input_device,
                std::vector<Axis>&& axes,
                bool supports_rumble);
  GamepadDevice(const GamepadDevice& other);
  ~GamepadDevice() override;

  // Axes the gamepad has e.g. analog thumb sticks.
  std::vector<Axis> axes;

  // Whether the gamepad device supports rumble type force feedback.
  bool supports_vibration_rumble = false;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_GAMEPAD_DEVICE_H_
