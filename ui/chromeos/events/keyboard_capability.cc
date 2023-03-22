// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <memory>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "device/udev_linux/scoped_udev.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

namespace {

bool GetDeviceProperty(const base::FilePath& device_path,
                       const char* key,
                       std::string* value) {
  device::ScopedUdevPtr udev(device::udev_new());
  if (!udev.get()) {
    return false;
  }

  device::ScopedUdevDevicePtr device(device::udev_device_new_from_syspath(
      udev.get(), device_path.value().c_str()));
  if (!device.get()) {
    return false;
  }

  *value = device::UdevDeviceGetPropertyValue(device.get(), key);
  return true;
}

}  // namespace

KeyboardCapability::KeyboardCapability(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DeviceDataManager::GetInstance()->AddObserver(this);
}

KeyboardCapability::~KeyboardCapability() {
  DeviceDataManager::GetInstance()->RemoveObserver(this);
}

KeyboardCapability::KeyboardInfo::KeyboardInfo() = default;
KeyboardCapability::KeyboardInfo::KeyboardInfo(KeyboardInfo&&) = default;
KeyboardCapability::KeyboardInfo& KeyboardCapability::KeyboardInfo::operator=(
    KeyboardInfo&&) = default;
KeyboardCapability::KeyboardInfo::~KeyboardInfo() = default;

// static
std::unique_ptr<EventDeviceInfo>
KeyboardCapability::CreateEventDeviceInfoFromInputDevice(
    const InputDevice& keyboard) {
  const char kDevNameProperty[] = "DEVNAME";
  std::string dev_name;
  if (!GetDeviceProperty(keyboard.sys_path, kDevNameProperty, &dev_name) ||
      dev_name.empty()) {
    return nullptr;
  }

  base::ScopedFD fd(open(dev_name.c_str(), O_RDONLY));
  if (fd.get() < 0) {
    LOG(ERROR) << "Cannot open " << dev_name.c_str() << " : " << errno;
    return nullptr;
  }

  std::unique_ptr<EventDeviceInfo> event_device_info =
      std::make_unique<EventDeviceInfo>();
  if (!event_device_info->Initialize(fd.get(), keyboard.sys_path)) {
    LOG(ERROR) << "Failed to get device information for "
               << keyboard.sys_path.value();
    return nullptr;
  }

  return event_device_info;
}

void KeyboardCapability::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void KeyboardCapability::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

bool KeyboardCapability::TopRowKeysAreFKeys() const {
  return delegate_->TopRowKeysAreFKeys();
}

void KeyboardCapability::SetTopRowKeysAsFKeysEnabledForTesting(
    bool enabled) const {
  delegate_->SetTopRowKeysAsFKeysEnabledForTesting(enabled);  // IN-TEST
}

// static
bool KeyboardCapability::IsSixPackKey(const KeyboardCode& key_code) {
  return base::Contains(kSixPackKeyToSystemKeyMap, key_code);
}

// static
bool KeyboardCapability::IsReversedSixPackKey(const KeyboardCode& key_code) {
  // [Back] maps back to both [Delete] and [Insert].
  return base::Contains(kReversedSixPackKeyToSystemKeyMap, key_code) ||
         key_code == ui::KeyboardCode::VKEY_BACK;
}

