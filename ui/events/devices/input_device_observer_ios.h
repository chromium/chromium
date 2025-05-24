// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_IOS_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_IOS_H_

#include "base/observer_list.h"
#include "build/blink_buildflags.h"
#include "ui/events/devices/input_device_event_observer.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// This class is a singleton responsible to notify InputDeviceChangeObserver
// whenever an input change happened on the IOS side.
class EVENTS_DEVICES_EXPORT InputDeviceObserverIOS {
 public:
  static InputDeviceObserverIOS* GetInstance();

  InputDeviceObserverIOS(const InputDeviceObserverIOS&) = delete;
  InputDeviceObserverIOS& operator=(const InputDeviceObserverIOS&) = delete;

  ~InputDeviceObserverIOS();

  void AddObserver(ui::InputDeviceEventObserver* observer);
  void RemoveObserver(ui::InputDeviceEventObserver* observer);
  void NotifyObserversDeviceConfigurationChanged(bool has_mouse_device);
  bool GetHasMouseDevice() { return has_mouse_device_; }

 private:
  InputDeviceObserverIOS();

  bool has_mouse_device_{false};
  base::ObserverList<ui::InputDeviceEventObserver>::Unchecked observers_;

  friend struct base::DefaultSingletonTraits<InputDeviceObserverIOS>;
  friend class InputDeviceObserverIOSTest;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_IOS_H_
