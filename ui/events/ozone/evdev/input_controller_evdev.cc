// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <linux/input.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

namespace ui {

InputControllerEvdev::InputControllerEvdev(KeyboardEvdev* keyboard,
                                           MouseButtonMapEvdev* button_map)
    : keyboard_(keyboard), button_map_(button_map) {}

InputControllerEvdev::~InputControllerEvdev() {
}

void InputControllerEvdev::SetInputDeviceFactory(
    InputDeviceFactoryEvdevProxy* input_device_factory) {
  input_device_factory_ = input_device_factory;

  UpdateDeviceSettings();
  UpdateCapsLockLed();
}

void InputControllerEvdev::set_has_mouse(bool has_mouse) {
  has_mouse_ = has_mouse;
}

void InputControllerEvdev::set_has_touchpad(bool has_touchpad) {
  has_touchpad_ = has_touchpad;
}

void InputControllerEvdev::SetInputDevicesEnabled(bool enabled) {
  input_device_settings_.enable_devices = enabled;
  ScheduleUpdateDeviceSettings();
}

bool InputControllerEvdev::HasMouse() {
  return has_mouse_;
}

bool InputControllerEvdev::HasTouchpad() {
  return has_touchpad_;
}

bool InputControllerEvdev::IsCapsLockEnabled() {
  return keyboard_->IsCapsLockEnabled();
}

void InputControllerEvdev::SetCapsLockEnabled(bool enabled) {
  keyboard_->SetCapsLockEnabled(enabled);
  UpdateCapsLockLed();
}

void InputControllerEvdev::SetNumLockEnabled(bool enabled) {
  // No num lock on Chrome OS.
}

bool InputControllerEvdev::IsAutoRepeatEnabled() {
  return keyboard_->IsAutoRepeatEnabled();
}

void InputControllerEvdev::SetAutoRepeatEnabled(bool enabled) {
  keyboard_->SetAutoRepeatEnabled(enabled);
}

void InputControllerEvdev::SetAutoRepeatRate(const base::TimeDelta& delay,
                                             const base::TimeDelta& interval) {
  keyboard_->SetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::GetAutoRepeatRate(base::TimeDelta* delay,
                                             base::TimeDelta* interval) {
  keyboard_->GetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::SetCurrentLayoutByName(
    const std::string& layout_name) {
  keyboard_->SetCurrentLayoutByName(layout_name);
}

void InputControllerEvdev::SetInternalTouchpadEnabled(bool enabled) {
  input_device_settings_.enable_internal_touchpad = enabled;
  ScheduleUpdateDeviceSettings();
}

bool InputControllerEvdev::IsInternalTouchpadEnabled() const {
  return input_device_settings_.enable_internal_touchpad;
}

void InputControllerEvdev::SetTouchscreensEnabled(bool enabled) {
  input_device_settings_.enable_touch_screens = enabled;
  ui::DeviceDataManager::GetInstance()->SetTouchscreensEnabled(enabled);
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchEventLoggingEnabled(bool enabled) {
  input_device_settings_.touch_event_logging_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetInternalKeyboardFilter(
    bool enable_filter,
    std::vector<DomCode> allowed_keys) {
  input_device_settings_.enable_internal_keyboard_filter = enable_filter;
  input_device_settings_.internal_keyboard_allowed_keys = allowed_keys;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadSensitivity(int value) {
  input_device_settings_.touchpad_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapToClick(bool enabled) {
  input_device_settings_.tap_to_click_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetThreeFingerClick(bool enabled) {
  input_device_settings_.three_finger_click_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapDragging(bool enabled) {
  input_device_settings_.tap_dragging_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetNaturalScroll(bool enabled) {
  input_device_settings_.natural_scroll_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseSensitivity(int value) {
  input_device_settings_.mouse_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPrimaryButtonRight(bool right) {
  button_map_->SetPrimaryButtonRight(right);
}

void InputControllerEvdev::SetMouseReverseScroll(bool enabled) {
  input_device_settings_.mouse_reverse_scroll_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseAcceleration(bool enabled) {
  input_device_settings_.mouse_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadAcceleration(bool enabled) {
  input_device_settings_.touchpad_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapToClickPaused(bool state) {
  input_device_settings_.tap_to_click_paused = state;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::GetTouchDeviceStatus(
    GetTouchDeviceStatusReply reply) {
  if (input_device_factory_)
    input_device_factory_->GetTouchDeviceStatus(std::move(reply));
  else
    std::move(reply).Run(std::string());
}

void InputControllerEvdev::GetTouchEventLog(const base::FilePath& out_dir,
                                            GetTouchEventLogReply reply) {
  if (input_device_factory_)
    input_device_factory_->GetTouchEventLog(out_dir, std::move(reply));
  else
    std::move(reply).Run(std::vector<base::FilePath>());
}

void InputControllerEvdev::GetGesturePropertiesService(
    mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver) {
  if (input_device_factory_)
    input_device_factory_->GetGesturePropertiesService(std::move(receiver));
}

void InputControllerEvdev::ScheduleUpdateDeviceSettings() {
  if (!input_device_factory_ || settings_update_pending_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&InputControllerEvdev::UpdateDeviceSettings,
                                weak_ptr_factory_.GetWeakPtr()));
  settings_update_pending_ = true;
}

void InputControllerEvdev::UpdateDeviceSettings() {
  input_device_factory_->UpdateInputDeviceSettings(input_device_settings_);
  settings_update_pending_ = false;
}

void InputControllerEvdev::UpdateCapsLockLed() {
  if (!input_device_factory_)
    return;
  bool caps_lock_state = IsCapsLockEnabled();
  if (caps_lock_state != caps_lock_led_state_)
    input_device_factory_->SetCapsLockLed(caps_lock_state);
  caps_lock_led_state_ = caps_lock_state;
}

}  // namespace ui
