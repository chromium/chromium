// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/event_switches.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"
#include "ui/events/ozone/evdev/imposter_checker_evdev.h"
#include "ui/events/ozone/evdev/imposter_checker_evdev_state.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"
#include "ui/events/ozone/evdev/microphone_mute_switch_event_converter_evdev.h"
#include "ui/events/ozone/evdev/stylus_button_event_converter_evdev.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"
#include "ui/events/ozone/features.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_feedback.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

#if defined(USE_LIBINPUT)
#include "ui/events/ozone/evdev/libinput_event_converter.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#endif

namespace ui {

namespace {

#if defined(USE_EVDEV_GESTURES)
void SetGestureIntProperty(GesturePropertyProvider* provider,
                           int id,
                           const std::string& name,
                           int value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<int> values(1, value);
    property->SetIntValue(values);
  }
}

void SetGestureBoolProperty(GesturePropertyProvider* provider,
                            int id,
                            const std::string& name,
                            bool value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<bool> values(1, value);
    property->SetBoolValue(values);
  }
}

#endif

bool IsUncategorizedDevice(const EventConverterEvdev& converter) {
  return !converter.HasTouchscreen() && !converter.HasKeyboard() &&
         !converter.HasMouse() && !converter.HasPointingStick() &&
         !converter.HasTouchpad() && !converter.HasGamepad() &&
         !converter.HasGraphicsTablet();
}

}  // namespace

InputDeviceFactoryEvdev::InputDeviceFactoryEvdev(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor,
    std::unique_ptr<InputDeviceOpener> input_device_opener,
    InputControllerEvdev* input_controller)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      cursor_(cursor),
      shared_palm_state_(new SharedPalmDetectionFilterState),
#if defined(USE_EVDEV_GESTURES)
      gesture_property_provider_(new GesturePropertyProvider),
#endif
      dispatcher_(std::move(dispatcher)),
      imposter_checker_(new ImposterCheckerEvdev()),
      input_device_opener_(std::move(input_device_opener)),
      input_controller_(input_controller) {
}

InputDeviceFactoryEvdev::~InputDeviceFactoryEvdev() = default;

void InputDeviceFactoryEvdev::AddInputDevice(int id,
                                             const base::FilePath& path) {
  OpenInputDeviceParams params;
  params.id = id;
  params.path = path;
  params.cursor = cursor_;
  params.dispatcher = dispatcher_.get();
  params.shared_palm_state = shared_palm_state_.get();

#if defined(USE_EVDEV_GESTURES)
  params.gesture_property_provider = gesture_property_provider_.get();
#endif

  std::unique_ptr<EventConverterEvdev> converter =
      input_device_opener_->OpenInputDevice(params);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::AttachInputDevice,
                     weak_ptr_factory_.GetWeakPtr(), std::move(converter)));

  ++pending_device_changes_;
}

void InputDeviceFactoryEvdev::RemoveInputDevice(const base::FilePath& path) {
  DetachInputDevice(path);
}

void InputDeviceFactoryEvdev::OnStartupScanComplete() {
  startup_devices_enumerated_ = true;
  NotifyDevicesUpdated();
}

