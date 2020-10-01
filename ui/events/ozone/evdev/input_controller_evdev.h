// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
                       MouseButtonMapEvdev* button_map);
  ~InputControllerEvdev() override;

  // Initialize device factory. This would be in the constructor if it was
  // built early enough for that to be possible.
  void SetInputDeviceFactory(
      InputDeviceFactoryEvdevProxy* input_device_factory);

  void set_has_mouse(bool has_mouse);
  void set_has_pointing_stick(bool has_pointing_stick);
  void set_has_touchpad(bool has_touchpad);

  void SetInputDevicesEnabled(bool enabled);

  // InputController:
  bool HasMouse() override;
  bool HasPointingStick() override;
  bool HasTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetCurrentLayoutByName(const std::string& layout_name) override;
  void SetTouchEventLoggingEnabled(bool enabled) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTouchpadScrollSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void SetMouseSensitivity(int value) override;
  void SetMouseScrollSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void SetMouseReverseScroll(bool enabled) override;
  void SetMouseAcceleration(bool enabled) override;
  void SuspendMouseAcceleration() override;
  void EndMouseAccelerationSuspension() override;
  void SetMouseScrollAcceleration(bool enabled) override;
  void SetTouchpadAcceleration(bool enabled) override;
  void SetTouchpadScrollAcceleration(bool enabled) override;
  void SetTapToClickPaused(bool state) override;
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override;
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override;
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

 private:
  // Post task to update settings.
  void ScheduleUpdateDeviceSettings();

  // Send settings update to input_device_factory_.
  void UpdateDeviceSettings();

  // Send caps lock update to input_device_factory_.
  void UpdateCapsLockLed();

  // Configuration that needs to be passed on to InputDeviceFactory.
  InputDeviceSettingsEvdev input_device_settings_;

  // Indicates when the mouse acceleration is turned off for PointerLock.
  bool mouse_acceleration_suspended_ = false;
  // Holds mouse acceleration setting while suspended.
  // Should only be considered a valid setting while
  // |mouse_acceleration_suspended| is true.
  bool stored_mouse_acceleration_setting_ = false;

  // Task to update config from input_device_settings_ is pending.
  bool settings_update_pending_ = false;

  // Factory for devices. Needed to update device config.
  InputDeviceFactoryEvdevProxy* input_device_factory_ = nullptr;

  // Keyboard state.
  KeyboardEvdev* const keyboard_;

  // Mouse button map.
  MouseButtonMapEvdev* const button_map_;

  // Device presence.
  bool has_mouse_ = false;
  bool has_pointing_stick_ = false;
  bool has_touchpad_ = false;

  // LED state.
  bool caps_lock_led_state_ = false;

  base::WeakPtrFactory<InputControllerEvdev> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputControllerEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_CONTROLLER_EVDEV_H_
