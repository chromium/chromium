// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
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
  for (auto& observer : observers_) {
    observer.OnGamepadDevicesUpdated();
  }
}

void GamepadProviderOzone::DispatchGamepadEvent(const GamepadEvent& event) {
  for (auto& observer : observers_) {
    observer.OnGamepadEvent(event);
  }
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
