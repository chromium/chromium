// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

#include <linux/input.h>

#include "base/logging.h"
namespace ui {
namespace {
// Used as the id to mark the value of the setting before settings were split
// per-device.
constexpr int kSharedDeviceSettingsId = -1;
}  // namespace

MouseButtonMapEvdev::MouseButtonMapEvdev() {
}

MouseButtonMapEvdev::~MouseButtonMapEvdev() {
}

void MouseButtonMapEvdev::SetPrimaryButtonRight(absl::optional<int> device_id,
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

void MouseButtonMapEvdev::EnablePerDeviceSettings() {
  enable_per_device_settings_ = true;
}

void MouseButtonMapEvdev::RemoveDeviceFromSettings(int device_id) {
  primary_button_right_map_.erase(device_id);
}

}  // namespace ui
