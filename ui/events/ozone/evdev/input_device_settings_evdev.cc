// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

#include "base/feature_list.h"
#include "ui/events/ozone/features.h"

namespace ui {

InputDeviceSettingsEvdev::InputDeviceSettingsEvdev() {
  touch_event_logging_enabled =
      base::FeatureList::IsEnabled(ui::kEnableInputEventLogging);
}

InputDeviceSettingsEvdev::InputDeviceSettingsEvdev(
    const InputDeviceSettingsEvdev& other) = default;

InputDeviceSettingsEvdev::~InputDeviceSettingsEvdev() {
}

}  // namespace ui
