// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device.h"

#include <ostream>
#include <string>

#include "base/strings/stringprintf.h"

namespace ui {

// static
const int InputDevice::kInvalidId = 0;

std::ostream& operator<<(std::ostream& os, const InputDeviceType value) {
  switch (value) {
    case InputDeviceType::INPUT_DEVICE_INTERNAL:
      return os << "ui::InputDeviceType::INPUT_DEVICE_INTERNAL";
    case InputDeviceType::INPUT_DEVICE_USB:
      return os << "ui::InputDeviceType::INPUT_DEVICE_USB";
    case InputDeviceType::INPUT_DEVICE_BLUETOOTH:
      return os << "ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH";
    case InputDeviceType::INPUT_DEVICE_UNKNOWN:
      return os << "ui::InputDeviceType::INPUT_DEVICE_UNKNOWN";
  }
  return os << "ui::InputDeviceType::unknown_value("
            << static_cast<unsigned int>(value) << ")";
}

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

std::ostream& InputDevice::DescribeForLog(std::ostream& os) const {
  return os << "class=ui::InputDevice id=" << id << std::endl
            << " input_device_type=" << type << std::endl
            << " name=\"" << name << "\"" << std::endl
            << " phys=\"" << phys << "\"" << std::endl
            << " enabled=" << enabled << std::endl
            << " suspected_keyboard_imposter=" << suspected_keyboard_imposter
            << std::endl
            << " suspected_mouse_imposter=" << suspected_mouse_imposter
            << std::endl
            << " sys_path=\"" << sys_path.AsUTF8Unsafe() << "\"" << std::endl
            << " vendor_id=" << base::StringPrintf("%04X", vendor_id)
            << std::endl
            << " product_id=" << base::StringPrintf("%04X", product_id)
            << std::endl
            << " version=" << base::StringPrintf("%04X", version) << std::endl;
}

}  // namespace ui
