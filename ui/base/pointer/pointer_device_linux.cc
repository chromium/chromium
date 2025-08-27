// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ui/events/devices/device_data_manager.h"

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  return !DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty();
}

bool IsMouseOrTouchpadPresent() {
  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  return std::ranges::any_of(device_data_manager->GetTouchpadDevices(),
                             std::identity(), &InputDevice::enabled) ||
         std::ranges::any_of(device_data_manager->GetMouseDevices(),
                             std::identity(), &InputDevice::enabled) ||
         std::ranges::any_of(device_data_manager->GetPointingStickDevices(),
                             std::identity(), &InputDevice::enabled);
}

}  // namespace

std::pair<int, int> GetAvailablePointerAndHoverTypesImpl() {
  int pointer_types = IsTouchDevicePresent() ? POINTER_TYPE_COARSE : 0;
  int hover_types = HOVER_TYPE_NONE;
  if (IsMouseOrTouchpadPresent()) {
    pointer_types |= POINTER_TYPE_FINE;
    hover_types = HOVER_TYPE_HOVER;
  }
  return {pointer_types ? pointer_types : POINTER_TYPE_NONE, hover_types};
}

TouchScreensAvailability GetTouchScreensAvailability() {
  if (!IsTouchDevicePresent()) {
    return TouchScreensAvailability::NONE;
  }

  return DeviceDataManager::GetInstance()->AreTouchscreensEnabled()
             ? TouchScreensAvailability::ENABLED
             : TouchScreensAvailability::DISABLED;
}

int MaxTouchPoints() {
  const std::vector<TouchscreenDevice>& touchscreen_devices =
      DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  const auto& it = std::ranges::max_element(touchscreen_devices, {},
                                            &TouchscreenDevice::touch_points);
  return (it == touchscreen_devices.end()) ? 0 : it->touch_points;
}

}  // namespace ui
