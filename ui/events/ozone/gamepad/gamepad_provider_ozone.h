// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_PROVIDER_OZONE_H_
#define UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_PROVIDER_OZONE_H_

#include <set>
#include <vector>
#include "base/component_export.h"
#include "base/observer_list.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE) GamepadProviderOzone {
 public:
  // Get the GamepadProviderOzone instance.
  static GamepadProviderOzone* GetInstance();

  GamepadProviderOzone(const GamepadProviderOzone&) = delete;
  GamepadProviderOzone& operator=(const GamepadProviderOzone&) = delete;

  // Dispatch GamepadDevicesUpdate event when gamepad device is connected or
  // disconnected. This function must be called on UI thread.
  void DispatchGamepadDevicesUpdated(
      std::vector<GamepadDevice> gamepad_devices);

  // Dispatch button event when gamepad event is seen.
  // Code is the index of gamepad button or gamepad axis defined in W3C standard
  // gamepad.
  // This function must be called on UI thread.
  void DispatchGamepadEvent(const GamepadEvent& event);

  // Add observer to gamepad provider. This function must be called on UI
  // thread.
  void AddGamepadObserver(GamepadObserver* observer);

  // Remove observer from gamepad provider. This function must be called on UI
  // thread.
  void RemoveGamepadObserver(GamepadObserver* observer);

  // Get the list of currently connected gamepad devices. This function must be
  // called on UI thread.
  std::vector<GamepadDevice> GetGamepadDevices();

 private:
  GamepadProviderOzone();

  ~GamepadProviderOzone();

  // Make SingletonTraits friend to enable singleton.
  friend struct base::DefaultSingletonTraits<GamepadProviderOzone>;

  // Registered observers will receive gamepad device update event and gamepad
  // event.
  base::ObserverList<GamepadObserver>::Unchecked observers_;

  // List of current connected gamepad events.
  std::vector<GamepadDevice> gamepad_devices_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_GAMEPAD_GAMEPAD_PROVIDER_OZONE_H_
