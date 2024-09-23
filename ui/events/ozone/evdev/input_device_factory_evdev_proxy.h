// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_PROXY_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_PROXY_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

enum class DomCode : uint32_t;
class InputDeviceFactoryEvdev;
struct InputDeviceSettingsEvdev;

// Thread safe proxy for InputDeviceFactoryEvdev.
//
// This is used on the UI thread to proxy calls to the real object on
// the device I/O thread.
class COMPONENT_EXPORT(EVDEV) InputDeviceFactoryEvdevProxy {
 public:
  InputDeviceFactoryEvdevProxy(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<InputDeviceFactoryEvdev> input_device_factory);

  InputDeviceFactoryEvdevProxy(const InputDeviceFactoryEvdevProxy&) = delete;
  InputDeviceFactoryEvdevProxy& operator=(const InputDeviceFactoryEvdevProxy&) =
      delete;

  ~InputDeviceFactoryEvdevProxy();

  // See InputDeviceFactoryEvdev for docs. These calls simply forward to
  // that object on another thread.
  void AddInputDevice(int id, const base::FilePath& path);
  void RemoveInputDevice(const base::FilePath& path);
  void OnStartupScanComplete();
  void SetCapsLockLed(bool enabled);
  void GetStylusSwitchState(InputController::GetStylusSwitchStateReply reply);
  void SetTouchEventLoggingEnabled(bool enabled);
  void UpdateInputDeviceSettings(const InputDeviceSettingsEvdev& settings);
  void GetTouchDeviceStatus(InputController::GetTouchDeviceStatusReply reply);
  void GetTouchEventLog(const base::FilePath& out_dir,
                        InputController::GetTouchEventLogReply reply);
  void DescribeForLog(InputController::DescribeForLogReply reply) const;
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver);
  void PlayVibrationEffect(int id, uint8_t amplitude, uint16_t duration_millis);
  void StopVibration(int id);
  void PlayHapticTouchpadEffect(HapticTouchpadEffect effect,
                                HapticTouchpadEffectStrength strength);
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength);
  void DisableKeyboardImposterCheck();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<InputDeviceFactoryEvdev> input_device_factory_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_PROXY_H_
