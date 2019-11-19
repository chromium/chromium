// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device.h"

#include <string>

namespace ui {

// static
const int InputDevice::kInvalidId = 0;

InputDevice::InputDevice()
    : id(kInvalidId),
      type(InputDeviceType::INPUT_DEVICE_UNKNOWN),
      vendor_id(0),
      product_id(0),
      version(0) {}

InputDevice::InputDevice(int id, InputDeviceType type, const std::string& name)
    : id(id), type(type), name(name), vendor_id(0), product_id(0), version(0) {}

InputDevice::InputDevice(int id,
                         InputDeviceType type,
                         const std::string& name,
                         const std::string& phys,
                         const base::FilePath& sys_path,
                         uint16_t vendor,
                         uint16_t product,
                         uint16_t version)
    : id(id),
      type(type),
      name(name),
      phys(phys),
      sys_path(sys_path),
      vendor_id(vendor),
      product_id(product),
      version(version) {}

InputDevice::InputDevice(const InputDevice& other) = default;

InputDevice::~InputDevice() {
}

}  // namespace ui
