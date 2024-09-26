// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"

namespace ui {

GamepadProviderOzone::GamepadProviderOzone() {}

GamepadProviderOzone::~GamepadProviderOzone() {}

GamepadProviderOzone* GamepadProviderOzone::GetInstance() {
  // GamepadProviderOzone is not holding any important resource. It's best to be
  // leaky to reduce shutdown time.
  return base::Singleton<
      GamepadProviderOzone,
      base::LeakySingletonTraits<GamepadProviderOzone>>::get();
}

void GamepadProviderOzone::DispatchGamepadDevicesUpdated(
    std::vector<GamepadDevice> gamepad_devices) {
  gamepad_devices_.swap(gamepad_devices);
  observers_.Notify(&GamepadObserver::OnGamepadDevicesUpdated);
}

void GamepadProviderOzone::DispatchGamepadEvent(const GamepadEvent& event) {
  observers_.Notify(&GamepadObserver::OnGamepadEvent, event);
}

void GamepadProviderOzone::AddGamepadObserver(GamepadObserver* observer) {
  observers_.AddObserver(observer);
}

void GamepadProviderOzone::RemoveGamepadObserver(GamepadObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<GamepadDevice> GamepadProviderOzone::GetGamepadDevices() {
  return gamepad_devices_;
}
}  // namespace ui
