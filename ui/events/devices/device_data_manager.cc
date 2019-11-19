// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_data_manager.h"

#include <algorithm>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/point3_f.h"

// This macro provides the implementation for the observer notification methods.
#define NOTIFY_OBSERVERS(method_decl, observer_call)      \
  void DeviceDataManager::method_decl {                   \
    for (InputDeviceEventObserver& observer : observers_) \
      observer.observer_call;                             \
  }

namespace ui {

namespace {

bool InputDeviceEquals(const ui::InputDevice& a, const ui::InputDevice& b) {
  return a.id == b.id && a.enabled == b.enabled;
}

}  // namespace

// static
DeviceDataManager* DeviceDataManager::instance_ = nullptr;

DeviceDataManager::DeviceDataManager() {
  DCHECK(!instance_);
  instance_ = this;
}

DeviceDataManager::~DeviceDataManager() {
  instance_ = nullptr;
}

// static
void DeviceDataManager::CreateInstance() {
  if (instance_)
    return;

  new DeviceDataManager();

  // TODO(bruthig): Replace the DeleteInstance callbacks with explicit calls.
  base::AtExitManager::RegisterTask(base::BindOnce(DeleteInstance));
}

// static
void DeviceDataManager::DeleteInstance() {
  delete instance_;
}

// static
DeviceDataManager* DeviceDataManager::GetInstance() {
  CHECK(instance_) << "DeviceDataManager was not created.";
  return instance_;
}

// static
bool DeviceDataManager::HasInstance() {
  return instance_ != nullptr;
}

void DeviceDataManager::ConfigureTouchDevices(
    const std::vector<ui::TouchDeviceTransform>& transforms) {
  ClearTouchDeviceAssociations();
  for (const TouchDeviceTransform& transform : transforms)
    UpdateTouchInfoFromTransform(transform);
  are_touchscreen_target_displays_valid_ = true;
  for (InputDeviceEventObserver& observer : observers_)
    observer.OnTouchDeviceAssociationChanged();
}

void DeviceDataManager::ClearTouchDeviceAssociations() {
  touch_map_.clear();
  for (TouchscreenDevice& touchscreen_device : touchscreen_devices_)
    touchscreen_device.target_display_id = display::kInvalidDisplayId;
}

void DeviceDataManager::UpdateTouchInfoFromTransform(
    const ui::TouchDeviceTransform& touch_device_transform) {
  DCHECK_GE(touch_device_transform.device_id, 0);

  touch_map_[touch_device_transform.device_id] = touch_device_transform;
  for (TouchscreenDevice& touchscreen_device : touchscreen_devices_) {
    if (touchscreen_device.id == touch_device_transform.device_id) {
      touchscreen_device.target_display_id = touch_device_transform.display_id;
      return;
    }
  }
}

void DeviceDataManager::UpdateTouchMap() {
  // Remove all entries for devices from the |touch_map_| that are not currently
  // connected.
  auto last_iter = std::remove_if(
      touch_map_.begin(), touch_map_.end(),
      [this](const std::pair<int, TouchDeviceTransform>& map_entry) {
        // Check if the given |map_entry| is present in the current list of
        // connected devices.
        auto iter = std::find_if(
            touchscreen_devices_.begin(), touchscreen_devices_.end(),
            [&map_entry](const TouchscreenDevice& touch_device) {
              return touch_device.id == map_entry.second.device_id;
            });

        // Remove the device identified by |map_entry| from |touch_map_| if it
        // is not present in the list of currently connected devices.
        return iter != touchscreen_devices_.end();
      });
  touch_map_.erase(last_iter, touch_map_.end());
}

void DeviceDataManager::ApplyTouchRadiusScale(int touch_device_id,
                                              double* radius) {
  auto iter = touch_map_.find(touch_device_id);
  if (iter != touch_map_.end())
    *radius = (*radius) * iter->second.radius_scale;
}

void DeviceDataManager::ApplyTouchTransformer(int touch_device_id,
                                              float* x,
                                              float* y) {
  auto iter = touch_map_.find(touch_device_id);
  if (iter != touch_map_.end()) {
    gfx::Point3F point(*x, *y, 0.0);
    const gfx::Transform& trans = iter->second.transform;
    trans.TransformPoint(&point);
    *x = point.x();
    *y = point.y();
  }
}

const std::vector<TouchscreenDevice>& DeviceDataManager::GetTouchscreenDevices()
    const {
  return touchscreen_devices_;
}

const std::vector<InputDevice>& DeviceDataManager::GetKeyboardDevices() const {
  return keyboard_devices_;
}

const std::vector<InputDevice>& DeviceDataManager::GetMouseDevices() const {
  return mouse_devices_;
}

const std::vector<InputDevice>& DeviceDataManager::GetTouchpadDevices() const {
  return touchpad_devices_;
}

const std::vector<InputDevice>& DeviceDataManager::GetUncategorizedDevices()
    const {
  return uncategorized_devices_;
}

bool DeviceDataManager::AreDeviceListsComplete() const {
  return device_lists_complete_;
}

int64_t DeviceDataManager::GetTargetDisplayForTouchDevice(
    int touch_device_id) const {
  auto iter = touch_map_.find(touch_device_id);
  if (iter != touch_map_.end())
    return iter->second.display_id;
  return display::kInvalidDisplayId;
}

void DeviceDataManager::OnTouchscreenDevicesUpdated(
    const std::vector<TouchscreenDevice>& devices) {
  if (devices.size() == touchscreen_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 touchscreen_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  are_touchscreen_target_displays_valid_ = false;
  touchscreen_devices_ = devices;
  for (TouchscreenDevice& touchscreen_device : touchscreen_devices_) {
    touchscreen_device.target_display_id =
        GetTargetDisplayForTouchDevice(touchscreen_device.id);
  }
  UpdateTouchMap();
  NotifyObserversTouchscreenDeviceConfigurationChanged();
}

void DeviceDataManager::OnKeyboardDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == keyboard_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 keyboard_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  keyboard_devices_ = devices;
  NotifyObserversKeyboardDeviceConfigurationChanged();
}

void DeviceDataManager::OnMouseDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == mouse_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 mouse_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  mouse_devices_ = devices;
  NotifyObserversMouseDeviceConfigurationChanged();
}

