// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
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

bool KeyboardCapability::IsTopRowKey(const KeyboardCode& key_code) const {
  // A set that includes all top row keys from different keyboards.
  // TODO(longbowei): Now only include top row keys from layout2, add more top
  // row keys from other keyboards in the future.
  static const base::NoDestructor<base::flat_set<KeyboardCode>>
      top_row_action_keys({
          VKEY_BROWSER_BACK,
          VKEY_BROWSER_REFRESH,
          VKEY_ZOOM,
          VKEY_MEDIA_LAUNCH_APP1,
          VKEY_BRIGHTNESS_DOWN,
          VKEY_BRIGHTNESS_UP,
          VKEY_MEDIA_PLAY_PAUSE,
          VKEY_VOLUME_MUTE,
          VKEY_VOLUME_DOWN,
          VKEY_VOLUME_UP,
      });
  return base::Contains(*top_row_action_keys, key_code);
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
