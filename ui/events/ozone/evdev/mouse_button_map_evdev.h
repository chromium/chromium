// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_

#include <stdint.h>

#include "base/component_export.h"

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

  // Swaps left & right mouse buttons.
  void SetPrimaryButtonRight(bool primary_button_right);

  // Return the mapped button.
  int GetMappedButton(uint16_t button) const;

 private:
  bool primary_button_right_ = false;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_MAP_EVDEV_H_