void DeviceDataManager::OnTouchpadDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == touchpad_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 touchpad_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  touchpad_devices_ = devices;
  NotifyObserversTouchpadDeviceConfigurationChanged();
}

void DeviceDataManager::OnUncategorizedDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == uncategorized_devices_.size() &&
      std::equal(devices.begin(), devices.end(), uncategorized_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  uncategorized_devices_ = devices;
  NotifyObserversUncategorizedDeviceConfigurationChanged();
}

void DeviceDataManager::OnDeviceListsComplete() {
  if (!device_lists_complete_) {
    device_lists_complete_ = true;
    NotifyObserversDeviceListsComplete();
  }
}

void DeviceDataManager::OnStylusStateChanged(StylusState state) {
  NotifyObserversStylusStateChanged(state);
}

NOTIFY_OBSERVERS(
    NotifyObserversKeyboardDeviceConfigurationChanged(),
    OnInputDeviceConfigurationChanged(InputDeviceEventObserver::kKeyboard))

NOTIFY_OBSERVERS(
    NotifyObserversMouseDeviceConfigurationChanged(),
    OnInputDeviceConfigurationChanged(InputDeviceEventObserver::kMouse))

NOTIFY_OBSERVERS(
    NotifyObserversTouchpadDeviceConfigurationChanged(),
    OnInputDeviceConfigurationChanged(InputDeviceEventObserver::kTouchpad))

NOTIFY_OBSERVERS(
    NotifyObserversUncategorizedDeviceConfigurationChanged(),
    OnInputDeviceConfigurationChanged(InputDeviceEventObserver::kUncategorized))

NOTIFY_OBSERVERS(
    NotifyObserversTouchscreenDeviceConfigurationChanged(),
    OnInputDeviceConfigurationChanged(InputDeviceEventObserver::kTouchscreen))

NOTIFY_OBSERVERS(NotifyObserversDeviceListsComplete(), OnDeviceListsComplete())

NOTIFY_OBSERVERS(NotifyObserversStylusStateChanged(StylusState state),
                 OnStylusStateChanged(state))

void DeviceDataManager::AddObserver(InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void DeviceDataManager::RemoveObserver(InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceDataManager::SetTouchscreensEnabled(bool enabled) {
  touch_screens_enabled_ = enabled;
}

bool DeviceDataManager::AreTouchscreensEnabled() const {
  return touch_screens_enabled_;
}

bool DeviceDataManager::AreTouchscreenTargetDisplaysValid() const {
  return are_touchscreen_target_displays_valid_;
}

}  // namespace ui
