// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

class InputDeviceFactoryEvdevProxy;
class KeyboardEvdev;
class MouseButtonMapEvdev;

// Ozone InputController implementation for the Linux input subsystem ("evdev").
class COMPONENT_EXPORT(EVDEV) InputControllerEvdev : public InputController {
 public:
  InputControllerEvdev(KeyboardEvdev* keyboard,
                       MouseButtonMapEvdev* mouse_button_map,
                       MouseButtonMapEvdev* pointing_stick_button_map);

  InputControllerEvdev(const InputControllerEvdev&) = delete;
  InputControllerEvdev& operator=(const InputControllerEvdev&) = delete;

  ~InputControllerEvdev() override;

  // Initialize device factory. This would be in the constructor if it was
  // built early enough for that to be possible.
  void SetInputDeviceFactory(
      InputDeviceFactoryEvdevProxy* input_device_factory);

  void set_has_mouse(bool has_mouse);
  void set_has_pointing_stick(bool has_pointing_stick);
  void set_has_touchpad(bool has_touchpad);
  void set_has_haptic_touchpad(bool has_haptic_touchpad);
  void set_any_keys_pressed(bool any);

  // InputController:
  bool HasMouse() override;
  bool HasPointingStick() override;
  bool HasTouchpad() override;
  bool HasHapticTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback) override;
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override;
  void SetTouchEventLoggingEnabled(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTouchpadSensitivity(std::optional<int> device_id, int value) override;
  void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                    int value) override;
  void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                 bool enabled) override;
  void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                         int value) override;
  void SetTapToClick(std::optional<int> device_id, bool enabled) override;
  void SetTapDragging(std::optional<int> device_id, bool enabled) override;
  void SetNaturalScroll(std::optional<int> device_id, bool enabled) override;
  void SetMouseSensitivity(std::optional<int> device_id, int value) override;
  void SetMouseScrollSensitivity(std::optional<int> device_id,
                                 int value) override;
  void SetMouseReverseScroll(std::optional<int> device_id,
                             bool enabled) override;
  void SetMouseAcceleration(std::optional<int> device_id,
                            bool enabled) override;
  void SetMouseScrollAcceleration(std::optional<int> device_id,
                                  bool enabled) override;
  void SetPointingStickSensitivity(std::optional<int> device_id,
                                   int value) override;
  void SetPointingStickAcceleration(std::optional<int> device_id,
                                    bool enabled) override;
  void SetTouchpadAcceleration(std::optional<int> device_id,
                               bool enabled) override;
  void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                     bool enabled) override;
  void SetPrimaryButtonRight(std::optional<int> device_id, bool right) override;
  void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                          bool right) override;
  void SuspendMouseAcceleration() override;
  void EndMouseAccelerationSuspension() override;
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetGamepadKeyBits(int id) override;
  void SetTapToClickPaused(bool state) override;
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override;
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override;
  void DescribeForLog(DescribeForLogReply reply) const override;
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  bool IsInternalTouchpadEnabled() const override;
  void SetTouchscreensEnabled(bool enabled) override;
  void SetInternalKeyboardFilter(bool enable_filter,
                                 std::vector<DomCode> allowed_keys) override;
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver)
      override;
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override;
  void StopVibration(int id) override;
  void PlayHapticTouchpadEffect(HapticTouchpadEffect effect,
                                HapticTouchpadEffectStrength strength) override;
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) override;
  bool AreAnyKeysPressed() override;
  void BlockModifiersOnDevices(std::vector<int> device_ids) override;
  bool AreInputDevicesEnabled() const override;
  std::unique_ptr<ScopedDisableInputDevices> DisableInputDevices() override;
  void DisableKeyboardImposterCheck() override;

  // Notifies the controller to delete any data for the given `device_id`.
  void OnInputDeviceRemoved(int device_id);

  // Configuration that needs to be passed on to InputDeviceFactory.
  InputDeviceSettingsEvdev GetInputDeviceSettings() const;

 private:
  class ScopedDisableInputDevicesImpl;

  // Called by `ScopedDisableInputDevicesImpl` when they are created/destroyed.
  void OnScopedDisableInputDevicesCreated();
  void OnScopedDisableInputDevicesDestroyed();

  // Post task to update settings.
  void ScheduleUpdateDeviceSettings();

  // Send settings update to input_device_factory_.
  void UpdateDeviceSettings();

  // Send caps lock update to input_device_factory_.
  void UpdateCapsLockLed();

  // Configuration that needs to be passed on to InputDeviceFactory.
  InputDeviceSettingsEvdev input_device_settings_;

  // Amount of `ScopedDisableInputDevicesImpl` currently alive.
  int num_scoped_input_devices_disablers_ = 0;

  // Task to update config from input_device_settings_ is pending.
  bool settings_update_pending_ = false;

  // Factory for devices. Needed to update device config.
  raw_ptr<InputDeviceFactoryEvdevProxy> input_device_factory_ = nullptr;

  // Keyboard state.
  const raw_ptr<KeyboardEvdev> keyboard_;

  // Keyboard keybits.
  base::flat_map<int, std::vector<uint64_t>> keyboard_key_bits_mapping_;

  // Mouse button map.
  const raw_ptr<MouseButtonMapEvdev> mouse_button_map_;

  // Pointing stick button map.
  const raw_ptr<MouseButtonMapEvdev> pointing_stick_button_map_;

  // Gamepad keybits.
  base::flat_map<int, std::vector<uint64_t>> gamepad_key_bits_mapping_;

  // Device presence.
  bool has_mouse_ = false;
  bool has_pointing_stick_ = false;
  bool has_touchpad_ = false;
  // if has_haptic_touchpad_ is true, then has_touchpad_ is also true.
  bool has_haptic_touchpad_ = false;

  bool any_keys_are_pressed_ = false;

  // LED state.
  bool caps_lock_led_state_ = false;

  base::WeakPtrFactory<InputControllerEvdev> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
