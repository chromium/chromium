// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"

namespace ui {

// Mouse button map for Evdev.
//
// Chrome relies on the underlying OS to interpret mouse buttons. The Linux
// input subsystem does not assign any special meaning to these keys, so
// this work must happen at a higher layer (normally X11 or the console driver).
// When using evdev directly, we must do it ourselves.
//
// The mouse button map is shared between all input devices connected to the
// system.
class COMPONENT_EXPORT(EVDEV) MouseButtonMapEvdev {
 public:
  MouseButtonMapEvdev();

  MouseButtonMapEvdev(const MouseButtonMapEvdev&) = delete;
  MouseButtonMapEvdev& operator=(const MouseButtonMapEvdev&) = delete;

  ~MouseButtonMapEvdev();

  // Swaps left & right mouse buttons. If `device_id` has no value, settings are
  // configured as though per device settings are disabled.
  void SetPrimaryButtonRight(std::optional<int> device_id,
                             bool primary_button_right);

  // Return the mapped button.
  int GetMappedButton(int device_id, uint16_t button) const;

  // Removes saved mouse button settings for a given `device_id`.
  void RemoveDeviceFromSettings(int device_id);

 private:
  const bool enable_per_device_settings_;
  base::flat_map<int, bool> primary_button_right_map_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_
