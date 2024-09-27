// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_win.h"

#include <windows.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/singleton.h"

// This macro provides the implementation for the observer notification methods.
#define WIN_NOTIFY_OBSERVERS(method_decl, input_device_types)         \
  void InputDeviceObserverWin::method_decl {                          \
    observers_.Notify(                                                \
        &InputDeviceEventObserver::OnInputDeviceConfigurationChanged, \
        InputDeviceEventObserver::input_device_types);                \
  }

namespace ui {

// The registry subkey that contains information about the state of the
// detachable/convertible laptop, it tells if the device has an accessible
// keyboard. OEMs are expected to follow these guidelines to report
// docked/undocked state
// https://msdn.microsoft.com/en-us/windows/hardware/commercialize/customize/desktop/unattend/microsoft-windows-gpiobuttons-convertibleslatemode
InputDeviceObserverWin::InputDeviceObserverWin()
    : registry_key_(HKEY_LOCAL_MACHINE,
                    L"System\\CurrentControlSet\\Control\\PriorityControl",
                    KEY_NOTIFY | KEY_READ) {
  if (registry_key_.Valid()) {
    slate_mode_enabled_ = IsSlateModeEnabled();
    // Start watching the registry for changes.
    registry_key_.StartWatching(base::BindOnce(
        &InputDeviceObserverWin::OnRegistryKeyChanged, base::Unretained(this)));
  }
}

InputDeviceObserverWin* InputDeviceObserverWin::GetInstance() {
  return base::Singleton<
      InputDeviceObserverWin,
      base::LeakySingletonTraits<InputDeviceObserverWin>>::get();
}

InputDeviceObserverWin::~InputDeviceObserverWin() {}

void InputDeviceObserverWin::OnRegistryKeyChanged() {
  // |OnRegistryKeyChanged| is removed as an observer when the ChangeCallback is
  // called, so we need to re-register.
  registry_key_.StartWatching(base::BindOnce(
      &InputDeviceObserverWin::OnRegistryKeyChanged, base::Unretained(this)));

  bool new_slate_mode = IsSlateModeEnabled();
  if (slate_mode_enabled_ == new_slate_mode)
    return;

  NotifyObserversTouchpadDeviceConfigurationChanged();
  NotifyObserversKeyboardDeviceConfigurationChanged();
  slate_mode_enabled_ = new_slate_mode;
}

bool InputDeviceObserverWin::IsSlateModeEnabled() {
  DCHECK(registry_key_.Valid());
  DWORD slate_enabled = 0;
  return registry_key_.ReadValueDW(L"ConvertibleSlateMode", &slate_enabled) ==
             ERROR_SUCCESS &&
         slate_enabled == 1;
}

void InputDeviceObserverWin::AddObserver(InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void InputDeviceObserverWin::RemoveObserver(
    InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

WIN_NOTIFY_OBSERVERS(NotifyObserversKeyboardDeviceConfigurationChanged(),
                     kKeyboard)

WIN_NOTIFY_OBSERVERS(NotifyObserversTouchpadDeviceConfigurationChanged(),
                     kTouchpad)

}  // namespace ui