void InputDeviceFactoryEvdev::AttachInputDevice(
    std::unique_ptr<EventConverterEvdev> converter) {
  if (converter.get()) {
    const base::FilePath& path = converter->path();

    TRACE_EVENT1("evdev", "AttachInputDevice", "path", path.value());
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    // If we have an existing device, detach it. We don't want two
    // devices with the same name open at the same time.
    if (converters_[path])
      DetachInputDevice(path);

    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasPen() &&
        base::FeatureList::IsEnabled(kEnablePalmSuppression)) {
      converter->SetPalmSuppressionCallback(
          base::BindRepeating(&InputDeviceFactoryEvdev::EnablePalmSuppression,
                              base::Unretained(this)));
    }

    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasPen()) {
      converter->SetReportStylusStateCallback(
          base::BindRepeating(&InputDeviceFactoryEvdev::SetLatestStylusState,
                              base::Unretained(this)));
    }

    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasTouchscreen() && !converter->HasPen()) {
      converter->SetGetLatestStylusStateCallback(
          base::BindRepeating(&InputDeviceFactoryEvdev::GetLatestStylusState,
                              base::Unretained(this)));
    }

    if ((converter->type() == InputDeviceType::INPUT_DEVICE_USB ||
         converter->type() == InputDeviceType::INPUT_DEVICE_BLUETOOTH) &&
        (converter->HasKeyboard() || converter->HasMouse())) {
      converter->SetReceivedValidInputCallback(base::BindRepeating(
          &InputDeviceFactoryEvdev::UpdateDevicesOnImposterOverride,
          base::Unretained(this)));
    }

    // Add initialized device to map.
    converters_[path] = std::move(converter);
    converters_[path]->Start();

    UpdateDirtyFlags(converters_[path].get());

    // Register device on physical port & get ids of devices on the same
    // physical port.
    std::vector<int> ids_to_check =
        imposter_checker_->OnDeviceAdded(converters_[path].get());
    // Check for imposters on all devices that share the same physical port.
    for (const auto& it : converters_) {
      if (base::Contains(ids_to_check, it.second->id()) &&
          imposter_checker_->FlagSuspectedImposter(it.second.get())) {
        UpdateDirtyFlags(it.second.get());
      }
    }

    input_device_factory_metrics_.OnDeviceAttach(converters_[path].get());
    // Sync settings to new device.
    ApplyInputDeviceSettings();
    ApplyCapsLockLed();
  }

  --pending_device_changes_;
  NotifyDevicesUpdated();
}

void InputDeviceFactoryEvdev::DetachInputDevice(const base::FilePath& path) {
  TRACE_EVENT1("evdev", "DetachInputDevice", "path", path.value());
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Remove device from map.
  std::unique_ptr<EventConverterEvdev> converter = std::move(converters_[path]);
  converters_.erase(path);

  if (converter) {
    // Disable the device (to release keys/buttons/etc).
    converter->SetEnabled(false);

    // Cancel libevent notifications from this converter.
    converter->Stop();

    // Notify the controller that the input device was removed.
    input_controller_->OnInputDeviceRemoved(converter->id());

    // Decrement device count on physical port. Get ids of devices on the same
    // physical port.
    std::vector<int> ids_to_check =
        imposter_checker_->OnDeviceRemoved(converter.get());
    // Check for imposters on all devices that share the same physical port.
    // Declassify any devices as no longer imposters, if the removal of this
    // device changes their status.
    for (const auto& it : converters_) {
      if (base::Contains(ids_to_check, it.second->id()) &&
          !imposter_checker_->FlagSuspectedImposter(it.second.get())) {
        UpdateDirtyFlags(it.second.get());
      }
    }

    UpdateDirtyFlags(converter.get());
    NotifyDevicesUpdated();
  }
}

void InputDeviceFactoryEvdev::GetStylusSwitchState(
    InputController::GetStylusSwitchStateReply reply) {
  for (const auto& it : converters_) {
    if (it.second->HasStylusSwitch()) {
      auto result = it.second->GetStylusSwitchState();
      std::move(reply).Run(result);
      return;
    }
  }
  std::move(reply).Run(ui::StylusState::REMOVED);
}

void InputDeviceFactoryEvdev::SetCapsLockLed(bool enabled) {
  caps_lock_led_enabled_ = enabled;
  ApplyCapsLockLed();
}

void InputDeviceFactoryEvdev::UpdateInputDeviceSettings(
    const InputDeviceSettingsEvdev& settings) {
  input_device_settings_ = settings;
  ApplyInputDeviceSettings();
}

void InputDeviceFactoryEvdev::GetTouchDeviceStatus(
    InputController::GetTouchDeviceStatusReply reply) {
  std::string status;
#if defined(USE_EVDEV_GESTURES)
  DumpTouchDeviceStatus(gesture_property_provider_.get(), &status);
#endif
  std::move(reply).Run(status);
}

