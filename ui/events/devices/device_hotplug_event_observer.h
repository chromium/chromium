// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_
#define UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_

#include <vector>

#include "ui/events/devices/events_devices_export.h"

namespace ui {

struct InputDevice;
struct KeyboardDevice;
enum class StylusState;
struct TouchpadDevice;
struct TouchscreenDevice;

// Listener for specific input device hotplug events.
class EVENTS_DEVICES_EXPORT DeviceHotplugEventObserver {
 public:
  virtual ~DeviceHotplugEventObserver() {}

  // On a hotplug event this is called with the list of available touchscreen
  // devices. The set of touchscreen devices may not have changed.
  virtual void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available keyboard
  // devices. The set of keyboard devices may not have changed.
  virtual void OnKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available mice. The set
  // of mice may not have changed.
  virtual void OnMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available pointing
  // sticks. The set of pointing sticks may not have changed.
  virtual void OnPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available touchpads. The
  // set of touchpads may not have changed.
  virtual void OnTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices) = 0;

  // On a hotplug event this is called with the list of available graphics
  // tablets. The set of graphics tablets may not have changed.
  virtual void OnGraphicsTabletDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;

  // On a hotplug event this is called with the list of the available
  // uncategorized input devices, which means not touchscreens, keyboards, mice
  // and touchpads.
  virtual void OnUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;

  // On completion of the initial startup scan. This means all of the above
  // OnDevicesUpdated() methods have been called with a complete list.
  virtual void OnDeviceListsComplete() = 0;

  // The stylus was removed or inserted into the device; |state| contains the
  // new stylus state.
  virtual void OnStylusStateChanged(StylusState state) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_HOTPLUG_EVENT_OBSERVER_H_
