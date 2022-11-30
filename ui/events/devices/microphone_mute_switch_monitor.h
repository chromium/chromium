// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_MICROPHONE_MUTE_SWITCH_MONITOR_H_
#define UI_EVENTS_DEVICES_MICROPHONE_MUTE_SWITCH_MONITOR_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/events/devices/events_devices_export.h"

namespace ui {

// Monitors the state of the Microphone mute switch device - Chrome OS devices
// may have a hardware toggle that disables audio input. The toggle state is
// exposed using the SW_MUTE_DEVICE event on an input device.
// MicrophoneMuteSwitchMonitor can be used to track the state of the microphone
// mute switch. The switch state will be updated by the input device's event
// converter, and can be observed using the Observer interface exposed by the
// MicrophoneMuteSwitchMonitor.
//
// NOTE: The mute switch state will be monitored only on Chrome OS with ozone.
class EVENTS_DEVICES_EXPORT MicrophoneMuteSwitchMonitor {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the microphone mute switch value changes.
    virtual void OnMicrophoneMuteSwitchValueChanged(bool muted) = 0;
  };

  MicrophoneMuteSwitchMonitor();
  ~MicrophoneMuteSwitchMonitor();
  MicrophoneMuteSwitchMonitor(const MicrophoneMuteSwitchMonitor&) = delete;
  MicrophoneMuteSwitchMonitor& operator=(const MicrophoneMuteSwitchMonitor&) =
      delete;

  static MicrophoneMuteSwitchMonitor* Get();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool microphone_mute_switch_on() const { return microphone_mute_switch_on_; }

  // Updates the microphone mute switch value. Set in response to a
  // SW_MUTE_DEVICE event on the input device associated with the microphone
  // mute switch.
  void SetMicrophoneMuteSwitchValue(bool switch_on);

 private:
  friend class base::NoDestructor<MicrophoneMuteSwitchMonitor>;

  // Whether the microphone mute switch was toggled to on, in which case the
  // internal microphone will be muted.
  bool microphone_mute_switch_on_ = false;

  base::ObserverList<MicrophoneMuteSwitchMonitor::Observer> observers_;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_MICROPHONE_MUTE_SWITCH_MONITOR_H_
