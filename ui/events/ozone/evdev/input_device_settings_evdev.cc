// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

#include "base/feature_list.h"
#include "ui/events/ozone/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace ui {
namespace {

// Used to denote the global instance of settings within the maps which is used
// when per device settings are disabled.
constexpr int kSharedSettingsDeviceId = -1;

bool ShouldEnablePerDeviceSettings() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::features::IsInputDeviceSettingsSplitEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

InputDeviceSettingsEvdev::InputDeviceSettingsEvdev()
    : enable_per_device_settings(ShouldEnablePerDeviceSettings()) {
  touch_event_logging_enabled =
      base::FeatureList::IsEnabled(ui::kEnableInputEventLogging);
}
InputDeviceSettingsEvdev::InputDeviceSettingsEvdev(
    const InputDeviceSettingsEvdev& input_device_settings) = default;
InputDeviceSettingsEvdev::~InputDeviceSettingsEvdev() = default;

void InputDeviceSettingsEvdev::RemoveDeviceFromSettings(int device_id) {
  touchpad_settings_.erase(device_id);
  pointing_stick_settings_.erase(device_id);
  mouse_settings_.erase(device_id);
}

TouchpadSettingsEvdev& InputDeviceSettingsEvdev::GetTouchpadSettings() {
  return touchpad_settings_[kSharedSettingsDeviceId];
}

MouseSettingsEvdev& InputDeviceSettingsEvdev::GetMouseSettings() {
  return mouse_settings_[kSharedSettingsDeviceId];
}

PointingStickSettingsEvdev&
InputDeviceSettingsEvdev::GetPointingStickSettings() {
  return pointing_stick_settings_[kSharedSettingsDeviceId];
}

const TouchpadSettingsEvdev& InputDeviceSettingsEvdev::GetTouchpadSettings()
    const {
  return touchpad_settings_[kSharedSettingsDeviceId];
}

const MouseSettingsEvdev& InputDeviceSettingsEvdev::GetMouseSettings() const {
  return mouse_settings_[kSharedSettingsDeviceId];
}

const PointingStickSettingsEvdev&
InputDeviceSettingsEvdev::GetPointingStickSettings() const {
  return pointing_stick_settings_[kSharedSettingsDeviceId];
}

TouchpadSettingsEvdev& InputDeviceSettingsEvdev::GetTouchpadSettings(
    int device_id) {
  if (!enable_per_device_settings) {
    return GetTouchpadSettings();
  }
  return touchpad_settings_[device_id];
}

MouseSettingsEvdev& InputDeviceSettingsEvdev::GetMouseSettings(int device_id) {
  if (!enable_per_device_settings) {
    return GetMouseSettings();
  }
  return mouse_settings_[device_id];
}

PointingStickSettingsEvdev& InputDeviceSettingsEvdev::GetPointingStickSettings(
    int device_id) {
  if (!enable_per_device_settings) {
    return GetPointingStickSettings();
  }
  return pointing_stick_settings_[device_id];
}

const TouchpadSettingsEvdev& InputDeviceSettingsEvdev::GetTouchpadSettings(
    int device_id) const {
  if (!enable_per_device_settings) {
    return GetTouchpadSettings();
  }
  return touchpad_settings_[device_id];
}

const MouseSettingsEvdev& InputDeviceSettingsEvdev::GetMouseSettings(
    int device_id) const {
  if (!enable_per_device_settings) {
    return GetMouseSettings();
  }
  return mouse_settings_[device_id];
}

const PointingStickSettingsEvdev&
InputDeviceSettingsEvdev::GetPointingStickSettings(int device_id) const {
  if (!enable_per_device_settings) {
    return GetPointingStickSettings();
  }
  return pointing_stick_settings_[device_id];
}

TouchpadSettingsEvdev::TouchpadSettingsEvdev() = default;
TouchpadSettingsEvdev::TouchpadSettingsEvdev(
    const TouchpadSettingsEvdev& touchpad_settings) = default;
TouchpadSettingsEvdev::~TouchpadSettingsEvdev() = default;

}  // namespace ui
