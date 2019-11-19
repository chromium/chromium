// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_feedback.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#endif

namespace ui {

namespace {

struct OpenInputDeviceParams {
  // Unique identifier for the new device.
  int id;

  // Device path to open.
  base::FilePath path;

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher;

  // State shared between devices.
  CursorDelegateEvdev* cursor;
#if defined(USE_EVDEV_GESTURES)
  GesturePropertyProvider* gesture_property_provider;
#endif
  SharedPalmDetectionFilterState* shared_palm_state;
};

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

std::unique_ptr<EventConverterEvdev> CreateConverter(
    const OpenInputDeviceParams& params,
    base::ScopedFD fd,
    const EventDeviceInfo& devinfo) {
#if defined(USE_EVDEV_GESTURES)
  // Touchpad or mouse: use gestures library.
  // EventReaderLibevdevCros -> GestureInterpreterLibevdevCros -> DispatchEvent
  if (devinfo.HasTouchpad() || devinfo.HasMouse()) {
    std::unique_ptr<GestureInterpreterLibevdevCros> gesture_interp =
        std::make_unique<GestureInterpreterLibevdevCros>(
            params.id, params.cursor, params.gesture_property_provider,
            params.dispatcher);
    return std::make_unique<EventReaderLibevdevCros>(std::move(fd), params.path,
                                                     params.id, devinfo,
                                                     std::move(gesture_interp));
  }
#endif

  // Touchscreen: use TouchEventConverterEvdev.
  if (devinfo.HasTouchscreen()) {
    std::unique_ptr<TouchEventConverterEvdev> converter(
        new TouchEventConverterEvdev(std::move(fd), params.path, params.id,
                                     devinfo, params.shared_palm_state,
                                     params.dispatcher));
    converter->Initialize(devinfo);
    return std::move(converter);
  }

  // Graphics tablet
  if (devinfo.HasTablet()) {
    return base::WrapUnique<EventConverterEvdev>(new TabletEventConverterEvdev(
        std::move(fd), params.path, params.id, params.cursor, devinfo,
        params.dispatcher));
  }

  if (devinfo.HasGamepad()) {
    return base::WrapUnique<EventConverterEvdev>(new GamepadEventConverterEvdev(
        std::move(fd), params.path, params.id, devinfo, params.dispatcher));
  }

  // Everything else: use EventConverterEvdevImpl.
  return base::WrapUnique<EventConverterEvdevImpl>(
      new EventConverterEvdevImpl(std::move(fd), params.path, params.id,
                                  devinfo, params.cursor, params.dispatcher));
}

// Open an input device and construct an EventConverterEvdev.
std::unique_ptr<EventConverterEvdev> OpenInputDevice(
    const OpenInputDeviceParams& params) {
  const base::FilePath& path = params.path;
  TRACE_EVENT1("evdev", "OpenInputDevice", "path", path.value());

  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    PLOG(ERROR) << "Cannot open " << path.value();
    return nullptr;
  }

  // Use monotonic timestamps for events. The touch code in particular
  // expects event timestamps to correlate to the monotonic clock
  // (base::TimeTicks).
  unsigned int clk = CLOCK_MONOTONIC;
  if (ioctl(fd.get(), EVIOCSCLOCKID, &clk))
    PLOG(ERROR) << "failed to set CLOCK_MONOTONIC";

  EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device information for " << path.value();
    return nullptr;
  }

  return CreateConverter(params, std::move(fd), devinfo);
}

bool IsUncategorizedDevice(const EventConverterEvdev& converter) {
  return !converter.HasTouchscreen() && !converter.HasKeyboard() &&
         !converter.HasMouse() && !converter.HasTouchpad() &&
         !converter.HasGamepad();
}

}  // namespace

InputDeviceFactoryEvdev::InputDeviceFactoryEvdev(
    std::unique_ptr<DeviceEventDispatcherEvdev> dispatcher,
    CursorDelegateEvdev* cursor)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      cursor_(cursor),
      shared_palm_state_(new SharedPalmDetectionFilterState),
#if defined(USE_EVDEV_GESTURES)
      gesture_property_provider_(new GesturePropertyProvider),
#endif
      dispatcher_(std::move(dispatcher)) {
}

InputDeviceFactoryEvdev::~InputDeviceFactoryEvdev() {
}

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

  std::unique_ptr<EventConverterEvdev> converter = OpenInputDevice(params);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
        converter->HasPen()) {
      converter->SetPalmSuppressionCallback(
          base::BindRepeating(&InputDeviceFactoryEvdev::EnablePalmSuppression,
                              base::Unretained(this)));
    }

    // Add initialized device to map.
    converters_[path] = std::move(converter);
    converters_[path]->Start();
    UpdateDirtyFlags(converters_[path].get());

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

    UpdateDirtyFlags(converter.get());
    NotifyDevicesUpdated();
  }
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

