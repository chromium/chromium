// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_WIN_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_WIN_H_

#include "base/observer_list.h"
#include "base/win/registry.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

class EVENTS_DEVICES_EXPORT InputDeviceObserverWin final {
 public:
  static InputDeviceObserverWin* GetInstance();

  InputDeviceObserverWin(const InputDeviceObserverWin&) = delete;
  InputDeviceObserverWin& operator=(const InputDeviceObserverWin&) = delete;

  ~InputDeviceObserverWin();

  void AddObserver(InputDeviceEventObserver* observer);
  void RemoveObserver(InputDeviceEventObserver* observer);

 private:
  friend struct base::DefaultSingletonTraits<InputDeviceObserverWin>;

  InputDeviceObserverWin();

  void OnRegistryKeyChanged();
  bool IsSlateModeEnabled();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();

  bool slate_mode_enabled_ = false;
  base::win::RegKey registry_key_;
  base::ObserverList<InputDeviceEventObserver>::Unchecked observers_;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_WIN_H_
