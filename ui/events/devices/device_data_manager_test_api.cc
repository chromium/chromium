// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_data_manager_test_api.h"

#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {

DeviceDataManagerTestApi::DeviceDataManagerTestApi() = default;

DeviceDataManagerTestApi::~DeviceDataManagerTestApi() = default;

void DeviceDataManagerTestApi::NotifyObserversDeviceListsComplete() {
  DeviceDataManager::instance_->NotifyObserversDeviceListsComplete();
}

void DeviceDataManagerTestApi::
    NotifyObserversKeyboardDeviceConfigurationChanged() {
  DeviceDataManager::instance_
      ->NotifyObserversKeyboardDeviceConfigurationChanged();
}

void DeviceDataManagerTestApi::
    NotifyObserversMouseDeviceConfigurationChanged() {
  DeviceDataManager::instance_
      ->NotifyObserversMouseDeviceConfigurationChanged();
}

void DeviceDataManagerTestApi::
    NotifyObserversPointingStickDeviceConfigurationChanged() {
  DeviceDataManager::instance_
      ->NotifyObserversPointingStickDeviceConfigurationChanged();
}

void DeviceDataManagerTestApi::NotifyObserversStylusStateChanged(
    StylusState stylus_state) {
  DeviceDataManager::instance_->NotifyObserversStylusStateChanged(stylus_state);
}

void DeviceDataManagerTestApi::
    NotifyObserversTouchscreenDeviceConfigurationChanged() {
  DeviceDataManager::instance_
      ->NotifyObserversTouchscreenDeviceConfigurationChanged();
}

void DeviceDataManagerTestApi::
    NotifyObserversTouchpadDeviceConfigurationChanged() {
  DeviceDataManager::instance_
      ->NotifyObserversTouchpadDeviceConfigurationChanged();
}

void DeviceDataManagerTestApi::OnDeviceListsComplete() {
  DeviceDataManager::instance_->OnDeviceListsComplete();
}

void DeviceDataManagerTestApi::SetKeyboardDevices(
    const std::vector<KeyboardDevice>& devices) {
  DeviceDataManager::instance_->OnKeyboardDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetGraphicsTabletDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::instance_->OnGraphicsTabletDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetMouseDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::instance_->OnMouseDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetPointingStickDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::instance_->OnPointingStickDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetTouchscreenDevices(
    const std::vector<TouchscreenDevice>& devices,
    bool are_touchscreen_target_displays_valid) {
  DeviceDataManager::instance_->OnTouchscreenDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetTouchpadDevices(
    const std::vector<TouchpadDevice>& devices) {
  DeviceDataManager::instance_->OnTouchpadDevicesUpdated(devices);
}

void DeviceDataManagerTestApi::SetUncategorizedDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::instance_->OnUncategorizedDevicesUpdated(devices);
}

}  // namespace ui
