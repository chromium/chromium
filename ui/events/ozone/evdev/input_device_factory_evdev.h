// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/imposter_checker_evdev.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_metrics.h"
#include "ui/events/ozone/evdev/input_device_opener.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_filter/shared_palm_detection_filter_state.h"
#include "ui/ozone/public/input_controller.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/gesture_properties_service.h"
#endif

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;
struct InProgressStylusState;
struct InProgressTouchEvdev;

#if !defined(USE_EVDEV)
#error Missing dependency on ui/events/ozone/evdev
#endif

#if defined(USE_EVDEV_GESTURES)
class GesturePropertyProvider;
#endif

// Manager for event device objects. All device I/O starts here.
class COMPONENT_EXPORT(EVDEV) InputDeviceFactoryEvdev {
 public:
  InputDeviceFactoryEvdev(
      std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
      CursorDelegateEvdev* cursor,
      std::unique_ptr<InputDeviceOpener> input_device_opener,
      InputControllerEvdev* input_controller);

  InputDeviceFactoryEvdev(const InputDeviceFactoryEvdev&) = delete;
  InputDeviceFactoryEvdev& operator=(const InputDeviceFactoryEvdev&) = delete;

  ~InputDeviceFactoryEvdev();

  // Open & start reading a newly plugged-in input device.
  void AddInputDevice(int id, const base::FilePath& path);

  // Stop reading & close an unplugged input device.
  void RemoveInputDevice(const base::FilePath& path);

  // Device thread handler for initial scan completion.
  void OnStartupScanComplete();

  // LED state.
  void SetCapsLockLed(bool enabled);

  void GetStylusSwitchState(InputController::GetStylusSwitchStateReply reply);

  // Handle gamepad force feedback effects.
  void PlayVibrationEffect(int id, uint8_t amplitude, uint16_t duration_millis);
  void StopVibration(int id);

  // Handle haptic touchpad effects.
  void PlayHapticTouchpadEffect(HapticTouchpadEffect effect,
                                HapticTouchpadEffectStrength strength);
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength);

  // Bits from InputController that have to be answered on IO.
  void UpdateInputDeviceSettings(const InputDeviceSettingsEvdev& settings);
  void GetTouchDeviceStatus(InputController::GetTouchDeviceStatusReply reply);
  void GetTouchEventLog(const base::FilePath& out_dir,
                        InputController::GetTouchEventLogReply reply);

  void GetGesturePropertiesService(
      mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver);

  // Describe internal state for system log.
  void DescribeForLog(InputController::DescribeForLogReply reply) const;

  void DisableKeyboardImposterCheck();
  void ForceReloadKeyboards();

  base::WeakPtr<InputDeviceFactoryEvdev> GetWeakPtr();

 private:
  // Open device at path & starting processing events.
  void AttachInputDevice(std::unique_ptr<EventConverterEvdev> converter);

  // Close device at path.
  void DetachInputDevice(const base::FilePath& file_path);

  // Sync input_device_settings_ to attached devices.
  void ApplyInputDeviceSettings();
  void ApplyCapsLockLed();

  // Policy for device enablement.
  bool IsDeviceEnabled(const EventConverterEvdev* converter);

  // Update observers on device changes.
  void UpdateDirtyFlags(const EventConverterEvdev* converter);
  void NotifyDevicesUpdated();
  void NotifyKeyboardsUpdated();
  void NotifyTouchscreensUpdated();
  void NotifyMouseDevicesUpdated();
  void NotifyPointingStickDevicesUpdated();
  void NotifyTouchpadDevicesUpdated();
  void NotifyGraphicsTabletDevicesUpdated();
  void NotifyGamepadDevicesUpdated();
  void NotifyUncategorizedDevicesUpdated();

  // Method used as callback to update device lists when a valid input event is
  // received on a device that is flagged as an imposter.
  void UpdateDevicesOnImposterOverride(const EventConverterEvdev* converter);

  void SetMousePropertiesPerDevice();
  void SetTouchpadPropertiesPerDevice();
  void SetPointingStickPropertiesPerDevice();

  void SetIntPropertyForOneType(const EventDeviceType type,
                                const std::string& name,
                                int value);
  void SetBoolPropertyForOneType(const EventDeviceType type,
                                 const std::string& name,
                                 bool value);
  void SetIntPropertyForOneDevice(int device_id,
                                  const std::string& name,
                                  int value);
  void SetBoolPropertyForOneDevice(int device_id,
                                   const std::string& name,
                                   bool value);
  void EnablePalmSuppression(bool enabled);
  void EnableDevices();

  void SetLatestStylusState(const InProgressTouchEvdev& event,
                            const int32_t x_res,
                            const int32_t y_res,
                            const base::TimeTicks& timestamp);
  void GetLatestStylusState(const InProgressStylusState** stylus_state) const;

  // Task runner for our thread.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Cursor movement.
  const raw_ptr<CursorDelegateEvdev> cursor_;

  // Shared Palm state.
  const std::unique_ptr<SharedPalmDetectionFilterState> shared_palm_state_;

  InputDeviceFactoryEvdevMetrics input_device_factory_metrics_;

#if defined(USE_EVDEV_GESTURES)
  // Gesture library property provider (used by touchpads/mice).
  std::unique_ptr<GesturePropertyProvider> gesture_property_provider_;
  std::unique_ptr<GesturePropertiesService> gesture_properties_service_;
#endif

  // Dispatcher for events.
  const std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher_;

  // Number of pending device additions & device classes.
  int pending_device_changes_ = 0;
  bool touchscreen_list_dirty_ = true;
  bool keyboard_list_dirty_ = true;
  bool mouse_list_dirty_ = true;
  bool pointing_stick_list_dirty_ = true;
  bool touchpad_list_dirty_ = true;
  bool graphics_tablet_list_dirty_ = true;
  bool gamepad_list_dirty_ = true;
  bool uncategorized_list_dirty_ = true;

  // Whether we have a list of devices that were present at startup.
  bool startup_devices_enumerated_ = false;

  // Whether devices that were present at startup are open.
  bool startup_devices_opened_ = false;

  // LEDs.
  bool caps_lock_led_enabled_ = false;

  // Whether touch palm suppression is enabled.
  bool palm_suppression_enabled_ = false;

  // Device settings. These primarily affect libgestures behavior.
  InputDeviceSettingsEvdev input_device_settings_;

  // Checks if a device is mis-identifying as another device.
  const std::unique_ptr<ImposterCheckerEvdev> imposter_checker_;

  // Owned per-device event converters (by path).
  // NB: This should be destroyed early, before any shared state.
  std::map<base::FilePath, std::unique_ptr<EventConverterEvdev>> converters_;

  // The latest stylus state, updated every time a stylus report comes.
  InProgressStylusState latest_stylus_state_;

  // Handles ioctl calls and creation of event converters.
  const std::unique_ptr<InputDeviceOpener> input_device_opener_;

  // Used to inform the input controller when devices are removed from the
  // system.
  raw_ptr<InputControllerEvdev> input_controller_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<InputDeviceFactoryEvdev> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_
