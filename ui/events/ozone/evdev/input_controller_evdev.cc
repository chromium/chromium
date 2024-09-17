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
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

namespace ui {
namespace {

TouchpadSettingsEvdev& GetTouchpadSettings(InputDeviceSettingsEvdev& settings,
                                           std::optional<int> device_id) {
  if (!device_id.has_value()) {
    return settings.GetTouchpadSettings();
  }
  return settings.GetTouchpadSettings(device_id.value());
}

MouseSettingsEvdev& GetMouseSettings(InputDeviceSettingsEvdev& settings,
                                     std::optional<int> device_id) {
  if (!device_id.has_value()) {
    return settings.GetMouseSettings();
  }
  return settings.GetMouseSettings(device_id.value());
}

PointingStickSettingsEvdev& GetPointingStickSettings(
    InputDeviceSettingsEvdev& settings,
    std::optional<int> device_id) {
  if (!device_id.has_value()) {
    return settings.GetPointingStickSettings();
  }
  return settings.GetPointingStickSettings(device_id.value());
}

}  // namespace

class InputControllerEvdev::ScopedDisableInputDevicesImpl
    : public ScopedDisableInputDevices {
 public:
  explicit ScopedDisableInputDevicesImpl(
      base::WeakPtr<InputControllerEvdev> parent)
      : parent_(parent) {
    parent_->OnScopedDisableInputDevicesCreated();
  }

  ~ScopedDisableInputDevicesImpl() override {
    if (parent_) {
      parent_->OnScopedDisableInputDevicesDestroyed();
    }
  }

 private:
  base::WeakPtr<InputControllerEvdev> parent_;
};

InputControllerEvdev::InputControllerEvdev(
    KeyboardEvdev* keyboard,
    MouseButtonMapEvdev* mouse_button_map,
    MouseButtonMapEvdev* pointing_stick_button_map)
    : keyboard_(keyboard),
      mouse_button_map_(mouse_button_map),
      pointing_stick_button_map_(pointing_stick_button_map) {}

InputControllerEvdev::~InputControllerEvdev() = default;

void InputControllerEvdev::SetInputDeviceFactory(
    InputDeviceFactoryEvdevProxy* input_device_factory) {
  input_device_factory_ = input_device_factory;

  UpdateDeviceSettings();
  UpdateCapsLockLed();
}

void InputControllerEvdev::set_has_mouse(bool has_mouse) {
  has_mouse_ = has_mouse;
}
void InputControllerEvdev::set_any_keys_pressed(bool any) {
  any_keys_are_pressed_ = any;
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

void InputControllerEvdev::OnScopedDisableInputDevicesCreated() {
  num_scoped_input_devices_disablers_++;
  if (num_scoped_input_devices_disablers_ == 1) {
    input_device_settings_.enable_devices = false;
    ScheduleUpdateDeviceSettings();
  }
}

void InputControllerEvdev::OnScopedDisableInputDevicesDestroyed() {
  num_scoped_input_devices_disablers_--;
  if (num_scoped_input_devices_disablers_ == 0) {
    input_device_settings_.enable_devices = true;
    ScheduleUpdateDeviceSettings();
  }
}

bool InputControllerEvdev::AreInputDevicesEnabled() const {
  return input_device_settings_.enable_devices;
}

std::unique_ptr<ScopedDisableInputDevices>
InputControllerEvdev::DisableInputDevices() {
  return std::make_unique<ScopedDisableInputDevicesImpl>(
      weak_ptr_factory_.GetWeakPtr());
}

void InputControllerEvdev::DisableKeyboardImposterCheck() {
  input_device_factory_->DisableKeyboardImposterCheck();
}

InputDeviceSettingsEvdev InputControllerEvdev::GetInputDeviceSettings() const {
  return input_device_settings_;
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
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  keyboard_->SetCurrentLayoutByName(layout_name, std::move(callback));
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

void InputControllerEvdev::SetThreeFingerClick(bool enabled) {
  input_device_settings_.three_finger_click_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadSensitivity(std::optional<int> device_id,
                                                  int value) {
  GetTouchpadSettings(input_device_settings_, device_id).sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadAcceleration(std::optional<int> device_id,
                                                   bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id).acceleration_enabled =
      enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id)
      .scroll_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadScrollSensitivity(
    std::optional<int> device_id,
    int value) {
  GetTouchpadSettings(input_device_settings_, device_id).scroll_sensitivity =
      value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadHapticFeedback(
    std::optional<int> device_id,
    bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id)
      .haptic_feedback_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTouchpadHapticClickSensitivity(
    std::optional<int> device_id,
    int value) {
  GetTouchpadSettings(input_device_settings_, device_id)
      .haptic_click_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapToClick(std::optional<int> device_id,
                                         bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id).tap_to_click_enabled =
      enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapDragging(std::optional<int> device_id,
                                          bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id).tap_dragging_enabled =
      enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetNaturalScroll(std::optional<int> device_id,
                                            bool enabled) {
  GetTouchpadSettings(input_device_settings_, device_id)
      .natural_scroll_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseSensitivity(std::optional<int> device_id,
                                               int value) {
  GetMouseSettings(input_device_settings_, device_id).sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseScrollSensitivity(
    std::optional<int> device_id,
    int value) {
  GetMouseSettings(input_device_settings_, device_id).scroll_sensitivity =
      value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {
  GetMouseSettings(input_device_settings_, device_id)
      .scroll_acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPointingStickSensitivity(
    std::optional<int> device_id,
    int value) {
  GetPointingStickSettings(input_device_settings_, device_id).sensitivity =
      value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseReverseScroll(std::optional<int> device_id,
                                                 bool enabled) {
  GetMouseSettings(input_device_settings_, device_id).reverse_scroll_enabled =
      enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPointingStickAcceleration(
    std::optional<int> device_id,
    bool enabled) {
  GetPointingStickSettings(input_device_settings_, device_id)
      .acceleration_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseAcceleration(std::optional<int> device_id,
                                                bool enabled) {
  GetMouseSettings(input_device_settings_, device_id).acceleration_enabled =
      enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPrimaryButtonRight(std::optional<int> device_id,
                                                 bool right) {
  mouse_button_map_->SetPrimaryButtonRight(device_id, right);
}

void InputControllerEvdev::SetPointingStickPrimaryButtonRight(
    std::optional<int> device_id,
    bool right) {
  pointing_stick_button_map_->SetPrimaryButtonRight(device_id, right);
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

void InputControllerEvdev::SuspendMouseAcceleration() {
  input_device_settings_.suspend_acceleration = true;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::EndMouseAccelerationSuspension() {
  input_device_settings_.suspend_acceleration = false;
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

void InputControllerEvdev::DescribeForLog(
    InputController::DescribeForLogReply reply) const {
  if (input_device_factory_) {
    input_device_factory_->DescribeForLog(std::move(reply));
  } else {
    std::move(reply).Run(std::string());
  }
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

void InputControllerEvdev::OnInputDeviceRemoved(int device_id) {
  input_device_settings_.RemoveDeviceFromSettings(device_id);
  // Mouse button map and pointing stick map can be null in tests.
  if (mouse_button_map_) {
    mouse_button_map_->RemoveDeviceFromSettings(device_id);
  }
  if (pointing_stick_button_map_) {
    pointing_stick_button_map_->RemoveDeviceFromSettings(device_id);
  }
  ScheduleUpdateDeviceSettings();
}

bool InputControllerEvdev::AreAnyKeysPressed() {
  return any_keys_are_pressed_;
}

void InputControllerEvdev::BlockModifiersOnDevices(
    std::vector<int> device_ids) {
  input_device_settings_.blocked_modifiers_devices = device_ids;
  ScheduleUpdateDeviceSettings();
}

}  // namespace ui
