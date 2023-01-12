// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <linux/input.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

namespace ui {

InputControllerEvdev::InputControllerEvdev(
    KeyboardEvdev* keyboard,
    MouseButtonMapEvdev* mouse_button_map,
    MouseButtonMapEvdev* pointing_stick_button_map)
    : keyboard_(keyboard),
      mouse_button_map_(mouse_button_map),
      pointing_stick_button_map_(pointing_stick_button_map) {}

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

void InputControllerEvdev::set_has_pointing_stick(bool has_pointing_stick) {
  has_pointing_stick_ = has_pointing_stick;
}

void InputControllerEvdev::set_has_touchpad(bool has_touchpad) {
  has_touchpad_ = has_touchpad;
}

void InputControllerEvdev::set_has_haptic_touchpad(bool has_haptic_touchpad) {
  has_haptic_touchpad_ = has_haptic_touchpad;
}

void InputControllerEvdev::SetInputDevicesEnabled(bool enabled) {
  input_device_settings_.enable_devices = enabled;
  ScheduleUpdateDeviceSettings();
}

bool InputControllerEvdev::HasMouse() {
  return has_mouse_;
}

bool InputControllerEvdev::HasPointingStick() {
  return has_pointing_stick_;
}

bool InputControllerEvdev::HasTouchpad() {
  return has_touchpad_;
}

bool InputControllerEvdev::HasHapticTouchpad() {
  return has_haptic_touchpad_;
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

void InputControllerEvdev::SetKeyboardKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {
  keyboard_key_bits_mapping_ = std::move(key_bits_mapping);
}

std::vector<uint64_t> InputControllerEvdev::GetKeyboardKeyBits(int id) {
  auto key_bits_mapping_iter = keyboard_key_bits_mapping_.find(id);
  return key_bits_mapping_iter == keyboard_key_bits_mapping_.end()
             ? std::vector<uint64_t>()
             : key_bits_mapping_iter->second;
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

void InputControllerEvdev::SetTouchpadScrollSensitivity(int value) {
  input_device_settings_.touchpad_scroll_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadHapticFeedback(bool enabled) {
  input_device_settings_.touchpad_haptic_feedback_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadHapticClickSensitivity(int value) {
  input_device_settings_.touchpad_haptic_click_sensitivity = value;
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

void InputControllerEvdev::SetMouseScrollSensitivity(int value) {
  input_device_settings_.mouse_scroll_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPointingStickSensitivity(int value) {
  input_device_settings_.pointing_stick_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPointingStickAcceleration(bool enabled) {
  if (is_mouse_acceleration_suspended()) {
    stored_acceleration_settings_->pointing_stick = enabled;
    return;
  }
  input_device_settings_.pointing_stick_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetGamepadKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {
  gamepad_key_bits_mapping_ = std::move(key_bits_mapping);
}

std::vector<uint64_t> InputControllerEvdev::GetGamepadKeyBits(int id) {
  auto gamepad_key_bits_iter = gamepad_key_bits_mapping_.find(id);
  return gamepad_key_bits_iter == gamepad_key_bits_mapping_.end()
             ? std::vector<uint64_t>()
             : gamepad_key_bits_iter->second;
}

void InputControllerEvdev::SetPrimaryButtonRight(bool right) {
  mouse_button_map_->SetPrimaryButtonRight(right);
}

void InputControllerEvdev::SetPointingStickPrimaryButtonRight(bool right) {
  pointing_stick_button_map_->SetPrimaryButtonRight(right);
}

void InputControllerEvdev::SetMouseReverseScroll(bool enabled) {
  input_device_settings_.mouse_reverse_scroll_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseAcceleration(bool enabled) {
  if (is_mouse_acceleration_suspended()) {
    stored_acceleration_settings_->mouse = enabled;
    return;
  }
  input_device_settings_.mouse_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SuspendMouseAcceleration() {
  // multiple calls to suspend are currently not supported.
  DCHECK(!is_mouse_acceleration_suspended());
  stored_acceleration_settings_ =
      std::make_unique<StoredAccelerationSettings>();
  stored_acceleration_settings_->mouse =
      input_device_settings_.mouse_acceleration_enabled;
  stored_acceleration_settings_->pointing_stick =
      input_device_settings_.pointing_stick_acceleration_enabled;
  input_device_settings_.mouse_acceleration_enabled = false;
  input_device_settings_.pointing_stick_acceleration_enabled = false;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::EndMouseAccelerationSuspension() {
  auto stored_settings = std::move(stored_acceleration_settings_);
  SetMouseAcceleration(stored_settings->mouse);
  SetPointingStickAcceleration(stored_settings->pointing_stick);
}

void InputControllerEvdev::SetMouseScrollAcceleration(bool enabled) {
  input_device_settings_.mouse_scroll_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadAcceleration(bool enabled) {
  input_device_settings_.touchpad_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadScrollAcceleration(bool enabled) {
  input_device_settings_.touchpad_scroll_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapToClickPaused(bool state) {
  input_device_settings_.tap_to_click_paused = state;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::GetStylusSwitchState(
    GetStylusSwitchStateReply reply) {
  if (input_device_factory_)
    input_device_factory_->GetStylusSwitchState(std::move(reply));
  else
    std::move(reply).Run(ui::StylusState::REMOVED);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

void InputControllerEvdev::PlayVibrationEffect(int id,
                                               uint8_t amplitude,
                                               uint16_t duration_millis) {
  if (!input_device_factory_)
    return;
  input_device_factory_->PlayVibrationEffect(id, amplitude, duration_millis);
}

void InputControllerEvdev::StopVibration(int id) {
  if (!input_device_factory_)
    return;
  input_device_factory_->StopVibration(id);
}

void InputControllerEvdev::PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  if (!input_device_factory_)
    return;
  input_device_factory_->PlayHapticTouchpadEffect(effect, strength);
}

void InputControllerEvdev::SetHapticTouchpadEffectForNextButtonRelease(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  if (!input_device_factory_)
    return;
  input_device_factory_->SetHapticTouchpadEffectForNextButtonRelease(effect,
                                                                     strength);
}

}  // namespace ui
