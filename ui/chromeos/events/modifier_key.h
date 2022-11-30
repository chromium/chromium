// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_MODIFIER_KEY_H_
#define UI_CHROMEOS_EVENTS_MODIFIER_KEY_H_

namespace ui {
namespace chromeos {

enum class ModifierKey : int {
  kSearchKey = 0,  // Customizable.
  kControlKey,     // Customizable.
  kAltKey,         // Customizable.
  kVoidKey,
  kCapsLockKey,   // Customizable.
  kEscapeKey,     // Customizable.
  kBackspaceKey,  // Customizable.
  kAssistantKey,  // Customizable,
  // IMPORTANT: Add new keys to the end, because the keys may have been stored
  // in user preferences.
  kNumModifierKeys,
};

}  // namespace chromeos
}  // namespace ui

#endif  // UI_CHROMEOS_EVENTS_MODIFIER_KEY_H_
