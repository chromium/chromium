// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_

#include <vector>

namespace ui {

enum class DomCode;

struct InputDeviceSettingsEvdev {
  InputDeviceSettingsEvdev();
  InputDeviceSettingsEvdev(const InputDeviceSettingsEvdev& other);
  ~InputDeviceSettingsEvdev();

  static const int kDefaultSensitivity = 3;

  // The initial settings are not critical since they will be shortly be changed
  // to the user's preferences or the application's own defaults.
  bool tap_to_click_enabled = true;
  bool three_finger_click_enabled = false;
  bool tap_dragging_enabled = false;
  bool natural_scroll_enabled = false;
  bool tap_to_click_paused = false;
  bool touch_event_logging_enabled = false;
  bool mouse_reverse_scroll_enabled = false;
  bool mouse_acceleration_enabled = true;
  bool mouse_scroll_acceleration_enabled = true;
  bool pointing_stick_acceleration_enabled = true;
  bool touchpad_acceleration_enabled = true;
  bool touchpad_scroll_acceleration_enabled = true;
  bool touchpad_haptic_feedback_enabled = true;

  int touchpad_sensitivity = kDefaultSensitivity;
  int touchpad_scroll_sensitivity = kDefaultSensitivity;
  int touchpad_haptic_click_sensitivity = kDefaultSensitivity;
  int mouse_sensitivity = kDefaultSensitivity;
  int mouse_scroll_sensitivity = kDefaultSensitivity;
  int pointing_stick_sensitivity = kDefaultSensitivity;

  bool enable_devices = true;  // If false, all input is disabled.
  bool enable_internal_touchpad = true;
  bool enable_touch_screens = true;
  bool enable_internal_keyboard_filter = false;
  std::vector<DomCode> internal_keyboard_allowed_keys;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_SETTINGS_EVDEV_H_
