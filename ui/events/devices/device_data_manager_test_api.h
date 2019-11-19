// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_

#include <vector>

#include "base/macros.h"

namespace ui {
struct InputDevice;
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
  ~DeviceDataManagerTestApi();

  void NotifyObserversDeviceListsComplete();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversStylusStateChanged(StylusState stylus_state);
  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void OnDeviceListsComplete();

  void SetKeyboardDevices(const std::vector<InputDevice>& devices);
  void SetMouseDevices(const std::vector<InputDevice>& devices);
  void SetTouchpadDevices(const std::vector<InputDevice>& devices);
  void SetUncategorizedDevices(const std::vector<InputDevice>& devices);

  // |are_touchscreen_target_displays_valid| is only applicable to
  // InputDeviceClient. See
  // InputDeviceClient::OnTouchscreenDeviceConfigurationChanged() for details.
  void SetTouchscreenDevices(
      const std::vector<TouchscreenDevice>& devices,
      bool are_touchscreen_target_displays_valid = false);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDataManagerTestApi);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_TEST_API_H_
