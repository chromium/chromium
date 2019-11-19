// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include "base/logging.h"
#include "ui/events/devices/device_data_manager.h"

namespace ui {

namespace {

bool IsTouchDevicePresent() {
  return !DeviceDataManager::GetInstance()->GetTouchscreenDevices().empty();
}

bool IsMouseOrTouchpadPresent() {
  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  for (const ui::InputDevice& device :
       device_data_manager->GetTouchpadDevices()) {
    if (device.enabled)
      return true;
  }
  // We didn't find a touchpad then let's look if there is a mouse connected.
  for (const ui::InputDevice& device : device_data_manager->GetMouseDevices()) {
    if (device.enabled)
      return true;
  }
  return false;
}

}  // namespace

int GetAvailablePointerTypes() {
  int available_pointer_types = 0;
  if (IsMouseOrTouchpadPresent())
    available_pointer_types |= POINTER_TYPE_FINE;

  if (IsTouchDevicePresent())
    available_pointer_types |= POINTER_TYPE_COARSE;

  if (available_pointer_types == 0)
    available_pointer_types = POINTER_TYPE_NONE;

  DCHECK(available_pointer_types);
  return available_pointer_types;
}

int GetAvailableHoverTypes() {
  if (IsMouseOrTouchpadPresent())
    return HOVER_TYPE_HOVER;

  return HOVER_TYPE_NONE;
}

TouchScreensAvailability GetTouchScreensAvailability() {
  if (!IsTouchDevicePresent())
    return TouchScreensAvailability::NONE;

  return DeviceDataManager::GetInstance()->AreTouchscreensEnabled()
             ? TouchScreensAvailability::ENABLED
             : TouchScreensAvailability::DISABLED;
}

int MaxTouchPoints() {
  int max_touch = 0;
  const std::vector<ui::TouchscreenDevice>& touchscreen_devices =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  for (const ui::TouchscreenDevice& device : touchscreen_devices) {
    if (device.touch_points > max_touch)
      max_touch = device.touch_points;
  }
  return max_touch;
}

PointerType GetPrimaryPointerType(int available_pointer_types) {
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

HoverType GetPrimaryHoverType(int available_hover_types) {
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}

}  // namespace ui