void InputDeviceFactoryEvdev::GetTouchEventLog(
    const base::FilePath& out_dir,
    InputController::GetTouchEventLogReply reply) {
#if defined(USE_EVDEV_GESTURES)
  DumpTouchEventLog(converters_, gesture_property_provider_.get(), out_dir,
                    std::move(reply));
#else
  std::move(reply).Run(std::vector<base::FilePath>());
#endif
}

void InputDeviceFactoryEvdev::DescribeForLog(
    InputController::DescribeForLogReply reply) const {
  std::stringstream str;
  for (const auto& it : converters_) {
    it.second->DescribeForLog(str);
    str << std::endl;
  }
  std::move(reply).Run(str.str());
}

void InputDeviceFactoryEvdev::GetGesturePropertiesService(
    mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver) {
#if defined(USE_EVDEV_GESTURES)
  gesture_properties_service_ = std::make_unique<GesturePropertiesService>(
      gesture_property_provider_.get(), std::move(receiver));
#endif
}

base::WeakPtr<InputDeviceFactoryEvdev> InputDeviceFactoryEvdev::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void InputDeviceFactoryEvdev::SetMousePropertiesPerDevice() {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(DT_MOUSE, &ids);
  for (const int id : ids) {
    const auto& mouse_settings = input_device_settings_.GetMouseSettings(id);
    SetIntPropertyForOneDevice(id, "Pointer Sensitivity",
                               mouse_settings.sensitivity);
    SetBoolPropertyForOneDevice(id, "Pointer Acceleration",
                                input_device_settings_.suspend_acceleration
                                    ? false
                                    : mouse_settings.acceleration_enabled);
    SetIntPropertyForOneDevice(id, "Mouse Scroll Sensitivity",
                               mouse_settings.scroll_sensitivity);
    SetBoolPropertyForOneDevice(id, "Mouse Scroll Acceleration",
                                mouse_settings.scroll_acceleration_enabled);
    SetBoolPropertyForOneDevice(id, "Mouse Reverse Scrolling",
                                mouse_settings.reverse_scroll_enabled);
    // Both reverse scroll and australian scrolling need to be set to cover
    // mice with mutlitouch surfaces as well as normal scrollwheel-type mice.
    if (gesture_property_provider_->IsDeviceIdOfType(id, DT_MULTITOUCH)) {
      // If settings are not enabled per-device, use old setting mapping which
      // means mice with multitouch surfaces utilize the touchpad settings.
      // TODO(dpad): Remove if/else once per-device settings launches.
      if (input_device_settings_.enable_per_device_settings) {
        SetBoolPropertyForOneDevice(id, "Australian Scrolling",
                                    mouse_settings.reverse_scroll_enabled);
      } else {
        SetBoolPropertyForOneDevice(
            id, "Australian Scrolling",
            input_device_settings_.GetTouchpadSettings(id)
                .natural_scroll_enabled);
      }
    }
    SetBoolPropertyForOneDevice(id, "Mouse High Resolution Scrolling", true);
    SetBoolPropertyForOneDevice(id, "Output Mouse Wheel Gestures", true);
  }
#endif
}

void InputDeviceFactoryEvdev::SetTouchpadPropertiesPerDevice() {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(DT_TOUCHPAD, &ids);
  for (const int id : ids) {
    const auto& touchpad_settings =
        input_device_settings_.GetTouchpadSettings(id);
    SetIntPropertyForOneDevice(id, "Haptic Button Sensitivity",
                               touchpad_settings.haptic_click_sensitivity);
    SetIntPropertyForOneDevice(id, "Pointer Sensitivity",
                               touchpad_settings.sensitivity);
    SetIntPropertyForOneDevice(id, "Scroll Sensitivity",
                               touchpad_settings.scroll_sensitivity);
    SetBoolPropertyForOneDevice(id, "Pointer Acceleration",
                                touchpad_settings.acceleration_enabled);
    SetBoolPropertyForOneDevice(id, "Scroll Acceleration",
                                touchpad_settings.scroll_acceleration_enabled);
    SetBoolPropertyForOneDevice(id, "Australian Scrolling",
                                touchpad_settings.natural_scroll_enabled);
    SetBoolPropertyForOneDevice(id, "Tap Enable",
                                touchpad_settings.tap_to_click_enabled);
    SetBoolPropertyForOneDevice(id, "Tap Drag Enable",
                                touchpad_settings.tap_dragging_enabled);
  }
#endif
}

