// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {

class DeviceDataManagerTest;
class InputDeviceEventObserver;

// Keeps track of device mappings and event transformations.
class EVENTS_DEVICES_EXPORT DeviceDataManager
    : public DeviceHotplugEventObserver {
 public:
  static const int kMaxDeviceNum = 128;

  DeviceDataManager(const DeviceDataManager&) = delete;
  DeviceDataManager& operator=(const DeviceDataManager&) = delete;

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
  const std::vector<KeyboardDevice>& GetKeyboardDevices() const;
  const std::vector<InputDevice>& GetMouseDevices() const;
  const std::vector<InputDevice>& GetPointingStickDevices() const;
  const std::vector<TouchpadDevice>& GetTouchpadDevices() const;
  const std::vector<InputDevice>& GetGraphicsTabletDevices() const;

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
  bool HasObserver(InputDeviceEventObserver* observer);

  // Resets all device lists and |device_lists_complete_|. This method exists
  // because the DeviceDataManager instance is created early in test suite setup
  // and is hard to replace for tests that require a fresh one.
  void ResetDeviceListsForTest();

 protected:
  DeviceDataManager();

  // DeviceHotplugEventObserver:
  void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override;
  void OnKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) override;
  void OnMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices) override;
  void OnGraphicsTabletDevicesUpdated(
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
  void NotifyObserversPointingStickDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void NotifyObserversGraphicsTabletDeviceConfigurationChanged();
  void NotifyObserversUncategorizedDeviceConfigurationChanged();
  void NotifyObserversDeviceListsComplete();
  void NotifyObserversStylusStateChanged(StylusState stylus_state);

  static DeviceDataManager* instance_;

  std::vector<TouchscreenDevice> touchscreen_devices_;
  std::vector<KeyboardDevice> keyboard_devices_;
  std::vector<InputDevice> mouse_devices_;
  std::vector<InputDevice> pointing_stick_devices_;
  std::vector<TouchpadDevice> touchpad_devices_;
  std::vector<InputDevice> graphics_tablet_devices_;
  std::vector<InputDevice> uncategorized_devices_;
  bool device_lists_complete_ = false;

  base::ObserverList<InputDeviceEventObserver>::Unchecked observers_;

  bool touch_screens_enabled_ = true;

  // Set to true when ConfigureTouchDevices() is called.
  bool are_touchscreen_target_displays_valid_ = false;

  // Contains touchscreen device info for each device mapped by device ID.
  base::flat_map<int, TouchDeviceTransform> touch_map_;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
