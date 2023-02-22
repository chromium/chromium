// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include "base/containers/contains.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/events/devices/device_data_manager.h"

namespace ui {

KeyboardCapability::KeyboardCapability(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

KeyboardCapability::~KeyboardCapability() = default;

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

}  // namespace ui
