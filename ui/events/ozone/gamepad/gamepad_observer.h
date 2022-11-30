// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_OBSERVER_H_
#define UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_OBSERVER_H_

#include "ui/events/ozone/gamepad/gamepad_event.h"

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE) GamepadObserver {
 public:
  // Callback function when connected gamepad devices is updated.
  virtual void OnGamepadDevicesUpdated() {}

  // Callback function on gamepad.
  virtual void OnGamepadEvent(const GamepadEvent& event) {}

  virtual ~GamepadObserver() {}
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_OBSERVER_H_