void InputDeviceFactoryEvdev::ApplyInputDeviceSettings() {
  TRACE_EVENT0("evdev", "ApplyInputDeviceSettings");

  SetIntPropertyForOneType(DT_TOUCHPAD, "Pointer Sensitivity",
                           input_device_settings_.touchpad_sensitivity);
  SetIntPropertyForOneType(DT_TOUCHPAD, "Scroll Sensitivity",
                           input_device_settings_.touchpad_sensitivity);
  SetBoolPropertyForOneType(
      DT_TOUCHPAD, "Pointer Acceleration",
      input_device_settings_.touchpad_acceleration_enabled);

  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Enable",
                            input_device_settings_.tap_to_click_enabled);
  SetBoolPropertyForOneType(DT_TOUCHPAD, "T5R2 Three Finger Click Enable",
                            input_device_settings_.three_finger_click_enabled);
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Drag Enable",
                            input_device_settings_.tap_dragging_enabled);

  SetBoolPropertyForOneType(DT_MULTITOUCH, "Australian Scrolling",
                            input_device_settings_.natural_scroll_enabled);

  SetIntPropertyForOneType(DT_MOUSE, "Pointer Sensitivity",
                           input_device_settings_.mouse_sensitivity);
  SetIntPropertyForOneType(DT_MOUSE, "Scroll Sensitivity",
                           input_device_settings_.mouse_sensitivity);
  SetBoolPropertyForOneType(DT_MOUSE, "Pointer Acceleration",
                            input_device_settings_.mouse_acceleration_enabled);
  SetBoolPropertyForOneType(
      DT_MOUSE, "Mouse Reverse Scrolling",
      input_device_settings_.mouse_reverse_scroll_enabled);

  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Paused",
                            input_device_settings_.tap_to_click_paused);

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

  if (converter->HasTouchpad())
    touchpad_list_dirty_ = true;

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
  if (touchpad_list_dirty_)
    NotifyTouchpadDevicesUpdated();
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
  touchpad_list_dirty_ = false;
  gamepad_list_dirty_ = false;
  uncategorized_list_dirty_ = false;
}

void InputDeviceFactoryEvdev::NotifyTouchscreensUpdated() {
  std::vector<TouchscreenDevice> touchscreens;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasTouchscreen()) {
      touchscreens.emplace_back(
          it->second->input_device(), it->second->GetTouchscreenSize(),
          it->second->GetTouchPoints(), it->second->HasPen());
    }
  }

  dispatcher_->DispatchTouchscreenDevicesUpdated(touchscreens);
}

void InputDeviceFactoryEvdev::NotifyKeyboardsUpdated() {
  std::vector<InputDevice> keyboards;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasKeyboard()) {
      keyboards.push_back(InputDevice(it->second->input_device()));
    }
  }

  dispatcher_->DispatchKeyboardDevicesUpdated(keyboards);
}

void InputDeviceFactoryEvdev::NotifyMouseDevicesUpdated() {
  std::vector<InputDevice> mice;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasMouse()) {
      mice.push_back(it->second->input_device());
    }
  }

  dispatcher_->DispatchMouseDevicesUpdated(mice);
}

void InputDeviceFactoryEvdev::NotifyTouchpadDevicesUpdated() {
  std::vector<InputDevice> touchpads;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasTouchpad()) {
      touchpads.push_back(it->second->input_device());
    }
  }

  dispatcher_->DispatchTouchpadDevicesUpdated(touchpads);
}

void InputDeviceFactoryEvdev::NotifyGamepadDevicesUpdated() {
  std::vector<GamepadDevice> gamepads;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasGamepad()) {
      gamepads.emplace_back(it->second->input_device(),
                            it->second->GetGamepadAxes());
    }
  }

  dispatcher_->DispatchGamepadDevicesUpdated(gamepads);
}

void InputDeviceFactoryEvdev::NotifyUncategorizedDevicesUpdated() {
  std::vector<InputDevice> uncategorized_devices;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (IsUncategorizedDevice(*(it->second)))
      uncategorized_devices.push_back(it->second->input_device());
  }

  dispatcher_->DispatchUncategorizedDevicesUpdated(uncategorized_devices);
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::EnableDevices,
                                weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceFactoryEvdev::EnableDevices() {
  // TODO(spang): Fix the UI to not dismiss menus when we use
  // ApplyInputDeviceSettings() instead of this function.
  for (const auto& it : converters_)
    it.second->SetEnabled(IsDeviceEnabled(it.second.get()));
}

}  // namespace ui