absl::optional<KeyboardCode> KeyboardCapability::GetMappedFKeyIfExists(
    const KeyboardCode& key_code,
    const InputDevice& keyboard) const {
  // TODO(zhangwenyu): Cache the layout for currently connected keyboards and
  // observe the keyboard changes.
  KeyboardTopRowLayout layout =
      EventRewriterChromeOS::GetKeyboardTopRowLayout(keyboard);
  switch (layout) {
    case KeyboardTopRowLayout::kKbdTopRowLayout1:
      if (kLayout1TopRowKeyToFKeyMap.contains(key_code)) {
        return kLayout1TopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayout2:
      if (kLayout2TopRowKeyToFKeyMap.contains(key_code)) {
        return kLayout2TopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
    case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
      if (kLayoutWilcoDrallionTopRowKeyToFKeyMap.contains(key_code)) {
        return kLayoutWilcoDrallionTopRowKeyToFKeyMap.at(key_code);
      }
      break;
    case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
      // TODO(zhangwenyu): Handle custom vivaldi layout.
      return absl::nullopt;
  }

  return absl::nullopt;
}

bool KeyboardCapability::HasLauncherButton(
    const absl::optional<InputDevice>& keyboard) {
  // Use current implementation. If keyboard is provided, launcher button
  // depends on if this keyboard is layout2 type. If keyboard is not provided,
  // launcher button depends on if any keyboard in DeviceDataManager is layout2
  // type.
  // TODO(zhangwenyu): Handle edge cases.
  if (!keyboard.has_value()) {
    // DeviceUsesKeyboardLayout2() relies on DeviceDataManager.
    DCHECK(DeviceDataManager::HasInstance());
    return DeviceUsesKeyboardLayout2();
  }

  return EventRewriterChromeOS::GetKeyboardTopRowLayout(keyboard.value()) ==
         KeyboardTopRowLayout::kKbdTopRowLayout2;
}

// static
bool KeyboardCapability::IsTopRowKey(const KeyboardCode& key_code) {
  // A set that includes all top row keys from different keyboards.
  static const base::NoDestructor<base::flat_set<KeyboardCode>>
      top_row_action_keys({
          KeyboardCode::VKEY_BROWSER_BACK,
          KeyboardCode::VKEY_BROWSER_FORWARD,
          KeyboardCode::VKEY_BROWSER_REFRESH,
          KeyboardCode::VKEY_ZOOM,
          KeyboardCode::VKEY_MEDIA_LAUNCH_APP1,
          KeyboardCode::VKEY_BRIGHTNESS_DOWN,
          KeyboardCode::VKEY_BRIGHTNESS_UP,
          KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
          KeyboardCode::VKEY_VOLUME_MUTE,
          KeyboardCode::VKEY_VOLUME_DOWN,
          KeyboardCode::VKEY_VOLUME_UP,
      });
  return base::Contains(*top_row_action_keys, key_code);
}

// static
bool KeyboardCapability::HasSixPackKey(const InputDevice& keyboard) {
  // If the keyboard is an internal keyboard, return false. Otherwise, return
  // true. This is correct for most of the keyboards. Edge cases will be handled
  // later.
  // TODO(zhangwenyu): handle edge cases when this logic doesn't apply.
  return keyboard.type != InputDeviceType::INPUT_DEVICE_INTERNAL;
}

// static
bool KeyboardCapability::HasSixPackOnAnyKeyboard() {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (ui::KeyboardCapability::HasSixPackKey(keyboard)) {
      return true;
    }
  }
  return false;
}

std::vector<mojom::ModifierKey> KeyboardCapability::GetModifierKeys(
    const InputDevice& keyboard) {
  // This set of modifier keys is available on every keyboard.
  std::vector<mojom::ModifierKey> modifier_keys = {
      mojom::ModifierKey::kBackspace, mojom::ModifierKey::kControl,
      mojom::ModifierKey::kMeta,      mojom::ModifierKey::kEscape,
      mojom::ModifierKey::kAlt,
  };

  const KeyboardInfo* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return modifier_keys;
  }

  // CapsLock exists on all non-chromeos keyboards.
  if (keyboard_info->device_type !=
          KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard &&
      keyboard_info->device_type !=
          KeyboardCapability::DeviceType::kDeviceInternalKeyboard) {
    modifier_keys.push_back(mojom::ModifierKey::kCapsLock);
  }

  // Assistant key can be checked by querying evdev properties.
  if (keyboard_info &&
      keyboard_info->event_device_info->HasKeyEvent(KEY_ASSISTANT)) {
    modifier_keys.push_back(mojom::ModifierKey::kAssistant);
  }

  return modifier_keys;
}

KeyboardCapability::DeviceType KeyboardCapability::GetDeviceType(
    const InputDevice& keyboard) {
  const auto* keyboard_info = GetKeyboardInfo(keyboard);
  if (!keyboard_info) {
    return DeviceType::kDeviceUnknown;
  }

  return keyboard_info->device_type;
}

