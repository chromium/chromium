// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_

#include <vector>

namespace ui {
struct InputDevice;
struct KeyboardDevice;
struct TouchpadDevice;
struct TouchscreenDevice;

enum class StylusState;

// Test interfaces for calling private functions of DeviceDataManager.
//
// Usage depends upon exactly what you want to do, but often times you will
// configure the set of devices (keyboards and/or touchscreens) and then call
// OnDeviceListsComplete().
class DeviceDataManagerTestApi {
 public:
  DeviceDataManagerTestApi();

  DeviceDataManagerTestApi(const DeviceDataManagerTestApi&) = delete;
  DeviceDataManagerTestApi& operator=(const DeviceDataManagerTestApi&) = delete;

  ~DeviceDataManagerTestApi();

  void NotifyObserversDeviceListsComplete();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversMouseDeviceConfigurationChanged();
  void NotifyObserversPointingStickDeviceConfigurationChanged();
  void NotifyObserversStylusStateChanged(StylusState stylus_state);
  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void OnDeviceListsComplete();

  void SetKeyboardDevices(const std::vector<KeyboardDevice>& devices);
  void SetGraphicsTabletDevices(const std::vector<InputDevice>& devices);
  void SetMouseDevices(const std::vector<InputDevice>& devices);
  void SetPointingStickDevices(const std::vector<InputDevice>& devices);
  void SetTouchpadDevices(const std::vector<TouchpadDevice>& devices);
  void SetUncategorizedDevices(const std::vector<InputDevice>& devices);

  // |are_touchscreen_target_displays_valid| is only applicable to
  // InputDeviceClient. See
  // InputDeviceClient::OnTouchscreenDeviceConfigurationChanged() for details.
  void SetTouchscreenDevices(
      const std::vector<TouchscreenDevice>& devices,
      bool are_touchscreen_target_displays_valid = false);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_
