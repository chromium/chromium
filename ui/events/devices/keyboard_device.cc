// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/input_device.h"

namespace ui {

KeyboardDevice::KeyboardDevice() = default;

KeyboardDevice::KeyboardDevice(int id,
                               InputDeviceType type,
                               const std::string& name,
                               bool has_assistant_key,
                               bool has_function_key)
    : InputDevice(id, type, name),
      has_assistant_key(has_assistant_key),
      has_function_key(has_function_key) {}

KeyboardDevice::KeyboardDevice(int id,
                               InputDeviceType type,
                               const std::string& name,
                               const std::string& phys,
                               const base::FilePath& sys_path,
                               uint16_t vendor,
                               uint16_t product,
                               uint16_t version,
                               bool has_assistant_key,
                               bool has_function_key)
    : InputDevice(id, type, name, phys, sys_path, vendor, product, version),
      has_assistant_key(has_assistant_key),
      has_function_key(has_function_key) {}

KeyboardDevice::KeyboardDevice(InputDevice input_device,
                               bool has_assistant_key,
                               bool has_function_key)
    : InputDevice(input_device),
      has_assistant_key(has_assistant_key),
      has_function_key(has_function_key) {}

KeyboardDevice::KeyboardDevice(const KeyboardDevice& other) = default;

KeyboardDevice::~KeyboardDevice() = default;

}  // namespace ui