void InputDeviceFactoryEvdev::SetPointingStickPropertiesPerDevice() {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(DT_POINTING_STICK, &ids);
  for (const int id : ids) {
    const auto& pointing_stick_settings =
        input_device_settings_.GetPointingStickSettings(id);
    SetIntPropertyForOneDevice(id, "Pointer Sensitivity",
                               pointing_stick_settings.sensitivity);
    SetBoolPropertyForOneDevice(
        id, "Pointer Acceleration",
        input_device_settings_.suspend_acceleration
            ? false
            : pointing_stick_settings.acceleration_enabled);
    SetBoolPropertyForOneDevice(id, "Mouse High Resolution Scrolling", true);
    SetBoolPropertyForOneDevice(id, "Output Mouse Wheel Gestures", true);
  }
#endif
}

void InputDeviceFactoryEvdev::ApplyInputDeviceSettings() {
  TRACE_EVENT0("evdev", "ApplyInputDeviceSettings");

  SetMousePropertiesPerDevice();
  SetTouchpadPropertiesPerDevice();
  SetPointingStickPropertiesPerDevice();

  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Paused",
                            input_device_settings_.tap_to_click_paused);
  SetBoolPropertyForOneType(DT_TOUCHPAD, "T5R2 Three Finger Click Enable",
                            input_device_settings_.three_finger_click_enabled);
  SetBoolPropertyForOneType(
      DT_ALL, "Event Logging Enable",
      base::FeatureList::IsEnabled(ui::kEnableInputEventLogging));

  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second.get();

    bool should_be_enabled = IsDeviceEnabled(converter);
    bool was_enabled = converter->IsEnabled();
    if (should_be_enabled != was_enabled) {
      converter->SetEnabled(should_be_enabled);

      // The device was activated/deactivated we need to notify so
      // Interactions MQs can be updated.
      UpdateDirtyFlags(converter);
    }

    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasKeyboard()) {
      converter->SetKeyFilter(
          input_device_settings_.enable_internal_keyboard_filter,
          input_device_settings_.internal_keyboard_allowed_keys);
    }

    // Block modifiers on the current converter if the device id exists in
    // `input_device_settings_.blocked_modifiers_devices`
    converter->SetBlockModifiers(base::Contains(
        input_device_settings_.blocked_modifiers_devices, converter->id()));

    converter->ApplyDeviceSettings(input_device_settings_);

    converter->SetTouchEventLoggingEnabled(
        input_device_settings_.touch_event_logging_enabled);
  }

  NotifyDevicesUpdated();
}

void InputDeviceFactoryEvdev::ApplyCapsLockLed() {
  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second.get();
    converter->SetCapsLockLed(caps_lock_led_enabled_);
  }
}

void InputDeviceFactoryEvdev::PlayVibrationEffect(int id,
                                                  uint8_t amplitude,
                                                  uint16_t duration_millis) {
  for (const auto& it : converters_) {
    if (it.second->id() == id) {
      it.second->PlayVibrationEffect(amplitude, duration_millis);
      return;
    }
  }
}

void InputDeviceFactoryEvdev::StopVibration(int id) {
  for (const auto& it : converters_) {
    if (it.second->id() == id) {
      it.second->StopVibration();
      return;
    }
  }
}

void InputDeviceFactoryEvdev::PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  for (const auto& it : converters_) {
    if (it.second->HasHapticTouchpad()) {
      it.second->PlayHapticTouchpadEffect(effect, strength);
    }
  }
}

void InputDeviceFactoryEvdev::SetHapticTouchpadEffectForNextButtonRelease(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  for (const auto& it : converters_) {
    if (it.second->HasHapticTouchpad()) {
      it.second->SetHapticTouchpadEffectForNextButtonRelease(effect, strength);
    }
  }
}

