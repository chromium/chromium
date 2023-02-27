// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_

#include <vector>

namespace ui {

enum class DomCode;

constexpr int kDefaultSensitivity = 3;

struct MouseSettingsEvdev {
  // The initial settings are not critical since they will be shortly be changed
  // to the user's preferences or the application's own defaults.
  bool reverse_scroll_enabled = false;
  bool acceleration_enabled = true;
  bool scroll_acceleration_enabled = true;
  int sensitivity = kDefaultSensitivity;
  int scroll_sensitivity = kDefaultSensitivity;
};

struct TouchpadSettingsEvdev {
  TouchpadSettingsEvdev();
  TouchpadSettingsEvdev(const TouchpadSettingsEvdev&);
  ~TouchpadSettingsEvdev();

  // The initial settings are not critical since they will be shortly be changed
  // to the user's preferences or the application's own defaults.
  bool tap_to_click_enabled = true;
  bool tap_dragging_enabled = false;
  bool natural_scroll_enabled = false;
  bool acceleration_enabled = true;
  bool scroll_acceleration_enabled = true;
  bool haptic_feedback_enabled = true;
  int sensitivity = kDefaultSensitivity;
  int scroll_sensitivity = kDefaultSensitivity;
  int haptic_click_sensitivity = kDefaultSensitivity;
};

struct PointingStickSettingsEvdev {
  // The initial settings are not critical since they will be shortly be changed
  // to the user's preferences or the application's own defaults.
  bool acceleration_enabled = true;
  int sensitivity = kDefaultSensitivity;
};

struct InputDeviceSettingsEvdev {
  InputDeviceSettingsEvdev();
  InputDeviceSettingsEvdev(const InputDeviceSettingsEvdev&);
  ~InputDeviceSettingsEvdev();

  TouchpadSettingsEvdev touchpad_settings;
  MouseSettingsEvdev mouse_settings;
  PointingStickSettingsEvdev pointing_stick_settings;

  // Pausing of tap to click applies to all touchpad devices.
  bool tap_to_click_paused = false;
  // Three finger click applies to all touchpad devices.
  bool three_finger_click_enabled = false;
  bool touch_event_logging_enabled = false;
  bool enable_devices = true;  // If false, all input is disabled.
  bool enable_internal_touchpad = true;
  bool enable_touch_screens = true;
  bool enable_internal_keyboard_filter = false;
  std::vector<DomCode> internal_keyboard_allowed_keys;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
