// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_

#include <stdint.h>

#include "ui/events/devices/events_devices_export.h"

namespace ui {

enum class StylusState;

// DeviceDataManager observer used to announce input hotplug events.
class EVENTS_DEVICES_EXPORT InputDeviceEventObserver {
 public:
  // Bitfields for input device types to update through
  // |OnInputDeviceConfigurationChanged|.
  static constexpr uint8_t kKeyboard = 1 << 0;
  static constexpr uint8_t kMouse = 1 << 1;
  static constexpr uint8_t kTouchpad = 1 << 2;
  static constexpr uint8_t kTouchscreen = 1 << 3;
  static constexpr uint8_t kUncategorized = 1 << 4;

  virtual ~InputDeviceEventObserver() {}

  // This method is called for configurations changes in the device types
  // specified in |input_device_types| bit-field.
  virtual void OnInputDeviceConfigurationChanged(uint8_t input_device_types) {}

  virtual void OnDeviceListsComplete() {}
  virtual void OnStylusStateChanged(StylusState state) {}

  // Called when ConfigureTouchDevices() is called. This indicates the
  // transform, scale and/or device<->display mapping has changed.
  virtual void OnTouchDeviceAssociationChanged() {}

 protected:
  InputDeviceEventObserver() {}
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_EVENT_OBSERVER_H_