bool InputDeviceFactoryEvdev::IsDeviceEnabled(
    const EventConverterEvdev* converter) {
  if (!input_device_settings_.enable_internal_touchpad &&
      converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
      converter->HasTouchpad())
    return false;

  if (!input_device_settings_.enable_touch_screens &&
      converter->HasTouchscreen())
    return false;

  if (palm_suppression_enabled_ &&
      converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
      converter->HasTouchscreen() && !converter->HasPen())
    return false;

  return input_device_settings_.enable_devices;
}

void InputDeviceFactoryEvdev::UpdateDirtyFlags(
    const EventConverterEvdev* converter) {
  if (converter->HasTouchscreen())
    touchscreen_list_dirty_ = true;

  if (converter->HasKeyboard())
    keyboard_list_dirty_ = true;

  if (converter->HasMouse())
    mouse_list_dirty_ = true;

  if (converter->HasPointingStick())
    pointing_stick_list_dirty_ = true;

  if (converter->HasTouchpad())
    touchpad_list_dirty_ = true;

  if (converter->HasGraphicsTablet()) {
    graphics_tablet_list_dirty_ = true;
  }

  if (converter->HasGamepad())
    gamepad_list_dirty_ = true;

  if (IsUncategorizedDevice(*converter))
    uncategorized_list_dirty_ = true;
}

void InputDeviceFactoryEvdev::NotifyDevicesUpdated() {
  if (!startup_devices_enumerated_ || pending_device_changes_)
    return;  // No update until full scan done and no pending operations.
  if (touchscreen_list_dirty_)
    NotifyTouchscreensUpdated();
  if (keyboard_list_dirty_)
    NotifyKeyboardsUpdated();
  if (mouse_list_dirty_)
    NotifyMouseDevicesUpdated();
  if (pointing_stick_list_dirty_)
    NotifyPointingStickDevicesUpdated();
  if (touchpad_list_dirty_)
    NotifyTouchpadDevicesUpdated();
  if (graphics_tablet_list_dirty_) {
    NotifyGraphicsTabletDevicesUpdated();
  }
  if (gamepad_list_dirty_)
    NotifyGamepadDevicesUpdated();
  if (uncategorized_list_dirty_)
    NotifyUncategorizedDevicesUpdated();
  if (!startup_devices_opened_) {
    dispatcher_->DispatchDeviceListsComplete();
    startup_devices_opened_ = true;
  }
  touchscreen_list_dirty_ = false;
  keyboard_list_dirty_ = false;
  mouse_list_dirty_ = false;
  pointing_stick_list_dirty_ = false;
  touchpad_list_dirty_ = false;
  graphics_tablet_list_dirty_ = false;
  gamepad_list_dirty_ = false;
  uncategorized_list_dirty_ = false;
}

void InputDeviceFactoryEvdev::NotifyTouchscreensUpdated() {
  std::vector<TouchscreenDevice> touchscreens;
  bool has_stylus_switch = false;

  // Check if there is a stylus garage/dock presence detection switch
  // among the devices. The internal touchscreen controller is not currently
  // responsible for exposing this device, it usually is a gpio-keys
  // device only containing the single switch.

  for (const auto& it : converters_) {
    if (it.second->HasStylusSwitch()) {
      has_stylus_switch = true;
      break;
    }
  }

  for (const auto& it : converters_) {
    if (it.second->HasTouchscreen()) {
      touchscreens.emplace_back(
          it.second->input_device(), it.second->GetTouchscreenSize(),
          it.second->GetTouchPoints(), it.second->HasPen(),
          it.second->type() == ui::InputDeviceType::INPUT_DEVICE_INTERNAL &&
              has_stylus_switch);
    }
  }

  dispatcher_->DispatchTouchscreenDevicesUpdated(touchscreens);
}