void KeyboardCapability::SetKeyboardInfoForTesting(const InputDevice& keyboard,
                                                   KeyboardInfo keyboard_info) {
  keyboard_info_map_.insert_or_assign(keyboard.id, std::move(keyboard_info));
}

const KeyboardCapability::KeyboardInfo* KeyboardCapability::GetKeyboardInfo(
    const InputDevice& keyboard) {
  auto iter = keyboard_info_map_.find(keyboard.id);
  if (iter != keyboard_info_map_.end()) {
    return &iter->second;
  }

  // Insert new keyboard info into the map.
  auto& keyboard_info = keyboard_info_map_[keyboard.id];
  keyboard_info.device_type = EventRewriterChromeOS::GetDeviceType(keyboard);
  keyboard_info.event_device_info =
      CreateEventDeviceInfoFromInputDevice(keyboard);
  if (!keyboard_info.event_device_info) {
    keyboard_info_map_.erase(keyboard.id);
    return nullptr;
  }

  return &keyboard_info;
}

void KeyboardCapability::OnDeviceListsComplete() {
  TrimKeyboardInfoMap();
}

void KeyboardCapability::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & InputDeviceEventObserver::kKeyboard) {
    TrimKeyboardInfoMap();
  }
}

void KeyboardCapability::TrimKeyboardInfoMap() {
  auto sorted_keyboards =
      DeviceDataManager::GetInstance()->GetKeyboardDevices();
  base::ranges::sort(sorted_keyboards, [](const ui::InputDevice& device1,
                                          const ui::InputDevice& device2) {
    return device1.id < device2.id;
  });

  // Generate a vector with only the device ids from the
  // `keyboard_info_map_` map. Guaranteed to be sorted as flat_map is always
  // in sorted order by key.
  std::vector<int> cached_event_device_info_ids;
  cached_event_device_info_ids.reserve(keyboard_info_map_.size());
  base::ranges::transform(keyboard_info_map_,
                          std::back_inserter(cached_event_device_info_ids),
                          [](const auto& pair) { return pair.first; });
  DCHECK(base::ranges::is_sorted(cached_event_device_info_ids));

  // Compares the `cached_event_device_info_ids` to the id field of
  // `sorted_keyboards`. Ids that are in `cached_event_device_info_ids` but not
  // in `sorted_keyboards` are inserted into `keyboard_ids_to_remove`.
  // `sorted_keyboards` and `cached_event_device_info_ids` must be sorted.
  std::vector<int> keyboard_ids_to_remove;
  base::ranges::set_difference(
      cached_event_device_info_ids, sorted_keyboards,
      std::back_inserter(keyboard_ids_to_remove),
      /*Comp=*/base::ranges::less(),
      /*Proj1=*/base::identity(),
      /*Proj2=*/[](const InputDevice& device) { return device.id; });

  for (const auto& id : keyboard_ids_to_remove) {
    keyboard_info_map_.erase(id);
  }
}

bool KeyboardCapability::HasKeyEvent(const KeyboardCode& key_code,
                                     const InputDevice& keyboard) const {
  // Handle top row keys.
  if (IsTopRowKey(key_code)) {
    KeyboardTopRowLayout layout =
        EventRewriterChromeOS::GetKeyboardTopRowLayout(keyboard);
    switch (layout) {
      case KeyboardTopRowLayout::kKbdTopRowLayout1:
        return kLayout1TopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayout2:
        return kLayout2TopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayoutWilco:
      case KeyboardTopRowLayout::kKbdTopRowLayoutDrallion:
        return kLayoutWilcoDrallionTopRowKeyToFKeyMap.contains(key_code);
      case KeyboardTopRowLayout::kKbdTopRowLayoutCustom:
        // TODO(zhangwenyu): Handle custom vivaldi layout.
        return true;
    }
  }

  // Handle six pack keys.
  if (IsSixPackKey(key_code)) {
    return HasSixPackKey(keyboard);
  }

  // TODO(zhangwenyu): check other specific keys, e.g. assistant key.
  return true;
}

bool KeyboardCapability::HasKeyEventOnAnyKeyboard(
    const KeyboardCode& key_code) const {
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (HasKeyEvent(key_code, keyboard)) {
      return true;
    }
  }
  return false;
}

}  // namespace ui
