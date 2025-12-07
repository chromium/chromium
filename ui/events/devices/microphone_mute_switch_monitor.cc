// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/microphone_mute_switch_monitor.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace ui {

MicrophoneMuteSwitchMonitor::MicrophoneMuteSwitchMonitor() = default;

MicrophoneMuteSwitchMonitor::~MicrophoneMuteSwitchMonitor() = default;

// static
MicrophoneMuteSwitchMonitor* MicrophoneMuteSwitchMonitor::Get() {
  static base::NoDestructor<MicrophoneMuteSwitchMonitor> instance;
  return instance.get();
}

void MicrophoneMuteSwitchMonitor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MicrophoneMuteSwitchMonitor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MicrophoneMuteSwitchMonitor::SetMicrophoneMuteSwitchValue(bool switch_on) {
  if (microphone_mute_switch_on_ == switch_on)
    return;
  microphone_mute_switch_on_ = switch_on;
  observers_.Notify(&Observer::OnMicrophoneMuteSwitchValueChanged,
                    microphone_mute_switch_on_);
}

}  // namespace ui