void InputDeviceFactoryEvdev::NotifyKeyboardsUpdated() {
  base::flat_map<int, std::vector<uint64_t>> key_bits_mapping;
  std::vector<KeyboardDevice> keyboards;
  for (auto& converter : converters_) {
    if (converter.second->HasKeyboard()) {
      keyboards.emplace_back(converter.second->input_device(),
                             converter.second->HasAssistantKey(),
                             converter.second->HasFunctionKey());
      key_bits_mapping[converter.second->id()] =
          converter.second->GetKeyboardKeyBits();
    }
  }
  dispatcher_->DispatchKeyboardDevicesUpdated(keyboards,
                                              std::move(key_bits_mapping));
}

void InputDeviceFactoryEvdev::NotifyMouseDevicesUpdated() {
  std::vector<InputDevice> mice;
  bool has_mouse = false;
  for (auto& converter : converters_) {
    if (converter.second->HasMouse()) {
      mice.push_back(converter.second->input_device());

      // If the device also has a keyboard, clear the keyboard suspected
      // imposter field as it only applies to the keyboard
      // `InputDevice` struct.
      if (converter.second->HasKeyboard()) {
        mice.back().suspected_keyboard_imposter = false;
      }

      // Some I2C touchpads falsely claim to be mice, see b/205272718
      if (converter.second->type() !=
          ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
        has_mouse = true;
      }
    }
  }

  dispatcher_->DispatchMouseDevicesUpdated(mice, has_mouse);
}

void InputDeviceFactoryEvdev::NotifyPointingStickDevicesUpdated() {
  std::vector<InputDevice> pointing_sticks;
  for (auto& converter : converters_) {
    if (converter.second->HasPointingStick()) {
      pointing_sticks.push_back(converter.second->input_device());

      // If the device also has a keyboard, clear the keyboard suspected
      // imposter field as it only applies to the keyboard
      // `InputDevice` struct.
      if (converter.second->HasKeyboard()) {
        pointing_sticks.back().suspected_keyboard_imposter = false;
      }
    }
  }

  dispatcher_->DispatchPointingStickDevicesUpdated(pointing_sticks);
}

void InputDeviceFactoryEvdev::NotifyTouchpadDevicesUpdated() {
  std::vector<TouchpadDevice> touchpads;
  bool has_haptic_touchpad = false;
  for (const auto& it : converters_) {
    if (it.second->HasTouchpad()) {
      if (it.second->HasHapticTouchpad())
        has_haptic_touchpad = true;
      touchpads.emplace_back(it.second->input_device(),
                             it.second->HasHapticTouchpad());

      // If the device also has a keyboard, clear the keyboard suspected
      // imposter field as it only applies to the keyboard
      // `InputDevice` struct.
      if (it.second->HasKeyboard()) {
        touchpads.back().suspected_keyboard_imposter = false;
      }
    }
  }

  dispatcher_->DispatchTouchpadDevicesUpdated(touchpads, has_haptic_touchpad);
}

void InputDeviceFactoryEvdev::NotifyGraphicsTabletDevicesUpdated() {
  std::vector<InputDevice> graphics_tablets;
  for (const auto& it : converters_) {
    if (it.second->HasGraphicsTablet()) {
      graphics_tablets.push_back(it.second->input_device());
    }
  }

  dispatcher_->DispatchGraphicsTabletDevicesUpdated(graphics_tablets);
}

void InputDeviceFactoryEvdev::NotifyGamepadDevicesUpdated() {
  base::flat_map<int, std::vector<uint64_t>> key_bits_mapping;
  std::vector<GamepadDevice> gamepads;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasGamepad()) {
      gamepads.emplace_back(it->second->input_device(),
                            it->second->GetGamepadAxes(),
                            it->second->GetGamepadRumbleCapability());
      key_bits_mapping[it->second->id()] = it->second->GetGamepadKeyBits();
    }
  }

  dispatcher_->DispatchGamepadDevicesUpdated(gamepads, std::move(key_bits_mapping));
}

void InputDeviceFactoryEvdev::NotifyUncategorizedDevicesUpdated() {
  std::vector<InputDevice> uncategorized_devices;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (IsUncategorizedDevice(*(it->second)))
      uncategorized_devices.push_back(it->second->input_device());
  }

  dispatcher_->DispatchUncategorizedDevicesUpdated(uncategorized_devices);
}

