// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/switches.h"

namespace ui {

// Enables logic to detect microphone mute switch device state, which disables
// internal audio input when toggled.
constexpr char kEnableMicrophoneMuteSwitchDeviceSwitch[] =
    "enable-microphone-mute-switch-device";

}  // namespace ui
