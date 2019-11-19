// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {

class DeviceDataManagerTest;
class InputDeviceEventObserver;

// Keeps track of device mappings and event transformations.
class EVENTS_DEVICES_EXPORT DeviceDataManager
    : public DeviceHotplugEventObserver {
 public:
  static const int kMaxDeviceNum = 128;
  ~DeviceDataManager() override;

  static void CreateInstance();
  static void DeleteInstance();
  static DeviceDataManager* GetInstance();
  static bool HasInstance();

  // Configures the touch devices. |transforms| contains the transform for each
  // device and display pair.
  void ConfigureTouchDevices(
      const std::vector<ui::TouchDeviceTransform>& transforms);

  void ApplyTouchTransformer(int touch_device_id, float* x, float* y);

  // Gets the display that touches from |touch_device_id| should be sent to.
  int64_t GetTargetDisplayForTouchDevice(int touch_device_id) const;

  void ApplyTouchRadiusScale(int touch_device_id, double* radius);

  void SetTouchscreensEnabled(bool enabled);

  const std::vector<TouchscreenDevice>& GetTouchscreenDevices() const;
  const std::vector<InputDevice>& GetKeyboardDevices() const;
  const std::vector<InputDevice>& GetMouseDevices() const;
  const std::vector<InputDevice>& GetTouchpadDevices() const;

  // Returns all the uncategorized input devices, which means input devices
  // besides keyboards, touchscreens, mice and touchpads.
  const std::vector<InputDevice>& GetUncategorizedDevices() const;
  bool AreDeviceListsComplete() const;
  bool AreTouchscreensEnabled() const;

  // Returns true if the |target_display_id| of the TouchscreenDevices returned
  // from GetTouchscreenDevices() is valid.
  bool AreTouchscreenTargetDisplaysValid() const;

  void AddObserver(InputDeviceEventObserver* observer);
  void RemoveObserver(InputDeviceEventObserver* observer);

 protected:
  DeviceDataManager();

  // DeviceHotplugEventObserver:
  void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override;
  void OnKeyboardDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnTouchpadDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnDeviceListsComplete() override;
  void OnStylusStateChanged(StylusState state) override;

 private:
  friend class DeviceDataManagerTest;
  friend class DeviceDataManagerTestApi;

  void ClearTouchDeviceAssociations();
  void UpdateTouchInfoFromTransform(
      const ui::TouchDeviceTransform& touch_device_transform);
  void UpdateTouchMap();

  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversMouseDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void NotifyObserversUncategorizedDeviceConfigurationChanged();
  void NotifyObserversDeviceListsComplete();
  void NotifyObserversStylusStateChanged(StylusState stylus_state);

  static DeviceDataManager* instance_;

  std::vector<TouchscreenDevice> touchscreen_devices_;
  std::vector<InputDevice> keyboard_devices_;
  std::vector<InputDevice> mouse_devices_;
  std::vector<InputDevice> touchpad_devices_;
  std::vector<InputDevice> uncategorized_devices_;
  bool device_lists_complete_ = false;

  base::ObserverList<InputDeviceEventObserver>::Unchecked observers_;

  bool touch_screens_enabled_ = true;

  // Set to true when ConfigureTouchDevices() is called.
  bool are_touchscreen_target_displays_valid_ = false;

  // Contains touchscreen device info for each device mapped by device ID.
  base::flat_map<int, TouchDeviceTransform> touch_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDataManager);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