void InputDeviceFactoryEvdev::UpdateDevicesOnImposterOverride(
    const EventConverterEvdev* converter) {
  UpdateDirtyFlags(converter);
  NotifyDevicesUpdated();
}

void InputDeviceFactoryEvdev::SetBoolPropertyForOneDevice(
    int device_id,
    const std::string& name,
    bool value) {
#if defined(USE_EVDEV_GESTURES)
  SetGestureBoolProperty(gesture_property_provider_.get(), device_id, name,
                         value);
#endif
}

void InputDeviceFactoryEvdev::SetIntPropertyForOneDevice(
    int device_id,
    const std::string& name,
    int value) {
#if defined(USE_EVDEV_GESTURES)
  SetGestureIntProperty(gesture_property_provider_.get(), device_id, name,
                        value);
#endif
}

void InputDeviceFactoryEvdev::SetIntPropertyForOneType(
    const EventDeviceType type,
    const std::string& name,
    int value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureIntProperty(gesture_property_provider_.get(), ids[i], name,
                          value);
  }
#endif
  // In the future, we may add property setting codes for other non-gesture
  // devices. One example would be keyboard settings.
  // TODO(sheckylin): See http://crbug.com/398518 for example.
}

void InputDeviceFactoryEvdev::SetBoolPropertyForOneType(
    const EventDeviceType type,
    const std::string& name,
    bool value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureBoolProperty(gesture_property_provider_.get(), ids[i], name,
                           value);
  }
#endif
}

void InputDeviceFactoryEvdev::EnablePalmSuppression(bool enabled) {
  if (enabled == palm_suppression_enabled_)
    return;
  palm_suppression_enabled_ = enabled;

  // This function can be called while disabling pen devices, so don't disable
  // inline here.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::EnableDevices,
                                weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceFactoryEvdev::EnableDevices() {
  // TODO(spang): Fix the UI to not dismiss menus when we use
  // ApplyInputDeviceSettings() instead of this function.
  for (const auto& it : converters_)
    it.second->SetEnabled(IsDeviceEnabled(it.second.get()));
}

void InputDeviceFactoryEvdev::DisableKeyboardImposterCheck() {
  ImposterCheckerEvdevState::Get().SetKeyboardCheckEnabled(/*enabled=*/false);
  ForceReloadKeyboards();
}

void InputDeviceFactoryEvdev::ForceReloadKeyboards() {
  for (const auto& it : converters_) {
    if (it.second->HasKeyboard()) {
      imposter_checker_->FlagSuspectedImposter(it.second.get());
      UpdateDirtyFlags(it.second.get());
    }
  }
  NotifyDevicesUpdated();
}

void InputDeviceFactoryEvdev::SetLatestStylusState(
    const InProgressTouchEvdev& event,
    const int32_t x_res,
    const int32_t y_res,
    const base::TimeTicks& timestamp) {
  // TODO(alanlxl): Copy happens here. This function may be called very
  // frequently because the firmware reports stylus status every few ms.
  // Comments it out for the timebeing until it's really used.
  // latest_stylus_state_.stylus_event = event;

  if (x_res <= 0) {
    VLOG(1) << "Invalid resolution " << x_res;
    latest_stylus_state_.x_res = 1;
  } else {
    latest_stylus_state_.x_res = x_res;
  }

  if (y_res <= 0) {
    VLOG(1) << "Invalid resolution " << y_res;
    latest_stylus_state_.y_res = 1;
  } else {
    latest_stylus_state_.y_res = y_res;
  }

  if (timestamp < latest_stylus_state_.timestamp) {
    VLOG(1) << "Unexpected decreased timestamp received.";
  }

  latest_stylus_state_.timestamp = timestamp;
}

void InputDeviceFactoryEvdev::GetLatestStylusState(
    const InProgressStylusState** stylus_state) const {
  *stylus_state = &latest_stylus_state_;
}

}  // namespace ui
