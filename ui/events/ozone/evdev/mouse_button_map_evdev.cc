// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

#include <linux/input.h>

#include "base/feature_list.h"
#include "base/logging.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace ui {
namespace {

// Used as the id to mark the value of the setting before settings were split
// per-device.
constexpr int kSharedDeviceSettingsId = -1;

bool ShouldEnablePerDeviceSettings() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::features::IsInputDeviceSettingsSplitEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

MouseButtonMapEvdev::MouseButtonMapEvdev()
    : enable_per_device_settings_(ShouldEnablePerDeviceSettings()) {}

MouseButtonMapEvdev::~MouseButtonMapEvdev() {
}

void MouseButtonMapEvdev::SetPrimaryButtonRight(std::optional<int> device_id,
                                                bool primary_button_right) {
  if (!enable_per_device_settings_ || !device_id.has_value()) {
    device_id = kSharedDeviceSettingsId;
  }
  primary_button_right_map_[device_id.value()] = primary_button_right;
}

int MouseButtonMapEvdev::GetMappedButton(int device_id, uint16_t button) const {
  if (!enable_per_device_settings_) {
    device_id = kSharedDeviceSettingsId;
  }
  auto iter = primary_button_right_map_.find(device_id);
  if (iter == primary_button_right_map_.end() || !iter->second) {
    return button;
  }
  if (button == BTN_LEFT)
    return BTN_RIGHT;
  if (button == BTN_RIGHT)
    return BTN_LEFT;
  return button;
}

void MouseButtonMapEvdev::RemoveDeviceFromSettings(int device_id) {
  if (!enable_per_device_settings_) {
    return;
  }

  primary_button_right_map_.erase(device_id);
}

}  // namespace ui
