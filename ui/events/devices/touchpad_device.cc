// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/touchpad_device.h"

namespace ui {

TouchpadDevice::TouchpadDevice() = default;

TouchpadDevice::TouchpadDevice(int id,
                               InputDeviceType type,
                               const std::string& name,
                               bool is_haptic)
    : InputDevice(id, type, name), is_haptic(is_haptic) {}

TouchpadDevice::TouchpadDevice(int id,
                               InputDeviceType type,
                               const std::string& name,
                               const std::string& phys,
                               const base::FilePath& sys_path,
                               uint16_t vendor,
                               uint16_t product,
                               uint16_t version,
                               bool is_haptic)
    : InputDevice(id, type, name, phys, sys_path, vendor, product, version),
      is_haptic(is_haptic) {}

TouchpadDevice::TouchpadDevice(const InputDevice& input_device, bool is_haptic)
    : InputDevice(input_device), is_haptic(is_haptic) {}

TouchpadDevice::TouchpadDevice(const TouchpadDevice& other) = default;

TouchpadDevice::~TouchpadDevice() = default;

}  // namespace ui
