// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_EVDEV_H_

#include "ui/events/ozone/evdev/input_device_opener.h"

#include "base/files/scoped_file.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

class COMPONENT_EXPORT(EVDEV) InputDeviceOpenerEvdev
    : public InputDeviceOpener {
 public:
  InputDeviceOpenerEvdev() = default;

  InputDeviceOpenerEvdev(const InputDeviceOpenerEvdev&) = delete;
  InputDeviceOpenerEvdev& operator=(const InputDeviceOpenerEvdev&) = delete;

  ~InputDeviceOpenerEvdev() override = default;

  std::unique_ptr<EventConverterEvdev> OpenInputDevice(
      const OpenInputDeviceParams& params) override;
};
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_OPENER_EVDEV_H_
